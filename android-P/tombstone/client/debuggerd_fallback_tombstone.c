

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

  // Essentially pthread_create without CLONE_FILES, so we still work during file descriptor
  // exhaustion.
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

void engrave_tombstone(unique_fd output_fd, BacktraceMap* map, Memory* process_memory,
                       const std::map<pid_t, ThreadInfo>& threads, pid_t target_thread,
                       uint64_t abort_msg_address, OpenFilesList* open_files,
                       std::string* amfd_data) {
  // don't copy log messages to tombstone unless this is a dev device
  bool want_logs = android::base::GetBoolProperty("ro.debuggable", false);

  log_t log;
  log.current_tid = target_thread;
  log.crashed_tid = target_thread;
  log.tfd = output_fd.get();
  log.amfd_data = amfd_data;

  _LOG(&log, logtype::HEADER, "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
  dump_header_info(&log);

  auto it = threads.find(target_thread);
  if (it == threads.end()) {
    LOG(FATAL) << "failed to find target thread";
  }
  dump_thread(&log, map, process_memory, it->second, abort_msg_address, true);

  if (want_logs) {
    dump_logs(&log, it->second.pid, 50);
  }

  for (auto& [tid, thread_info] : threads) {
    if (tid == target_thread) {
      continue;
    }

    dump_thread(&log, map, process_memory, thread_info, 0, false);
  }

  if (open_files) {
    _LOG(&log, logtype::OPEN_FILES, "\nopen files:\n");
    dump_open_files_list(&log, *open_files, "    ");
  }

  if (want_logs) {
    dump_logs(&log, it->second.pid, 0);
  }
}






