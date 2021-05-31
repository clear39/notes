
//  @ system/core/debuggerd/crash_dump.cpp

int main(int argc, char** argv) {
  DefuseSignalHandlers();

  atrace_begin(ATRACE_TAG, "before reparent");
  pid_t target_process = getppid();

  // Open /proc/`getppid()` before we daemonize.
  std::string target_proc_path = "/proc/" + std::to_string(target_process);
  int target_proc_fd = open(target_proc_path.c_str(), O_DIRECTORY | O_RDONLY);
  if (target_proc_fd == -1) {
    PLOG(FATAL) << "failed to open " << target_proc_path;
  }

  // Make sure getppid() hasn't changed.
  if (getppid() != target_process) {
    LOG(FATAL) << "parent died";
  }
  atrace_end(ATRACE_TAG);

  // Reparent ourselves to init, so that the signal handler can waitpid on the
  // original process to avoid leaving a zombie for non-fatal dumps.
  // Move the input/output pipes off of stdout/stderr, out of paranoia.
  unique_fd output_pipe(dup(STDOUT_FILENO));
  unique_fd input_pipe(dup(STDIN_FILENO));

  unique_fd fork_exit_read, fork_exit_write;
  if (!Pipe(&fork_exit_read, &fork_exit_write)) {
    PLOG(FATAL) << "failed to create pipe";
  }

  pid_t forkpid = fork();
  if (forkpid == -1) {
    PLOG(FATAL) << "fork failed";
  } else if (forkpid == 0) {
    fork_exit_read.reset();
  } else {
    // We need the pseudothread to live until we get around to verifying the vm pid against it.
    // The last thing it does is block on a waitpid on us, so wait until our child tells us to die.
    fork_exit_write.reset();
    char buf;
    TEMP_FAILURE_RETRY(read(fork_exit_read.get(), &buf, sizeof(buf)));
    _exit(0);
  }

  ATRACE_NAME("after reparent");
  pid_t pseudothread_tid;
  DebuggerdDumpType dump_type;
  uintptr_t abort_address = 0;

  Initialize(argv);
  ParseArgs(argc, argv, &pseudothread_tid, &dump_type);

  // Die if we take too long.
  //
  // Note: processes with many threads and minidebug-info can take a bit to
  //       unwind, do not make this too small. b/62828735
  alarm(30);

  // Get the process name (aka cmdline).
  std::string process_name = get_process_name(g_target_thread);

  // Collect the list of open files.
  OpenFilesList open_files;
  {
    ATRACE_NAME("open files");
    populate_open_files_list(g_target_thread, &open_files);
  }

  // In order to reduce the duration that we pause the process for, we ptrace
  // the threads, fetch their registers and associated information, and then
  // fork a separate process as a snapshot of the process's address space.
  std::set<pid_t> threads;
  // 获取所有线程
  // system/core/libprocinfo/include/procinfo/process.h
  // 读取/proc/pid/task/
  if (!android::procinfo::GetProcessTids(g_target_thread, &threads)) {
    PLOG(FATAL) << "failed to get process threads";
  }

  std::map<pid_t, ThreadInfo> thread_info;
  siginfo_t siginfo;
  std::string error;

  {
    ATRACE_NAME("ptrace");
    // 遍历线程数组
    for (pid_t thread : threads) {
      // Trace the pseudothread separately, so we can use different options.
      if (thread == pseudothread_tid) {
        continue;
      }

      if (!ptrace_seize_thread(target_proc_fd, thread, &error)) {
        bool fatal = thread == g_target_thread;
        LOG(fatal ? FATAL : WARNING) << error;
      }

      ThreadInfo info;
      info.pid = target_process;
      info.tid = thread;
      info.process_name = process_name;
      info.thread_name = get_thread_name(thread);
      //  system/core/debuggerd/crash_dump.cpp
      if (!ptrace_interrupt(thread, &info.signo)) {
        PLOG(WARNING) << "failed to ptrace interrupt thread " << thread;
        ptrace(PTRACE_DETACH, thread, 0, 0);
        continue;
      }

      if (thread == g_target_thread) {
        // Read the thread's registers along with the rest of the crash info out of the pipe.
        // abort_address 获取异常地址
        ReadCrashInfo(input_pipe, &siginfo, &info.registers, &abort_address);
        info.siginfo = &siginfo;
        info.signo = info.siginfo->si_signo;
      } else {
        info.registers.reset(Regs::RemoteGet(thread));
        if (!info.registers) {
          PLOG(WARNING) << "failed to fetch registers for thread " << thread;
          ptrace(PTRACE_DETACH, thread, 0, 0);
          continue;
        }
      }

      thread_info[thread] = std::move(info);
    }

  }

  // Trace the pseudothread with PTRACE_O_TRACECLONE and tell it to fork.
  if (!ptrace_seize_thread(target_proc_fd, pseudothread_tid, &error, PTRACE_O_TRACECLONE)) {
    LOG(FATAL) << "failed to seize pseudothread: " << error;
  }

  if (TEMP_FAILURE_RETRY(write(output_pipe.get(), "\1", 1)) != 1) {
    PLOG(FATAL) << "failed to write to pseudothread";
  }

  pid_t vm_pid = wait_for_vm_process(pseudothread_tid);
  if (ptrace(PTRACE_DETACH, pseudothread_tid, 0, 0) != 0) {
    PLOG(FATAL) << "failed to detach from pseudothread";
  }

  // The pseudothread can die now.
  fork_exit_write.reset();

  // Defer the message until later, for readability.
  bool wait_for_gdb = android::base::GetBoolProperty("debug.debuggerd.wait_for_gdb", false);
  if (siginfo.si_signo == DEBUGGER_SIGNAL) {
    wait_for_gdb = false;
  }

  // Detach from all of our attached threads before resuming.
  for (const auto& [tid, thread] : thread_info) {
    int resume_signal = thread.signo == DEBUGGER_SIGNAL ? 0 : thread.signo;
    if (wait_for_gdb) {
      resume_signal = 0;
      if (tgkill(target_process, tid, SIGSTOP) != 0) {
        PLOG(WARNING) << "failed to send SIGSTOP to " << tid;
      }
    }

    LOG(DEBUG) << "detaching from thread " << tid;
    if (ptrace(PTRACE_DETACH, tid, 0, resume_signal) != 0) {
      PLOG(ERROR) << "failed to detach from thread " << tid;
    }
  }

  // Drop our capabilities now that we've fetched all of the information we need.
  drop_capabilities();

  {
    ATRACE_NAME("tombstoned_connect");
    LOG(INFO) << "obtaining output fd from tombstoned, type: " << dump_type;
    g_tombstoned_connected = tombstoned_connect(g_target_thread, &g_tombstoned_socket, &g_output_fd, dump_type);
  }

  if (g_tombstoned_connected) {
    if (TEMP_FAILURE_RETRY(dup2(g_output_fd.get(), STDOUT_FILENO)) == -1) {
      PLOG(ERROR) << "failed to dup2 output fd (" << g_output_fd.get() << ") to STDOUT_FILENO";
    }
  } else {
    unique_fd devnull(TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR)));
    TEMP_FAILURE_RETRY(dup2(devnull.get(), STDOUT_FILENO));
    g_output_fd = std::move(devnull);
  }

  LOG(INFO) << "performing dump of process " << target_process << " (target tid = " << g_target_thread << ")";

  int signo = siginfo.si_signo;
  bool fatal_signal = signo != DEBUGGER_SIGNAL;
  bool backtrace = false;

  // si_value is special when used with DEBUGGER_SIGNAL.
  //   0: dump tombstone
  //   1: dump backtrace
  if (!fatal_signal) {
    int si_val = siginfo.si_value.sival_int;
    if (si_val == 0) {
      backtrace = false;
    } else if (si_val == 1) {
      backtrace = true;
    } else {
      LOG(WARNING) << "unknown si_value value " << si_val;
    }
  }

  // TODO: Use seccomp to lock ourselves down.
  std::unique_ptr<BacktraceMap> map(BacktraceMap::Create(vm_pid, false));
  if (!map) {
    LOG(FATAL) << "failed to create backtrace map";
  }

  std::shared_ptr<unwindstack::Memory> process_memory = map->GetProcessMemory();
  if (!process_memory) {
    LOG(FATAL) << "failed to get unwindstack::Memory handle";
  }

  std::string amfd_data;
  if (backtrace) {
    ATRACE_NAME("dump_backtrace");
    // 打印堆栈信息
    dump_backtrace(std::move(g_output_fd), map.get(), thread_info, g_target_thread);
  } else {
    ATRACE_NAME("engrave_tombstone");
    // 打印tombstone信息
    engrave_tombstone(std::move(g_output_fd), map.get(), process_memory.get(), thread_info, g_target_thread, abort_address, &open_files, &amfd_data);
  }

  if (fatal_signal) {
    // Don't try to notify ActivityManager if it just crashed, or we might hang until timeout.
    if (thread_info[target_process].thread_name != "system_server") {
      activity_manager_notify(target_process, signo, amfd_data);
    }
  }

  if (wait_for_gdb) {
    // Use ALOGI to line up with output from engrave_tombstone.
    ALOGI(
        "***********************************************************\n"
        "* Process %d has been suspended while crashing.\n"
        "* To attach gdbserver and start gdb, run this on the host:\n"
        "*\n"
        "*     gdbclient.py -p %d\n"
        "*\n"
        "***********************************************************",
        target_process, target_process);
  }

  // Close stdout before we notify tombstoned of completion.
  close(STDOUT_FILENO);
  if (g_tombstoned_connected && !tombstoned_notify_completion(g_tombstoned_socket.get())) {
    LOG(ERROR) << "failed to notify tombstoned of completion";
  }

  return 0;
}



//-------------------------------------------------------------------------
// BacktraceMap create function.
//-------------------------------------------------------------------------
BacktraceMap* BacktraceMap::Create(pid_t pid, bool uncached) {
  BacktraceMap* map;

  if (uncached) {
    // Force use of the base class to parse the maps when this call is made.
    map = new BacktraceMap(pid);
  } else if (pid == getpid()) {
    map = new UnwindStackMap(0);
  } else {
    map = new UnwindStackMap(pid);
  }
  if (!map->Build()) {
    delete map;
    return nullptr;
  }
  return map;
}


// Interrupt a process and wait for it to be interrupted.
static bool ptrace_interrupt(pid_t tid, int* received_signal) {
  //
  if (ptrace(PTRACE_INTERRUPT, tid, 0, 0) == 0) {
    return wait_for_stop(tid, received_signal);
  }

  PLOG(ERROR) << "failed to interrupt " << tid << " to detach";
  return false;
}



static void ReadCrashInfo(unique_fd& fd, siginfo_t* siginfo, std::unique_ptr<unwindstack::Regs>* regs, uintptr_t* abort_address) {
  std::aligned_storage<sizeof(CrashInfo) + 1, alignof(CrashInfo)>::type buf;
  ssize_t rc = TEMP_FAILURE_RETRY(read(fd.get(), &buf, sizeof(buf)));
  if (rc == -1) {
    PLOG(FATAL) << "failed to read target ucontext";
  } else if (rc != sizeof(CrashInfo)) {
    LOG(FATAL) << "read " << rc << " bytes when reading target crash information, expected " << sizeof(CrashInfo);
  }

  CrashInfo* crash_info = reinterpret_cast<CrashInfo*>(&buf);
  if (crash_info->version != 1) {
    LOG(FATAL) << "version mismatch, expected 1, received " << crash_info->version;
  }

  *siginfo = crash_info->siginfo;
  regs->reset(Regs::CreateFromUcontext(Regs::CurrentArch(), &crash_info->ucontext));
  *abort_address = crash_info->abort_msg_address;
}
