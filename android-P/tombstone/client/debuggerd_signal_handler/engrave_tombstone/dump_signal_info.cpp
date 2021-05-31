
//  @ system/core/debuggerd/libdebuggerd/tombstone.cpp
static void dump_signal_info(log_t* log, const siginfo_t* si) {
  char addr_desc[32]; // ", fault addr 0x1234"
  if (signal_has_si_addr(si->si_signo, si->si_code)) {
    snprintf(addr_desc, sizeof(addr_desc), "%p", si->si_addr);
  } else {
    snprintf(addr_desc, sizeof(addr_desc), "--------");
  }

  _LOG(log, logtype::HEADER, "signal %d (%s), code %d (%s), fault addr %s\n", si->si_signo,
       get_signame(si->si_signo), si->si_code, get_sigcode(si->si_signo, si->si_code), addr_desc);

  dump_probable_cause(log, si);
}




static void dump_probable_cause(log_t* log, const siginfo_t* si) {
  std::string cause;
  if (si->si_signo == SIGSEGV && si->si_code == SEGV_MAPERR) {
    if (si->si_addr < reinterpret_cast<void*>(4096)) {
      cause = StringPrintf("null pointer dereference");
    } else if (si->si_addr == reinterpret_cast<void*>(0xffff0ffc)) {
      cause = "call to kuser_helper_version";
    } else if (si->si_addr == reinterpret_cast<void*>(0xffff0fe0)) {
      cause = "call to kuser_get_tls";
    } else if (si->si_addr == reinterpret_cast<void*>(0xffff0fc0)) {
      cause = "call to kuser_cmpxchg";
    } else if (si->si_addr == reinterpret_cast<void*>(0xffff0fa0)) {
      cause = "call to kuser_memory_barrier";
    } else if (si->si_addr == reinterpret_cast<void*>(0xffff0f60)) {
      cause = "call to kuser_cmpxchg64";
    }
  } else if (si->si_signo == SIGSYS && si->si_code == SYS_SECCOMP) {
    cause = StringPrintf("seccomp prevented call to disallowed %s system call %d", ABI_STRING, si->si_syscall);
  }

  if (!cause.empty()) _LOG(log, logtype::HEADER, "Cause: %s\n", cause.c_str());
}

//   @ system/core/debuggerd/libdebuggerd/utility.cpp
bool signal_has_si_addr(int si_signo, int si_code) {
  // Manually sent signals won't have si_addr.
  if (si_code == SI_USER || si_code == SI_QUEUE || si_code == SI_TKILL) {
    return false;
  }

  switch (si_signo) {
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
    case SIGSEGV:
    case SIGTRAP:
      return true;
    default:
      return false;
  }
}

const char* get_signame(int sig) {
  switch (sig) {
    case SIGABRT: return "SIGABRT";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    case SIGSEGV: return "SIGSEGV";
#if defined(SIGSTKFLT)
    case SIGSTKFLT: return "SIGSTKFLT";
#endif
    case SIGSTOP: return "SIGSTOP";
    case SIGSYS: return "SIGSYS";
    case SIGTRAP: return "SIGTRAP";
    case DEBUGGER_SIGNAL: return "<debuggerd signal>";
    default: return "?";
  }
}


const char* get_sigcode(int signo, int code) {
  // Try the signal-specific codes...
  switch (signo) {
    case SIGILL:
      switch (code) {
        case ILL_ILLOPC: return "ILL_ILLOPC";
        case ILL_ILLOPN: return "ILL_ILLOPN";
        case ILL_ILLADR: return "ILL_ILLADR";
        case ILL_ILLTRP: return "ILL_ILLTRP";
        case ILL_PRVOPC: return "ILL_PRVOPC";
        case ILL_PRVREG: return "ILL_PRVREG";
        case ILL_COPROC: return "ILL_COPROC";
        case ILL_BADSTK: return "ILL_BADSTK";
      }
      static_assert(NSIGILL == ILL_BADSTK, "missing ILL_* si_code");
      break;
    case SIGBUS:
      switch (code) {
        case BUS_ADRALN: return "BUS_ADRALN";
        case BUS_ADRERR: return "BUS_ADRERR";
        case BUS_OBJERR: return "BUS_OBJERR";
        case BUS_MCEERR_AR: return "BUS_MCEERR_AR";
        case BUS_MCEERR_AO: return "BUS_MCEERR_AO";
      }
      static_assert(NSIGBUS == BUS_MCEERR_AO, "missing BUS_* si_code");
      break;
    case SIGFPE:
      switch (code) {
        case FPE_INTDIV: return "FPE_INTDIV";
        case FPE_INTOVF: return "FPE_INTOVF";
        case FPE_FLTDIV: return "FPE_FLTDIV";
        case FPE_FLTOVF: return "FPE_FLTOVF";
        case FPE_FLTUND: return "FPE_FLTUND";
        case FPE_FLTRES: return "FPE_FLTRES";
        case FPE_FLTINV: return "FPE_FLTINV";
        case FPE_FLTSUB: return "FPE_FLTSUB";
      }
      static_assert(NSIGFPE == FPE_FLTSUB, "missing FPE_* si_code");
      break;
    case SIGSEGV:
      switch (code) {
        case SEGV_MAPERR: return "SEGV_MAPERR";
        case SEGV_ACCERR: return "SEGV_ACCERR";
#if defined(SEGV_BNDERR)
        case SEGV_BNDERR: return "SEGV_BNDERR";
#endif
#if defined(SEGV_PKUERR)
        case SEGV_PKUERR: return "SEGV_PKUERR";
#endif
      }
#if defined(SEGV_PKUERR)
      static_assert(NSIGSEGV == SEGV_PKUERR, "missing SEGV_* si_code");
#elif defined(SEGV_BNDERR)
      static_assert(NSIGSEGV == SEGV_BNDERR, "missing SEGV_* si_code");
#else
      static_assert(NSIGSEGV == SEGV_ACCERR, "missing SEGV_* si_code");
#endif
      break;
#if defined(SYS_SECCOMP) // Our glibc is too old, and we build this for the host too.
    case SIGSYS:
      switch (code) {
        case SYS_SECCOMP: return "SYS_SECCOMP";
      }
      static_assert(NSIGSYS == SYS_SECCOMP, "missing SYS_* si_code");
      break;
#endif
    case SIGTRAP:
      switch (code) {
        case TRAP_BRKPT: return "TRAP_BRKPT";
        case TRAP_TRACE: return "TRAP_TRACE";
        case TRAP_BRANCH: return "TRAP_BRANCH";
        case TRAP_HWBKPT: return "TRAP_HWBKPT";
      }
      if ((code & 0xff) == SIGTRAP) {
        switch ((code >> 8) & 0xff) {
          case PTRACE_EVENT_FORK:
            return "PTRACE_EVENT_FORK";
          case PTRACE_EVENT_VFORK:
            return "PTRACE_EVENT_VFORK";
          case PTRACE_EVENT_CLONE:
            return "PTRACE_EVENT_CLONE";
          case PTRACE_EVENT_EXEC:
            return "PTRACE_EVENT_EXEC";
          case PTRACE_EVENT_VFORK_DONE:
            return "PTRACE_EVENT_VFORK_DONE";
          case PTRACE_EVENT_EXIT:
            return "PTRACE_EVENT_EXIT";
          case PTRACE_EVENT_SECCOMP:
            return "PTRACE_EVENT_SECCOMP";
          case PTRACE_EVENT_STOP:
            return "PTRACE_EVENT_STOP";
        }
      }
      static_assert(NSIGTRAP == TRAP_HWBKPT, "missing TRAP_* si_code");
      break;
  }
  // Then the other codes...
  switch (code) {
    case SI_USER: return "SI_USER";
    case SI_KERNEL: return "SI_KERNEL";
    case SI_QUEUE: return "SI_QUEUE";
    case SI_TIMER: return "SI_TIMER";
    case SI_MESGQ: return "SI_MESGQ";
    case SI_ASYNCIO: return "SI_ASYNCIO";
    case SI_SIGIO: return "SI_SIGIO";
    case SI_TKILL: return "SI_TKILL";
    case SI_DETHREAD: return "SI_DETHREAD";
  }
  // Then give up...
  return "?";
}
