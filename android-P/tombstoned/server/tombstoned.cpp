
int main(int, char* []) {
  umask(0137);

  // Don't try to connect to ourselves if we crash.
  struct sigaction action = {}; 
  action.sa_handler = [](int signal) {
    LOG(ERROR) << "received fatal signal " << signal;
    _exit(1);
  };  
  debuggerd_register_handlers(&action);

  int intercept_socket = android_get_control_socket(kTombstonedInterceptSocketName);
  int crash_socket = android_get_control_socket(kTombstonedCrashSocketName);

  if (intercept_socket == -1 || crash_socket == -1) {
    PLOG(FATAL) << "failed to get socket from init";
  }

  evutil_make_socket_nonblocking(intercept_socket);
  evutil_make_socket_nonblocking(crash_socket);

  event_base* base = event_base_new();
  if (!base) {
    LOG(FATAL) << "failed to create event_base";
  }

  intercept_manager = new InterceptManager(base, intercept_socket);

  evconnlistener* tombstone_listener =
      evconnlistener_new(base, crash_accept_cb, CrashQueue::for_tombstones(), LEV_OPT_CLOSE_ON_FREE,
                         -1 /* backlog */, crash_socket);
  if (!tombstone_listener) {
    LOG(FATAL) << "failed to create evconnlistener for tombstones.";
  }

  if (kJavaTraceDumpsEnabled) {
    const int java_trace_socket = android_get_control_socket(kTombstonedJavaTraceSocketName);
    if (java_trace_socket == -1) {
      PLOG(FATAL) << "failed to get socket from init";
    }   

    evutil_make_socket_nonblocking(java_trace_socket);
    evconnlistener* java_trace_listener =
        evconnlistener_new(base, crash_accept_cb, CrashQueue::for_anrs(), LEV_OPT_CLOSE_ON_FREE, -1 /* backlog */, java_trace_socket);
    
    if (!java_trace_listener) {
      LOG(FATAL) << "failed to create evconnlistener for java traces.";
    }
  }

  LOG(INFO) << "tombstoned successfully initialized";
  event_base_dispatch(base);
}

