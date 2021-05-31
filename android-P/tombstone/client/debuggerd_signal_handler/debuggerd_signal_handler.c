

// Handler that does crash dumping by forking and doing the processing in the child.
// Do this by ptracing the relevant thread, and then execing debuggerd to do the actual dump.
static void debuggerd_signal_handler(int signal_number, siginfo_t* info, void* context) {
  // Make sure we don't change the value of errno, in case a signal comes in between the process
  // making a syscall and checking errno.
  ErrnoRestorer restorer;

  // It's possible somebody cleared the SA_SIGINFO flag, which would mean
  // our "info" arg holds an undefined value.
  if (!have_siginfo(signal_number)) {
    info = nullptr;
  }

  struct siginfo si = {};
  if (!info) {
    memset(&si, 0, sizeof(si));
    si.si_signo = signal_number;
    si.si_code = SI_USER;
    si.si_pid = __getpid();
    si.si_uid = getuid();
    info = &si;
  } else if (info->si_code >= 0 || info->si_code == SI_TKILL) {
    // rt_tgsigqueueinfo(2)'s documentation appears to be incorrect on kernels
    // that contain commit 66dd34a (3.9+). The manpage claims to only allow
    // negative si_code values that are not SI_TKILL, but 66dd34a changed the
    // check to allow all si_code values in calls coming from inside the house.
  }

  void* abort_message = nullptr;
  if (signal_number != DEBUGGER_SIGNAL && g_callbacks.get_abort_message) {
    abort_message = g_callbacks.get_abort_message();
  }

  // If sival_int is ~0, it means that the fallback handler has been called
  // once before and this function is being called again to dump the stack
  // of a specific thread. It is possible that the prctl call might return 1,
  // then return 0 in subsequent calls, so check the sival_int to determine if
  // the fallback handler should be called first.
  if (info->si_value.sival_int == ~0 || prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) == 1) {
    // This check might be racy if another thread sets NO_NEW_PRIVS, but this should be unlikely,
    // you can only set NO_NEW_PRIVS to 1, and the effect should be at worst a single missing
    // ANR trace.
    debuggerd_fallback_handler(info, static_cast<ucontext_t*>(context), abort_message);
    resend_signal(info);
    return;
  }

  // Only allow one thread to handle a signal at a time.
  int ret = pthread_mutex_lock(&crash_mutex);
  if (ret != 0) {
    async_safe_format_log(ANDROID_LOG_INFO, "libc", "pthread_mutex_lock failed: %s", strerror(ret));
    return;
  }

  log_signal_summary(info);

  debugger_thread_info thread_info = {
      .pseudothread_tid = -1,
      .crashing_tid = __gettid(),
      .siginfo = info,
      .ucontext = context,
      .abort_msg = reinterpret_cast<uintptr_t>(abort_message),
  };

  // Set PR_SET_DUMPABLE to 1, so that crash_dump can ptrace us.
  int orig_dumpable = prctl(PR_GET_DUMPABLE);
  if (prctl(PR_SET_DUMPABLE, 1) != 0) {
    fatal_errno("failed to set dumpable");
  }

  // On kernels with yama_ptrace enabled, also allow any process to attach.
  bool restore_orig_ptracer = true;
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
    if (errno == EINVAL) {
      // This kernel does not support PR_SET_PTRACER_ANY, or Yama is not enabled.
      restore_orig_ptracer = false;
    } else {
      fatal_errno("failed to set traceable");
    }
  }

  // Essentially pthread_create without CLONE_FILES, so we still work during file descriptor exhaustion.
  pid_t child_pid =
    clone(debuggerd_dispatch_pseudothread, pseudothread_stack,
          CLONE_THREAD | CLONE_SIGHAND | CLONE_VM | CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID,
          &thread_info, nullptr, nullptr, &thread_info.pseudothread_tid);
  if (child_pid == -1) {
    fatal_errno("failed to spawn debuggerd dispatch thread");
  }

  // Wait for the child to start...
  futex_wait(&thread_info.pseudothread_tid, -1);

  // and then wait for it to terminate.
  futex_wait(&thread_info.pseudothread_tid, child_pid);

  // Restore PR_SET_DUMPABLE to its original value.
  if (prctl(PR_SET_DUMPABLE, orig_dumpable) != 0) {
    fatal_errno("failed to restore dumpable");
  }

  // Restore PR_SET_PTRACER to its original value.
  if (restore_orig_ptracer && prctl(PR_SET_PTRACER, 0) != 0) {
    fatal_errno("failed to restore traceable");
  }

  if (info->si_signo == DEBUGGER_SIGNAL) {
    // If the signal is fatal, don't unlock the mutex to prevent other crashing threads from
    // starting to dump right before our death.
    pthread_mutex_unlock(&crash_mutex);
  } else {
    // Resend the signal, so that either gdb or the parent's waitpid sees it.
    resend_signal(info);
  }
}



static int debuggerd_dispatch_pseudothread(void* arg) {
  debugger_thread_info* thread_info = static_cast<debugger_thread_info*>(arg);

  for (int i = 0; i < 1024; ++i) {
    close(i);
  }

  int devnull = TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
  if (devnull == -1) {
    fatal_errno("failed to open /dev/null");
  } else if (devnull != 0) {
    fatal_errno("expected /dev/null fd to be 0, actually %d", devnull);
  }

  // devnull will be 0.
  TEMP_FAILURE_RETRY(dup2(devnull, 1));
  TEMP_FAILURE_RETRY(dup2(devnull, 2));

  unique_fd input_read, input_write;
  unique_fd output_read, output_write;
  // 管道创建
  if (!Pipe(&input_read, &input_write) != 0 || !Pipe(&output_read, &output_write)) {
    fatal_errno("failed to create pipe");
  }

  // ucontext_t is absurdly large on AArch64, so piece it together manually with writev.
  uint32_t version = 1;
  constexpr size_t expected = sizeof(version) + sizeof(siginfo_t) + sizeof(ucontext_t) + sizeof(uintptr_t);

  errno = 0;
  if (fcntl(output_write.get(), F_SETPIPE_SZ, expected) < static_cast<int>(expected)) {
    fatal_errno("failed to set pipe bufer size");
  }

  struct iovec iovs[4] = {
      {.iov_base = &version, .iov_len = sizeof(version)},
      {.iov_base = thread_info->siginfo, .iov_len = sizeof(siginfo_t)},
      {.iov_base = thread_info->ucontext, .iov_len = sizeof(ucontext_t)},
      {.iov_base = &thread_info->abort_msg, .iov_len = sizeof(uintptr_t)},
  };

  ssize_t rc = TEMP_FAILURE_RETRY(writev(output_write.get(), iovs, 4));
  if (rc == -1) {
    fatal_errno("failed to write crash info");
  } else if (rc != expected) {
    fatal("failed to write crash info, wrote %zd bytes, expected %zd", rc, expected);
  }

  // Don't use fork(2) to avoid calling pthread_atfork handlers.
  pid_t crash_dump_pid = __fork();
  if (crash_dump_pid == -1) {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "failed to fork in debuggerd signal handler: %s", strerror(errno));
  } else if (crash_dump_pid == 0) {
    TEMP_FAILURE_RETRY(dup2(input_write.get(), STDOUT_FILENO));
    TEMP_FAILURE_RETRY(dup2(output_read.get(), STDIN_FILENO));
    input_read.reset();
    input_write.reset();
    output_read.reset();
    output_write.reset();

    raise_caps();

    char main_tid[10];
    char pseudothread_tid[10];
    char debuggerd_dump_type[10];
    // 格式化参数
    async_safe_format_buffer(main_tid, sizeof(main_tid), "%d", thread_info->crashing_tid);
    async_safe_format_buffer(pseudothread_tid, sizeof(pseudothread_tid), "%d", thread_info->pseudothread_tid);
    async_safe_format_buffer(debuggerd_dump_type, sizeof(debuggerd_dump_type), "%d", get_dump_type(thread_info));
    /*
    #if defined(__LP64__)
    #define CRASH_DUMP_NAME "crash_dump64"
    #else
    #define CRASH_DUMP_NAME "crash_dump32"
    #endif

    #define CRASH_DUMP_PATH "/system/bin/" CRASH_DUMP_NAME
    */
    execle(CRASH_DUMP_PATH, CRASH_DUMP_NAME, main_tid, pseudothread_tid, debuggerd_dump_type, nullptr, nullptr);
    //
    fatal_errno("exec failed");
  }

  input_write.reset();
  output_read.reset();

  // crash_dump will ptrace and pause all of our threads, and then write to the pipe to tell
  // us to fork off a process to read memory from.
  char buf[4];
  rc = TEMP_FAILURE_RETRY(read(input_read.get(), &buf, sizeof(buf)));
  if (rc == -1) {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "read of IPC pipe failed: %s", strerror(errno));
    return 1;
  } else if (rc == 0) {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "crash_dump helper failed to exec");
    return 1;
  } else if (rc != 1) {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "read of IPC pipe returned unexpected value: %zd", rc);
    return 1;
  } else if (buf[0] != '\1') {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "crash_dump helper reported failure");
    return 1;
  }

  // crash_dump is ptracing us, fork off a copy of our address space for it to use.
  create_vm_process();

  // Don't leave a zombie child.
  int status;
  if (TEMP_FAILURE_RETRY(waitpid(crash_dump_pid, &status, 0)) == -1) {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "failed to wait for crash_dump helper: %s", strerror(errno));
  } else if (WIFSTOPPED(status) || WIFSIGNALED(status)) {
    async_safe_format_log(ANDROID_LOG_FATAL, "libc", "crash_dump helper crashed or stopped");
  }

  if (thread_info->siginfo->si_signo != DEBUGGER_SIGNAL) {
    // For crashes, we don't need to minimize pause latency.
    // Wait for the dump to complete before having the process exit, to avoid being murdered by
    // ActivityManager or init.
    TEMP_FAILURE_RETRY(read(input_read, &buf, sizeof(buf)));
  }

  return 0;
}

























// @	system/core/debuggerd/handler/debuggerd_fallback.cpp
extern "C" void debuggerd_fallback_handler(siginfo_t* info, ucontext_t* ucontext, void* abort_message) {
  if (info->si_signo == DEBUGGER_SIGNAL && info->si_value.sival_int != 0) {
    return trace_handler(info, ucontext);
  } else {
    return crash_handler(info, ucontext, abort_message);
  }
}


static void crash_handler(siginfo_t* info, ucontext_t* ucontext, void* abort_message) {
  // Only allow one thread to handle a crash at a time (this can happen multiple times without
  // exit, since tombstones can be requested without a real crash happening.)
  static std::recursive_mutex crash_mutex;
  static int lock_count;

  crash_mutex.lock();
  if (lock_count++ > 0) {
    async_safe_format_log(ANDROID_LOG_ERROR, "libc", "recursed signal handler call, exiting");
    _exit(1);
  }

  unique_fd tombstone_socket, output_fd;
  bool tombstoned_connected = tombstoned_connect(getpid(), &tombstone_socket, &output_fd, kDebuggerdTombstone);
  debuggerd_fallback_tombstone(output_fd.get(), ucontext, info, abort_message);
  if (tombstoned_connected) {
    tombstoned_notify_completion(tombstone_socket.get());
  }

  --lock_count;
  crash_mutex.unlock();
}

/**
output_fd 执行 tombstone 输出文件
ucontext 信号回调函数返回，系统API
siginfo 信号回调函数返回，系统API
abort_message
*/
static void debuggerd_fallback_tombstone(int output_fd, ucontext_t* ucontext, siginfo_t* siginfo,void* abort_message) {
	//
  if (!__linker_enable_fallback_allocator()) {
    async_safe_format_log(ANDROID_LOG_ERROR, "libc", "fallback allocator already in use");
    return;
  }

  engrave_tombstone_ucontext(output_fd, reinterpret_cast<uintptr_t>(abort_message), siginfo,ucontext);
  __linker_disable_fallback_allocator();
}


//	@	system/core/debuggerd/libdebuggerd/tombstone.cpp
void engrave_tombstone_ucontext(int tombstone_fd, uint64_t abort_msg_address, siginfo_t* siginfo, ucontext_t* ucontext) {
  pid_t pid = getpid();
  pid_t tid = gettid();

  log_t log;
  log.current_tid = tid;
  log.crashed_tid = tid;
  log.tfd = tombstone_fd;
  log.amfd_data = nullptr;

  char thread_name[16];
  char process_name[128];

  read_with_default("/proc/self/comm", thread_name, sizeof(thread_name), "<unknown>");
  read_with_default("/proc/self/cmdline", process_name, sizeof(process_name), "<unknown>");

  std::unique_ptr<Regs> regs(Regs::CreateFromUcontext(Regs::CurrentArch(), ucontext));

  std::map<pid_t, ThreadInfo> threads;
  threads[gettid()] = ThreadInfo{
      .registers = std::move(regs),
      .tid = tid,
      .thread_name = thread_name,
      .pid = pid,
      .process_name = process_name,
      .siginfo = siginfo,
  };

  std::unique_ptr<BacktraceMap> backtrace_map(BacktraceMap::Create(getpid(), false));
  if (!backtrace_map) {
    ALOGE("failed to create backtrace map");
    _exit(1);
  }

  std::shared_ptr<Memory> process_memory = backtrace_map->GetProcessMemory();
  engrave_tombstone(unique_fd(dup(tombstone_fd)), backtrace_map.get(), process_memory.get(), threads, tid, abort_msg_address, nullptr, nullptr);
}
