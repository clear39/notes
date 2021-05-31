




//  @ system/core/debuggerd/protocol.h


// Sent from handler to crash_dump via pipe.
struct __attribute__((__packed__)) CrashInfo {
  uint32_t version;  // must be 1.
  siginfo_t siginfo;
  ucontext_t ucontext;
  uintptr_t abort_msg_address;
};
