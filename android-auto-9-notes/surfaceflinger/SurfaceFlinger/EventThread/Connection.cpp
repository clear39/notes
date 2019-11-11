
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/surfaceflinger/EventThread.h
class Connection : public BnDisplayEventConnection {
    
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/surfaceflinger/EventThread.cpp

/**
 * gui::BitTube mChannel;
*/
EventThread::Connection::Connection(EventThread* eventThread)
      : count(-1), mEventThread(eventThread), mChannel(gui::BitTube::DefaultSize) {}

void EventThread::Connection::onFirstRef() {
    // NOTE: mEventThread doesn't hold a strong reference on us
    mEventThread->registerDisplayEventConnection(this);
}


status_t EventThread::Connection::stealReceiveChannel(gui::BitTube* outChannel) {
    outChannel->setReceiveFd(mChannel.moveReceiveFd());
    return NO_ERROR;
}

