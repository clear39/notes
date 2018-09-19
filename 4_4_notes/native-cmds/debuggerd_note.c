//	system/core/debuggerd/debuggerd.c
//入口
int main(int argc, char** argv) {
    if (argc == 1) {
        return do_server();
    }

    bool dump_backtrace = false;
    bool have_tid = false;
    pid_t tid = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b")) {
            dump_backtrace = true;
        } else if (!have_tid) {
            tid = atoi(argv[i]);
            have_tid = true;
        } else {
            usage();
            return 1;
        }
    }
    if (!have_tid) {
        usage();
        return 1;
    }
    return do_explicit_dump(tid, dump_backtrace);
}

///////////////////////////////////////////////////////////////////////////
//服务启动
static int do_server() {
    int s;
    struct sigaction act;
    int logsocket = -1;

    /*
     * debuggerd crashes can't be reported to debuggerd.  Reset all of the
     * crash handlers.
     */
    signal(SIGILL, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
#ifdef SIGSTKFLT
    signal(SIGSTKFLT, SIG_DFL);
#endif

    // Ignore failed writes to closed sockets
    signal(SIGPIPE, SIG_IGN);

    logsocket = socket_local_client("logd",ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_DGRAM);
    if(logsocket < 0) {
        logsocket = -1;
    } else {
        fcntl(logsocket, F_SETFD, FD_CLOEXEC);
    }

    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask,SIGCHLD);
    act.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &act, 0);

    //	system/core/include/cutils/debugger.h:26:#define DEBUGGER_SOCKET_NAME "android:debuggerd"
    s = socket_local_server(DEBUGGER_SOCKET_NAME,ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if(s < 0) return 1;
    fcntl(s, F_SETFD, FD_CLOEXEC);

    LOG("debuggerd: " __DATE__ " " __TIME__ "\n");

    for(;;) {
        struct sockaddr addr;
        socklen_t alen;
        int fd;

        alen = sizeof(addr);
        XLOG("waiting for connection\n");
        fd = accept(s, &addr, &alen);
        if(fd < 0) {
            XLOG("accept failed: %s\n", strerror(errno));
            continue;
        }

        fcntl(fd, F_SETFD, FD_CLOEXEC);

        handle_request(fd);
    }
    return 0;
}


static void handle_request(int fd) {
    XLOG("handle_request(%d)\n", fd);

    debugger_request_t request;
    memset(&request, 0, sizeof(request));
    int status = read_request(fd, &request);//分析
    if (!status) {
        XLOG("BOOM: pid=%d uid=%d gid=%d tid=%d\n", request.pid, request.uid, request.gid, request.tid);

        /* At this point, the thread that made the request is blocked in
         * a read() call.  If the thread has crashed, then this gives us
         * time to PTRACE_ATTACH to it before it has a chance to really fault.
         *
         * The PTRACE_ATTACH sends a SIGSTOP to the target process, but it
         * won't necessarily have stopped by the time ptrace() returns.  (We
         * currently assume it does.)  We write to the file descriptor to
         * ensure that it can run as soon as we call PTRACE_CONT below.
         * See details in bionic/libc/linker/debugger.c, in function
         * debugger_signal_handler().
         */
        if (ptrace(PTRACE_ATTACH, request.tid, 0, 0)) {
            LOG("ptrace attach failed: %s\n", strerror(errno));
        } else {
            bool detach_failed = false;

	    //检查类型是否为DEBUGGER_ACTION_CRASH，
	    //如果是，获取属性debug.db.uid的值，判断当前请求的uid小于等于debug_uid
            bool attach_gdb = should_attach_gdb(&request);//分析
            if (TEMP_FAILURE_RETRY(write(fd, "\0", 1)) != 1) {
		//写入失败报错
                LOG("failed responding to client: %s\n", strerror(errno));
            } else {
                char* tombstone_path = NULL;

                if (request.action == DEBUGGER_ACTION_CRASH) {
		    //关闭客户端连接的socket
                    close(fd);
                    fd = -1;
                }

                int total_sleep_time_usec = 0;
                for (;;) {
                    int signal = wait_for_signal(request.tid, &total_sleep_time_usec);
                    if (signal < 0) {
                        break;
                    }

                    switch (signal) {
                    case SIGSTOP:
                        if (request.action == DEBUGGER_ACTION_DUMP_TOMBSTONE) {
                            XLOG("stopped -- dumping to tombstone\n");
                            tombstone_path = engrave_tombstone(request.pid, request.tid,signal, request.abort_msg_address, true, true, &detach_failed,&total_sleep_time_usec);
                        } else if (request.action == DEBUGGER_ACTION_DUMP_BACKTRACE) {
                            XLOG("stopped -- dumping to fd\n");
                            dump_backtrace(fd, -1,request.pid, request.tid, &detach_failed,&total_sleep_time_usec);
                        } else {
                            XLOG("stopped -- continuing\n");
                            status = ptrace(PTRACE_CONT, request.tid, 0, 0);
                            if (status) {
                                LOG("ptrace continue failed: %s\n", strerror(errno));
                            }
                            continue; /* loop again */
                        }
                        break;

                    case SIGILL:
                    case SIGABRT:
                    case SIGBUS:
                    case SIGFPE:
                    case SIGSEGV:
                    case SIGPIPE:
#ifdef SIGSTKFLT
                    case SIGSTKFLT:
#endif
                        {
                        XLOG("stopped -- fatal signal\n");
                        /*
                         * Send a SIGSTOP to the process to make all of
                         * the non-signaled threads stop moving.  Without
                         * this we get a lot of "ptrace detach failed:
                         * No such process".
                         */
                        kill(request.pid, SIGSTOP);
                        /* don't dump sibling threads when attaching to GDB because it
                         * makes the process less reliable, apparently... */
                        tombstone_path = engrave_tombstone(request.pid, request.tid,signal, request.abort_msg_address, !attach_gdb, false,&detach_failed, &total_sleep_time_usec);
                        break;
                    }

                    default:
                        XLOG("stopped -- unexpected signal\n");
                        LOG("process stopped due to unexpected signal %d\n", signal);
                        break;
                    }
                    break;
                }

                if (request.action == DEBUGGER_ACTION_DUMP_TOMBSTONE) {
		    //把TOMBSTONE日志保存的路径回传给客户端
                    if (tombstone_path) {
                        write(fd, tombstone_path, strlen(tombstone_path));
                    }
                    close(fd);
                    fd = -1;
                }
                free(tombstone_path);
            }

            XLOG("detaching\n");
            if (attach_gdb) {
                /* stop the process so we can debug */
                kill(request.pid, SIGSTOP);

                /* detach so we can attach gdbserver */
                if (ptrace(PTRACE_DETACH, request.tid, 0, 0)) {//解除对子进程的追踪
                    LOG("ptrace detach from %d failed: %s\n", request.tid, strerror(errno));
                    detach_failed = true;
                }

                /*
                 * if debug.db.uid is set, its value indicates if we should wait
                 * for user action for the crashing process.
                 * in this case, we log a message and turn the debug LED on
                 * waiting for a gdb connection (for instance)
                 */
                wait_for_user_action(request.pid);
            } else {
                /* just detach */
                if (ptrace(PTRACE_DETACH, request.tid, 0, 0)) {//解除对子进程的追踪
                    LOG("ptrace detach from %d failed: %s\n", request.tid, strerror(errno));
                    detach_failed = true;
                }
            }

            /* resume stopped process (so it can crash in peace). */
            kill(request.pid, SIGCONT);//恢复被停止的子进程，并让其自然终止

            /* If we didn't successfully detach, we're still the parent, and the
             * actual parent won't receive a death notification via wait(2).  At this point
             * there's not much we can do about that. */
            if (detach_failed) {
                LOG("debuggerd committing suicide to free the zombie!\n");
                kill(getpid(), SIGKILL);
            }
        }

    }
    if (fd >= 0) {
        close(fd);
    }
}


typedef enum {
    // dump a crash
    DEBUGGER_ACTION_CRASH,
    // dump a tombstone file
    DEBUGGER_ACTION_DUMP_TOMBSTONE,
    // dump a backtrace only back to the socket
    DEBUGGER_ACTION_DUMP_BACKTRACE,
} debugger_action_t;

typedef struct {
    debugger_action_t action;
    pid_t pid, tid;
    uid_t uid, gid;
    uintptr_t abort_msg_address;
} debugger_request_t;




static int read_request(int fd, debugger_request_t* out_request) {
    struct ucred cr;
    int len = sizeof(cr);
    int status = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &len);
    if (status != 0) {
        LOG("cannot get credentials\n");
        return -1;
    }

    XLOG("reading tid\n");
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct pollfd pollfds[1];
    pollfds[0].fd = fd;
    pollfds[0].events = POLLIN;
    pollfds[0].revents = 0;
    status = TEMP_FAILURE_RETRY(poll(pollfds, 1, 3000));
    if (status != 1) {
        LOG("timed out reading tid (from pid=%d uid=%d)\n", cr.pid, cr.uid);
        return -1;
    }

    debugger_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    status = TEMP_FAILURE_RETRY(read(fd, &msg, sizeof(msg)));
    if (status < 0) {
        LOG("read failure? %s (pid=%d uid=%d)\n",
            strerror(errno), cr.pid, cr.uid);
        return -1;
    }
    if (status == sizeof(debugger_msg_t)) {
        XLOG("crash request of size %d abort_msg_address=%#08x\n", status, msg.abort_msg_address);
    } else {
        LOG("invalid crash request of size %d (from pid=%d uid=%d)\n",
            status, cr.pid, cr.uid);
        return -1;
    }

    out_request->action = msg.action;
    out_request->tid = msg.tid;
    out_request->pid = cr.pid;
    out_request->uid = cr.uid;
    out_request->gid = cr.gid;
    out_request->abort_msg_address = msg.abort_msg_address;

    if (msg.action == DEBUGGER_ACTION_CRASH) {
        /* Ensure that the tid reported by the crashing process is valid. */
        char buf[64];
        struct stat s;
        snprintf(buf, sizeof buf, "/proc/%d/task/%d", out_request->pid, out_request->tid);
        if(stat(buf, &s)) {
            LOG("tid %d does not exist in pid %d. ignoring debug request\n",out_request->tid, out_request->pid);
            return -1;
        }
    } else if (cr.uid == 0 || (cr.uid == AID_SYSTEM && msg.action == DEBUGGER_ACTION_DUMP_BACKTRACE)) {
        /* Only root or system can ask us to attach to any process and dump it explicitly.
         * However, system is only allowed to collect backtraces but cannot dump tombstones. */
        status = get_process_info(out_request->tid, &out_request->pid,&out_request->uid, &out_request->gid);
        if (status < 0) {
            LOG("tid %d does not exist. ignoring explicit dump request\n",out_request->tid);
            return -1;
        }
    } else {
        /* No one else is allowed to dump arbitrary processes. */
        return -1;
    }
    return 0;
}

//检查类型是否为DEBUGGER_ACTION_CRASH，
//如果是，获取属性debug.db.uid的值，判断当前请求的uid小于等于debug_uid
static bool should_attach_gdb(debugger_request_t* request) {
    if (request->action == DEBUGGER_ACTION_CRASH) {
        char value[PROPERTY_VALUE_MAX];
        property_get("debug.db.uid", value, "-1");
        int debug_uid = atoi(value);
        return debug_uid >= 0 && request->uid <= (uid_t)debug_uid;
    }
    return false;
}


//	system/core/debuggerd/utility.c
int wait_for_signal(pid_t tid, int* total_sleep_time_usec) {
    for (;;) {
        int status;
        pid_t n = waitpid(tid, &status, __WALL | WNOHANG);
        if (n < 0) {
            if(errno == EAGAIN) continue;
            LOG("waitpid failed: %s\n", strerror(errno));
            return -1;
        } else if (n > 0) {
            XLOG("waitpid: n=%d status=%08x\n", n, status);
            if (WIFSTOPPED(status)) {
                return WSTOPSIG(status);
            } else {
                LOG("unexpected waitpid response: n=%d, status=%08x\n", n, status);
                return -1;
            }
        }

        if (*total_sleep_time_usec > max_total_sleep_usec) {
            LOG("timed out waiting for tid=%d to die\n", tid);
            return -1;
        }

        /* not ready yet */
        XLOG("not ready yet\n");
        usleep(sleep_time_usec);
        *total_sleep_time_usec += sleep_time_usec;
    }
}



static int get_process_info(pid_t tid, pid_t* out_pid, uid_t* out_uid, uid_t* out_gid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", tid);

    FILE* fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    int fields = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 6 && !memcmp(line, "Tgid:\t", 6)) {
            *out_pid = atoi(line + 6);
            fields |= 1;
        } else if (len > 5 && !memcmp(line, "Uid:\t", 5)) {
            *out_uid = atoi(line + 5);
            fields |= 2;
        } else if (len > 5 && !memcmp(line, "Gid:\t", 5)) {
            *out_gid = atoi(line + 5);
            fields |= 4;
        }
    }
    fclose(fp);
    return fields == 7 ? 0 : -1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* engrave_tombstone(pid_t pid, pid_t tid, int signal, uintptr_t abort_msg_address,bool dump_sibling_threads, bool quiet, bool* detach_failed,int* total_sleep_time_usec) {
    mkdir(TOMBSTONE_DIR, 0755);
    chown(TOMBSTONE_DIR, AID_SYSTEM, AID_SYSTEM);

    if (selinux_android_restorecon(TOMBSTONE_DIR) == -1) {
        *detach_failed = false;
        return NULL;
    }

    int fd;
    char* path = find_and_open_tombstone(&fd);//查找tombstone文件，并打开
    if (!path) {
        *detach_failed = false;
        return NULL;
    }

    log_t log;
    log.tfd = fd;
    log.amfd = activity_manager_connect();
    log.quiet = quiet;
    *detach_failed = dump_crash(&log, pid, tid, signal, abort_msg_address, dump_sibling_threads,total_sleep_time_usec);

    close(log.amfd);
    close(fd);
    return path;
}


//查找tombstone文件，并打开
static char* find_and_open_tombstone(int* fd)
{
    unsigned long mtime = ULONG_MAX;
    struct stat sb;

    /*
     * XXX: Our stat.st_mtime isn't time_t. If it changes, as it probably ought
     * to, our logic breaks. This check will generate a warning if that happens.
     */
    typecheck(mtime, sb.st_mtime);

    /*
     * In a single wolf-like pass, find an available slot and, in case none
     * exist, find and record the least-recently-modified file.
     */
    char path[128];
    int oldest = 0;
    for (int i = 0; i < MAX_TOMBSTONES; i++) {
        snprintf(path, sizeof(path), TOMBSTONE_DIR"/tombstone_%02d", i);

        if (!stat(path, &sb)) {
            if (sb.st_mtime < mtime) {
                oldest = i;
                mtime = sb.st_mtime;
            }
            continue;
        }
        if (errno != ENOENT)
            continue;

        *fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (*fd < 0)
            continue;   /* raced ? */

        fchown(*fd, AID_SYSTEM, AID_SYSTEM);
        return strdup(path);
    }

    /* we didn't find an available file, so we clobber the oldest one */
    snprintf(path, sizeof(path), TOMBSTONE_DIR"/tombstone_%02d", oldest);
    *fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (*fd < 0) {
        LOG("failed to open tombstone file '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    fchown(*fd, AID_SYSTEM, AID_SYSTEM);
    return strdup(path);
}



static int activity_manager_connect() {
    int amfd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (amfd >= 0) {
        struct sockaddr_un address;
        int err;

        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strncpy(address.sun_path, NCRASH_SOCKET_PATH, sizeof(address.sun_path));
        err = TEMP_FAILURE_RETRY( connect(amfd, (struct sockaddr*) &address, sizeof(address)) );
        if (!err) {
            struct timeval tv;
            memset(&tv, 0, sizeof(tv));
            tv.tv_sec = 1;  // tight leash
            err = setsockopt(amfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            if (!err) {
                tv.tv_sec = 3;  // 3 seconds on handshake read
                err = setsockopt(amfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            }
        }
        if (err) {
            close(amfd);
            amfd = -1;
        }
    }

    return amfd;
}


/*
 * Dumps all information about the specified pid to the tombstone.
 */
static bool dump_crash(log_t* log, pid_t pid, pid_t tid, int signal, uintptr_t abort_msg_address,bool dump_sibling_threads, int* total_sleep_time_usec)
{
    /* don't copy log messages to tombstone unless this is a dev device */
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.debuggable", value, "0");
    bool want_logs = (value[0] == '1');

    if (log->amfd >= 0) {
        /*
         * Activity Manager protocol: binary 32-bit network-byte-order ints for the
         * pid and signal number, followed by the raw text of the dump, culminating
         * in a zero byte that marks end-of-data.
         */
        uint32_t datum = htonl(pid);
        TEMP_FAILURE_RETRY( write(log->amfd, &datum, 4) );
        datum = htonl(signal);
        TEMP_FAILURE_RETRY( write(log->amfd, &datum, 4) );
    }

    _LOG(log, SCOPE_AT_FAULT, "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
    dump_build_info(log);
    dump_revision_info(log);
    dump_thread_info(log, pid, tid, true);
    if (signal) {
        dump_fault_addr(log, tid, signal);
    }
    dump_abort_message(log, tid, abort_msg_address);

    ptrace_context_t* context = load_ptrace_context(tid);
    dump_thread(context, log, tid, true, total_sleep_time_usec);

    if (want_logs) {
        dump_logs(log, pid, true);
    }

    bool detach_failed = false;
    if (dump_sibling_threads) {
        detach_failed = dump_sibling_thread_report(context, log, pid, tid, total_sleep_time_usec);
    }

    free_ptrace_context(context);

    if (want_logs) {
        dump_logs(log, pid, false);
    }

    /* send EOD to the Activity Manager, then wait for its ack to avoid racing ahead
     * and killing the target out from under it */
    if (log->amfd >= 0) {
        uint8_t eodMarker = 0;
        TEMP_FAILURE_RETRY( write(log->amfd, &eodMarker, 1) );
        /* 3 sec timeout reading the ack; we're fine if that happens */
        TEMP_FAILURE_RETRY( read(log->amfd, &eodMarker, 1) );
    }

    return detach_failed;
}


static void dump_build_info(log_t* log)
{
    char fingerprint[PROPERTY_VALUE_MAX];

    property_get("ro.build.fingerprint", fingerprint, "unknown");

    _LOG(log, SCOPE_AT_FAULT, "Build fingerprint: '%s'\n", fingerprint);
}


static void dump_revision_info(log_t* log)
{
    char revision[PROPERTY_VALUE_MAX];

    property_get("ro.revision", revision, "unknown");

    _LOG(log, SCOPE_AT_FAULT, "Revision: '%s'\n", revision);
}


static void dump_thread_info(log_t* log, pid_t pid, pid_t tid, bool at_fault = true) {
    char path[64];
    char threadnamebuf[1024];
    char* threadname = NULL;
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/comm", tid);
    if ((fp = fopen(path, "r"))) {
        threadname = fgets(threadnamebuf, sizeof(threadnamebuf), fp);//获取线程名
        fclose(fp);
        if (threadname) {
            size_t len = strlen(threadname);
            if (len && threadname[len - 1] == '\n') {
                threadname[len - 1] = '\0';
            }
        }
    }

    if (at_fault) {
        char procnamebuf[1024];
        char* procname = NULL;

        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        if ((fp = fopen(path, "r"))) {
            procname = fgets(procnamebuf, sizeof(procnamebuf), fp);//获取进城名
            fclose(fp);
        }

        _LOG(log, SCOPE_AT_FAULT, "pid: %d, tid: %d, name: %s  >>> %s <<<\n", pid, tid,threadname ? threadname : "UNKNOWN",procname ? procname : "UNKNOWN");
    } else {
        _LOG(log, 0, "pid: %d, tid: %d, name: %s\n",pid, tid, threadname ? threadname : "UNKNOWN");
    }
}



static void dump_fault_addr(log_t* log, pid_t tid, int sig)
{
    siginfo_t si;

    memset(&si, 0, sizeof(si));
    if(ptrace(PTRACE_GETSIGINFO, tid, 0, &si)){
        _LOG(log, SCOPE_AT_FAULT, "cannot get siginfo: %s\n", strerror(errno));
    } else if (signal_has_address(sig)) {
        _LOG(log, SCOPE_AT_FAULT, "signal %d (%s), code %d (%s), fault addr %08x\n",sig, get_signame(sig),si.si_code, get_sigcode(sig, si.si_code),(uintptr_t) si.si_addr);
    } else {
        _LOG(log, SCOPE_AT_FAULT, "signal %d (%s), code %d (%s), fault addr --------\n",sig, get_signame(sig), si.si_code, get_sigcode(sig, si.si_code));
    }
}


static void dump_abort_message(log_t* log, pid_t tid, uintptr_t address) {
  if (address == 0) {
    return;
  }

  address += sizeof(size_t); // Skip the buffer length.

  char msg[512];
  memset(msg, 0, sizeof(msg));
  char* p = &msg[0];
  while (p < &msg[sizeof(msg)]) {
    uint32_t data;
    if (!try_get_word_ptrace(tid, address, &data)) {
      break;
    }
    address += sizeof(uint32_t);

    if ((*p++ = (data >>  0) & 0xff) == 0) {
      break;
    }
    if ((*p++ = (data >>  8) & 0xff) == 0) {
      break;
    }
    if ((*p++ = (data >> 16) & 0xff) == 0) {
      break;
    }
    if ((*p++ = (data >> 24) & 0xff) == 0) {
      break;
    }
  }
  msg[sizeof(msg) - 1] = '\0';

  _LOG(log, SCOPE_AT_FAULT, "Abort message: '%s'\n", msg);
}


//	system/core/libcorkscrew/ptrace.c:82
bool try_get_word_ptrace(pid_t tid, uintptr_t ptr, uint32_t* out_value) {
    memory_t memory;
    init_memory_ptrace(&memory, tid);
    return try_get_word(&memory, ptr, out_value);
}

void init_memory_ptrace(memory_t* memory, pid_t tid) {
    memory->tid = tid;
    memory->map_info_list = NULL;
}


bool try_get_word(const memory_t* memory, uintptr_t ptr, uint32_t* out_value) {
    ALOGV("try_get_word: reading word at %p", (void*) ptr);
    if (ptr & 3) {
        ALOGV("try_get_word: invalid pointer %p", (void*) ptr);
        *out_value = 0xffffffffL;
        return false;
    }
    if (memory->tid < 0) {
        if (!is_readable_map(memory->map_info_list, ptr)) {
            ALOGV("try_get_word: pointer %p not in a readable map", (void*) ptr);
            *out_value = 0xffffffffL;
            return false;
        }
        *out_value = *(uint32_t*)ptr;
        return true;
    } else {
#if defined(__APPLE__)
        ALOGV("no ptrace on Mac OS");
        return false;
#else
        // ptrace() returns -1 and sets errno when the operation fails.
        // To disambiguate -1 from a valid result, we clear errno beforehand.
        errno = 0;
        *out_value = ptrace(PTRACE_PEEKTEXT, memory->tid, (void*)ptr, NULL);
        if (*out_value == 0xffffffffL && errno) {
            ALOGV("try_get_word: invalid pointer 0x%08x reading from tid %d, " "ptrace() errno=%d", ptr, memory->tid, errno);
            return false;
        }
        return true;
#endif
    }
}

typedef struct map_info {
    struct map_info* next;
    uintptr_t start;
    uintptr_t end;
    bool is_readable;
    bool is_writable;
    bool is_executable;
    void* data; // arbitrary data associated with the map by the user, initially NULL
    char name[];
} map_info_t;

/* Stores information about a process that is used for several different
 * ptrace() based operations. */
typedef struct {
    map_info_t* map_info_list;
} ptrace_context_t;

/* Describes how to access memory from a process. */
typedef struct {
    pid_t tid;
    const map_info_t* map_info_list;
} memory_t;



//	system/core/libcorkscrew/ptrace.c:106
ptrace_context_t* load_ptrace_context(pid_t pid) {
    ptrace_context_t* context = (ptrace_context_t*)calloc(1, sizeof(ptrace_context_t));
    if (context) {
        context->map_info_list = load_map_info_list(pid);
        for (map_info_t* mi = context->map_info_list; mi; mi = mi->next) {
            load_ptrace_map_info_data(pid, mi);
        }
    }
    return context;
}

//	system/core/libcorkscrew/map_info.c
map_info_t* load_map_info_list(pid_t tid) {
    char path[PATH_MAX];
    char line[1024];
    FILE* fp;
    map_info_t* milist = NULL;

    snprintf(path, PATH_MAX, "/proc/%d/maps", tid);
    fp = fopen(path, "r");
    if (fp) {
        while(fgets(line, sizeof(line), fp)) {
            map_info_t* mi = parse_maps_line(line);
            if (mi) {
                mi->next = milist;
                milist = mi;
            }
        }
        fclose(fp);
    }
    return milist;
}

static map_info_t* parse_maps_line(const char* line)
{
    unsigned long int start;
    unsigned long int end;
    char permissions[5];
    int name_pos;
    if (sscanf(line, "%lx-%lx %4s %*x %*x:%*x %*d%n", &start, &end,permissions, &name_pos) != 3) {
        return NULL;
    }

    while (isspace(line[name_pos])) {
        name_pos += 1;
    }
    const char* name = line + name_pos;
    size_t name_len = strlen(name);
    if (name_len && name[name_len - 1] == '\n') {
        name_len -= 1;
    }

    map_info_t* mi = calloc(1, sizeof(map_info_t) + name_len + 1);
    if (mi) {
        mi->start = start;
        mi->end = end;
        mi->is_readable = strlen(permissions) == 4 && permissions[0] == 'r';
        mi->is_writable = strlen(permissions) == 4 && permissions[1] == 'w';
        mi->is_executable = strlen(permissions) == 4 && permissions[2] == 'x';
        mi->data = NULL;
        memcpy(mi->name, name, name_len);
        mi->name[name_len] = '\0';
        ALOGV("Parsed map: start=0x%08x, end=0x%08x, "
              "is_readable=%d, is_writable=%d, is_executable=%d, name=%s",
              mi->start, mi->end,mi->is_readable, mi->is_writable, mi->is_executable, mi->name);
    }
    return mi;
}


static void load_ptrace_map_info_data(pid_t pid, map_info_t* mi) {
    if (mi->is_executable && mi->is_readable) {
        uint32_t elf_magic;
        if (try_get_word_ptrace(pid, mi->start, &elf_magic) && elf_magic == ELF_MAGIC) {
            map_info_data_t* data = (map_info_data_t*)calloc(1, sizeof(map_info_data_t));
            if (data) {
                mi->data = data;
                if (mi->name[0]) {
                    data->symbol_table = load_symbol_table(mi->name);
                }
#ifdef CORKSCREW_HAVE_ARCH
                load_ptrace_map_info_data_arch(pid, mi, data);
#endif
            }
        }
    }
}


















































//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//客户端启动
static void usage() {
    fputs("Usage: -b [<tid>]\n"
            "  -b dump backtrace to console, otherwise dump full tombstone file\n"
            "\n"
            "If tid specified, sends a request to debuggerd to dump that task.\n"
            "Otherwise, starts the debuggerd server.\n", stderr);
}


static int do_explicit_dump(pid_t tid, bool dump_backtrace) {
    fprintf(stdout, "Sending request to dump task %d.\n", tid);

    if (dump_backtrace) {
        fflush(stdout);
        if (dump_backtrace_to_file(tid, fileno(stdout)) < 0) {
            fputs("Error dumping backtrace.\n", stderr);
            return 1;
        }
    } else {
        char tombstone_path[PATH_MAX];
	//
        if (dump_tombstone(tid, tombstone_path, sizeof(tombstone_path)) < 0) {
            fputs("Error dumping tombstone.\n", stderr);
            return 1;
        }
        fprintf(stderr, "Tombstone written to: %s\n", tombstone_path);
    }
    return 0;
}


//	system/core/libcutils/debugger.c
int dump_backtrace_to_file(pid_t tid, int fd) {
    //	system/core/include/cutils/debugger.h:26:#define DEBUGGER_SOCKET_NAME "android:debuggerd"
    int s = socket_local_client(DEBUGGER_SOCKET_NAME,ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (s < 0) {
        return -1;
    }

    debugger_msg_t msg;
    msg.tid = tid;
    msg.action = DEBUGGER_ACTION_DUMP_BACKTRACE;

    int result = 0;
    if (TEMP_FAILURE_RETRY(write(s, &msg, sizeof(msg))) != sizeof(msg)) {
        result = -1;
    } else {
        char ack;
	//读取ack值是否为1
        if (TEMP_FAILURE_RETRY(read(s, &ack, 1)) != 1) {
            result = -1;
        } else {
            char buffer[4096];
            ssize_t n;
	    // 读取写入对应文件描述符中
            while ((n = TEMP_FAILURE_RETRY(read(s, buffer, sizeof(buffer)))) > 0) {
                if (TEMP_FAILURE_RETRY(write(fd, buffer, n)) != n) {
                    result = -1;
                    break;
                }
            }
        }
    }
    TEMP_FAILURE_RETRY(close(s));
    return result;
}



int dump_tombstone(pid_t tid, char* pathbuf, size_t pathlen) {

    //	system/core/include/cutils/debugger.h:26:#define DEBUGGER_SOCKET_NAME "android:debuggerd"
    int s = socket_local_client(DEBUGGER_SOCKET_NAME,ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (s < 0) {
        return -1;
    }

    debugger_msg_t msg;
    msg.tid = tid;
    msg.action = DEBUGGER_ACTION_DUMP_TOMBSTONE;

    int result = 0;
    if (TEMP_FAILURE_RETRY(write(s, &msg, sizeof(msg))) != sizeof(msg)) {
        result = -1;
    } else {
        char ack;
	//读取ack值是否为1
        if (TEMP_FAILURE_RETRY(read(s, &ack, 1)) != 1) {
            result = -1;
        } else {
            if (pathbuf && pathlen) {
                ssize_t n = TEMP_FAILURE_RETRY(read(s, pathbuf, pathlen - 1));
                if (n <= 0) {
                    result = -1;
                } else {
                    pathbuf[n] = '\0';
                }
            }
        }
    }
    TEMP_FAILURE_RETRY(close(s));
    return result;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////
void dump_backtrace(int fd, int amfd, pid_t pid, pid_t tid, bool* detach_failed,int* total_sleep_time_usec) {
    log_t log;
    log.tfd = fd;
    log.amfd = amfd;
    log.quiet = true;

    ptrace_context_t* context = load_ptrace_context(tid);
    dump_process_header(&log, pid);
    dump_thread(&log, tid, context, true, detach_failed, total_sleep_time_usec);

    char task_path[64];
    snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid);
    DIR* d = opendir(task_path);
    if (d != NULL) {
        struct dirent* de = NULL;
        while ((de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
                continue;
            }

            char* end;
            pid_t new_tid = strtoul(de->d_name, &end, 10);
            if (*end || new_tid == tid) {
                continue;
            }

            dump_thread(&log, new_tid, context, false, detach_failed, total_sleep_time_usec);
        }
        closedir(d);
    }

    dump_process_footer(&log, pid);
    free_ptrace_context(context);
}







