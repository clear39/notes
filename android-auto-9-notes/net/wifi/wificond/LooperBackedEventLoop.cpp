//  @   /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/looper_backed_event_loop.cpp

class LooperBackedEventLoop: public EventLoop {

}

//  @ /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/event_loop.h
class EventLoop {}




LooperBackedEventLoop::LooperBackedEventLoop(): should_continue_(true) {
  looper_ = android::Looper::prepare(Looper::PREPARE_ALLOW_NON_CALLBACKS);
}