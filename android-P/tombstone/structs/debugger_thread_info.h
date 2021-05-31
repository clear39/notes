


struct debugger_thread_info {
  pid_t crashing_tid;
  pid_t pseudothread_tid;
  siginfo_t* siginfo;
  void* ucontext;
  uintptr_t abort_msg;
};
