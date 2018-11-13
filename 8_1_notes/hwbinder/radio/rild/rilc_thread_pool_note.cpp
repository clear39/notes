//@hardware/ril/libril/ril_service.cpp
void rilc_thread_pool() {
    joinRpcThreadpool();//@system/libhidl/transport/HidlTransportSupport.cpp:26
}


void joinRpcThreadpool() {
    // TODO(b/32756130) this should be transport-dependent
    joinBinderRpcThreadpool();//	@system/libhidl/transport/HidlBinderSupport.cpp
}


void joinBinderRpcThreadpool() {
    IPCThreadState::self()->joinThreadPool();
}