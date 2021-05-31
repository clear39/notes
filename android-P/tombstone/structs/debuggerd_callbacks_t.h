
//	@	system/core/debuggerd/include/debuggerd/handler.h


// These callbacks are called in a signal handler, and thus must be async signal safe.
// If null, the callbacks will not be called.
typedef struct {
  struct abort_msg_t* (*get_abort_message)();
  void (*post_dump)();
} debuggerd_callbacks_t;




// @	bionic/linker/linker_main.cpp

debuggerd_callbacks_t callbacks = {
    .get_abort_message = []() {
      return g_abort_message;
    },
    .post_dump = &notify_gdb_of_libraries,
  };


  debuggerd_init(&callbacks);
