//	@system/core/logwrapper/logwrapper.c

int main(int argc, char* argv[]) {

    int seg_fault_on_exit = 0;
    int log_target = LOG_ALOG;
    bool abbreviated = false;
    int ch;
    int status = 0xAAAA;
    int rc;

    while ((ch = getopt(argc, argv, "adk")) != -1) {
        switch (ch) {
            case 'a':
                abbreviated = true;
                break;
            case 'd':
                seg_fault_on_exit = 1;
                break;
            case 'k':
                log_target = LOG_KLOG;
                klog_set_level(6);
                break;
            case '?':
            default:
              usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1) {
        usage();
    }

    rc = android_fork_execvp_ext(argc, &argv[0], &status, true,log_target, abbreviated, NULL, NULL, 0);
    if (!rc) {
        if (WIFEXITED(status))
            rc = WEXITSTATUS(status);
        else
            rc = -ECHILD;
    }

    if (seg_fault_on_exit) {
        uintptr_t fault_address = (uintptr_t) status;
        *(int *) fault_address = 0;  // causes SIGSEGV with fault_address = status
    }

    return rc;
}


void usage() {
    fatal(
        "Usage: logwrapper [-a] [-d] [-k] BINARY [ARGS ...]\n"
        "\n"
        "Forks and executes BINARY ARGS, redirecting stdout and stderr to\n"
        "the Android logging system. Tag is set to BINARY, priority is\n"
        "always LOG_INFO.\n"
        "\n"
        "-a: Causes logwrapper to do abbreviated logging.\n"
        "    This logs up to the first 4K and last 4K of the command\n"
        "    being run, and logs the output when the command exits\n"
        "-d: Causes logwrapper to SIGSEGV when BINARY terminates\n"
        "    fault address is set to the status of wait()\n"
        "-k: Causes logwrapper to log to the kernel log instead of\n"
        "    the Android system log\n");
}


int android_fork_execvp_ext(int argc, char* argv[], int *status, bool ignore_int_quit,
        int log_target, bool abbreviated, char *file_path,void *unused_opts, int unused_opts_len) {
    pid_t pid;
    int parent_ptty;
    int child_ptty;
    struct sigaction intact;
    struct sigaction quitact;
    sigset_t blockset;
    sigset_t oldset;
    int rc = 0;

    LOG_ALWAYS_FATAL_IF(unused_opts != NULL);
    LOG_ALWAYS_FATAL_IF(unused_opts_len != 0);

    rc = pthread_mutex_lock(&fd_mutex);
    if (rc) {
        ERROR("failed to lock signal_fd mutex\n");
        goto err_lock;
    }

    /* Use ptty instead of socketpair so that STDOUT is not buffered */
    parent_ptty = TEMP_FAILURE_RETRY(open("/dev/ptmx", O_RDWR));
    if (parent_ptty < 0) {
        ERROR("Cannot create parent ptty\n");
        rc = -1;
        goto err_open;
    }

    char child_devname[64];
    //grantpt	grant access to the slave pseudoterminal(授予对从属伪终端的访问权)
    //unlockpt	unlock a pseudoterminal master/slave pair(解锁伪终端主/从对)
    //ptsname_r			get the name of the slave pseudoterminal(获取从属伪终端的名称)
    if (grantpt(parent_ptty) || unlockpt(parent_ptty) || ptsname_r(parent_ptty, child_devname, sizeof(child_devname)) != 0) {
        ERROR("Problem with /dev/ptmx\n");
        rc = -1;
        goto err_ptty;
    }

    child_ptty = TEMP_FAILURE_RETRY(open(child_devname, O_RDWR));
    if (child_ptty < 0) {
        ERROR("Cannot open child_ptty\n");
        rc = -1;
        goto err_child_ptty;
    }

    sigemptyset(&blockset);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGQUIT);
    pthread_sigmask(SIG_BLOCK, &blockset, &oldset);

    pid = fork();
    if (pid < 0) {
        close(child_ptty);
        ERROR("Failed to fork\n");
        rc = -1;
        goto err_fork;
    } else if (pid == 0) {//子进程
        pthread_mutex_unlock(&fd_mutex);
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        close(parent_ptty);

        dup2(child_ptty, 1);
        dup2(child_ptty, 2);
        close(child_ptty);

        child(argc, argv);
    } else {//父进程
        close(child_ptty);
        if (ignore_int_quit) {
            struct sigaction ignact;

            memset(&ignact, 0, sizeof(ignact));
            ignact.sa_handler = SIG_IGN;
            sigaction(SIGINT, &ignact, &intact);
            sigaction(SIGQUIT, &ignact, &quitact);
        }

        rc = parent(argv[0], parent_ptty, pid, status, log_target,abbreviated, file_path);
    }

    if (ignore_int_quit) {
        sigaction(SIGINT, &intact, NULL);
        sigaction(SIGQUIT, &quitact, NULL);
    }


err_fork:
    pthread_sigmask(SIG_SETMASK, &oldset, NULL);
err_child_ptty:
err_ptty:
    close(parent_ptty);
err_open:
    pthread_mutex_unlock(&fd_mutex);
err_lock:
    return rc;
}


static void child(int argc, char* argv[]) {
    // create null terminated argv_child array
    char* argv_child[argc + 1];
    memcpy(argv_child, argv, argc * sizeof(char *));
    argv_child[argc] = NULL;

    if (execvp(argv_child[0], argv_child)) {
        FATAL_CHILD("executing %s failed: %s\n", argv_child[0],strerror(errno));
    }
}


static int parent(const char *tag, int parent_read, pid_t pid,int *chld_sts, int log_target, bool abbreviated, char *file_path) {
    int status = 0;
    char buffer[4096];
    struct pollfd poll_fds[] = {
        [0] = {
            .fd = parent_read,
            .events = POLLIN,
        },
    };
    int rc = 0;
    int fd;

    struct log_info log_info;

    int a = 0;  // start index of unprocessed data
    int b = 0;  // end index of unprocessed data
    int sz;
    bool found_child = false;
    char tmpbuf[256];

    log_info.btag = basename(tag);
    if (!log_info.btag) {
        log_info.btag = (char*) tag;
    }

    if (abbreviated && (log_target == LOG_NONE)) {
        abbreviated = 0;
    }
    if (abbreviated) {
        init_abbr_buf(&log_info.a_buf);
    }

    if (log_target & LOG_KLOG) {
        snprintf(log_info.klog_fmt, sizeof(log_info.klog_fmt),"<6>%.*s: %%s\n", MAX_KLOG_TAG, log_info.btag);
    }

    if ((log_target & LOG_FILE) && !file_path) {
        /* No file_path specified, clear the LOG_FILE bit */
        log_target &= ~LOG_FILE;
    }

    if (log_target & LOG_FILE) {
        fd = open(file_path, O_WRONLY | O_CREAT, 0664);
        if (fd < 0) {
            ERROR("Cannot log to file %s\n", file_path);
            log_target &= ~LOG_FILE;
        } else {
            lseek(fd, 0, SEEK_END);
            log_info.fp = fdopen(fd, "a");
        }
    }

    log_info.log_target = log_target;
    log_info.abbreviated = abbreviated;

    while (!found_child) {
        if (TEMP_FAILURE_RETRY(poll(poll_fds, ARRAY_SIZE(poll_fds), -1)) < 0) {
            ERROR("poll failed\n");
            rc = -1;
            goto err_poll;
        }

        if (poll_fds[0].revents & POLLIN) {
            sz = TEMP_FAILURE_RETRY(read(parent_read, &buffer[b], sizeof(buffer) - 1 - b));

            sz += b;
            // Log one line at a time
            for (b = 0; b < sz; b++) {
                if (buffer[b] == '\r') {
                    if (abbreviated) {
                        /* The abbreviated logging code uses newline as
                         * the line separator.  Lucikly, the pty layer
                         * helpfully cooks the output of the command
                         * being run and inserts a CR before NL.  So
                         * I just change it to NL here when doing
                         * abbreviated logging.
                         */
                        buffer[b] = '\n';
                    } else {
                        buffer[b] = '\0';
                    }
                } else if (buffer[b] == '\n') {
                    buffer[b] = '\0';
                    log_line(&log_info, &buffer[a], b - a);
                    a = b + 1;
                }
            }

            if (a == 0 && b == sizeof(buffer) - 1) {
                // buffer is full, flush
                buffer[b] = '\0';
                log_line(&log_info, &buffer[a], b - a);
                b = 0;
            } else if (a != b) {
                // Keep left-overs
                b -= a;
                memmove(buffer, &buffer[a], b);
                a = 0;
            } else {
                a = 0;
                b = 0;
            }
        }

        if (poll_fds[0].revents & POLLHUP) {
            int ret;

            ret = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
            if (ret < 0) {
                rc = errno;
                ALOG(LOG_ERROR, "logwrap", "waitpid failed with %s\n", strerror(errno));
                goto err_waitpid;
            }
            if (ret > 0) {
                found_child = true;
            }
        }
    }

    if (chld_sts != NULL) {
        *chld_sts = status;
    } else {
      if (WIFEXITED(status))
        rc = WEXITSTATUS(status);
      else
        rc = -ECHILD;
    }

    // Flush remaining data
    if (a != b) {
      buffer[b] = '\0';
      log_line(&log_info, &buffer[a], b - a);
    }

    /* All the output has been processed, time to dump the abbreviated output */
    if (abbreviated) {
        print_abbr_buf(&log_info);
    }

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status)) {
        snprintf(tmpbuf, sizeof(tmpbuf),"%s terminated by exit(%d)\n", log_info.btag, WEXITSTATUS(status));
        do_log_line(&log_info, tmpbuf);
      }
    } else {
      if (WIFSIGNALED(status)) {
        snprintf(tmpbuf, sizeof(tmpbuf),"%s terminated by signal %d\n", log_info.btag, WTERMSIG(status));
        do_log_line(&log_info, tmpbuf);
      } else if (WIFSTOPPED(status)) {
        snprintf(tmpbuf, sizeof(tmpbuf),"%s stopped by signal %d\n", log_info.btag, WSTOPSIG(status));
        do_log_line(&log_info, tmpbuf);
      }
    }

err_waitpid:
err_poll:
    if (log_target & LOG_FILE) {
        fclose(log_info.fp); /* Also closes underlying fd */
    }
    if (abbreviated) {
        free_abbr_buf(&log_info.a_buf);
    }
    return rc;
}


/* Log to either the abbreviated buf, or directly to the specified log
 * via do_log_line() above.
 */
static void log_line(struct log_info *log_info, char *line, int len) {
    if (log_info->abbreviated) {
        add_line_to_abbr_buf(&log_info->a_buf, line, len);
    } else {
        do_log_line(log_info, line);
    }
}

static void add_line_to_abbr_buf(struct abbr_buf *a_buf, char *linebuf, int linelen) {
    if (!a_buf->beginning_buf_full) {
        a_buf->beginning_buf_full = add_line_to_linear_buf(&a_buf->b_buf, linebuf, linelen);
    }
    if (a_buf->beginning_buf_full) {
        add_line_to_circular_buf(&a_buf->e_buf, linebuf, linelen);
    }
}



/* Log directly to the specified log */
static void do_log_line(struct log_info *log_info, char *line) {
    if (log_info->log_target & LOG_KLOG) {
        klog_write(6, log_info->klog_fmt, line);
    }
    if (log_info->log_target & LOG_ALOG) {
        ALOG(LOG_INFO, log_info->btag, "%s", line);
    }
    if (log_info->log_target & LOG_FILE) {
        fprintf(log_info->fp, "%s\n", line);
    }
}
