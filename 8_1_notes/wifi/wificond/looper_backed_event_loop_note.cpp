







//	@system/connectivity/wificond/looper_backed_event_loop.cpp
LooperBackedEventLoop::LooperBackedEventLoop(): should_continue_(true) {//	 bool should_continue_;
  looper_ = android::Looper::prepare(Looper::PREPARE_ALLOW_NON_CALLBACKS);//  sp<android::Looper> looper_;
}
