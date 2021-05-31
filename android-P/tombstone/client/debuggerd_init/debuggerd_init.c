


$ grep debuggerd_init -nrw bionic/
bionic/linker/linker_main.cpp:258:  debuggerd_init(&callbacks);


//	@	system/core/debuggerd/handler/debuggerd_handler.cpp

void debuggerd_init(debuggerd_callbacks_t* callbacks) {
  if (callbacks) {
    g_callbacks = *callbacks;
  }

  void* thread_stack_allocation = mmap(nullptr, PAGE_SIZE * 3, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (thread_stack_allocation == MAP_FAILED) {
    fatal_errno("failed to allocate debuggerd thread stack");
  }

  char* stack = static_cast<char*>(thread_stack_allocation) + PAGE_SIZE;
  if (mprotect(stack, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0) {
    fatal_errno("failed to mprotect debuggerd thread stack");
  }

  // Stack grows negatively, set it to the last byte in the page...
  stack = (stack + PAGE_SIZE - 1);
  // and align it.
  stack -= 15;
  pseudothread_stack = stack;

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);
  action.sa_sigaction = debuggerd_signal_handler;
  action.sa_flags = SA_RESTART | SA_SIGINFO;

  // Use the alternate signal stack if available so we can catch stack overflows.
  action.sa_flags |= SA_ONSTACK;
  debuggerd_register_handlers(&action);
}


static void __attribute__((__unused__)) debuggerd_register_handlers(struct sigaction* action) {
  sigaction(SIGABRT, action, nullptr);
  sigaction(SIGBUS, action, nullptr);
  sigaction(SIGFPE, action, nullptr);
  sigaction(SIGILL, action, nullptr);
  sigaction(SIGSEGV, action, nullptr);
#if defined(SIGSTKFLT)
  sigaction(SIGSTKFLT, action, nullptr);
#endif
  sigaction(SIGSYS, action, nullptr);
  sigaction(SIGTRAP, action, nullptr);
  sigaction(DEBUGGER_SIGNAL, action, nullptr);
}