
//  @   frameworks/native/services/displayservice/DisplayEventReceiver.cpp

DisplayEventReceiver::AttachedEvent::AttachedEvent(const sp<IEventCallback> &callback)
    : mCallback(callback)
{
    /**
     * struct AttachedEvent : LooperCallback {
     * 
     * AttachedEvent 继承 LooperCallback
     * 
     * @    frameworks/native/include/gui/DisplayEventReceiver.h
     * 这里的mFwkReceiver 为 using FwkReceiver = ::android::DisplayEventReceiver;
     * 
    */
    mLooperAttached = getLooper()->addFd(mFwkReceiver.getFd(),
            Looper::POLL_CALLBACK,
            Looper::EVENT_INPUT,
            this,
            nullptr);
}


bool DisplayEventReceiver::AttachedEvent::valid() const {
    return mFwkReceiver.initCheck() == OK && mLooperAttached;
}


int DisplayEventReceiver::AttachedEvent::handleEvent(int fd, int events, void* /* data */) {
    CHECK(fd == mFwkReceiver.getFd());

    if (events & (Looper::EVENT_ERROR | Looper::EVENT_HANGUP)) {
        LOG(ERROR) << "AttachedEvent handleEvent received error or hangup:" << events;
        return 0; // remove the callback
    }

    if (!(events & Looper::EVENT_INPUT)) {
        LOG(ERROR) << "AttachedEvent handleEvent unhandled poll event:" << events;
        return 1; // keep the callback
    }

    constexpr size_t SIZE = 1;

    ssize_t n;
    FwkReceiver::Event buf[SIZE];
    while ((n = mFwkReceiver.getEvents(buf, SIZE)) > 0) {
        for (size_t i = 0; i < static_cast<size_t>(n); ++i) {
            const FwkReceiver::Event &event = buf[i];

            uint32_t type = event.header.type;
            uint64_t timestamp = event.header.timestamp;

            switch(buf[i].header.type) {
                case FwkReceiver::DISPLAY_EVENT_VSYNC: {
                    auto ret = mCallback->onVsync(timestamp, event.vsync.count);
                    if (!ret.isOk()) {
                        LOG(ERROR) << "AttachedEvent handleEvent fails on onVsync callback"
                                   << " because of " << ret.description();
                        return 0;  // remove the callback
                    }
                } break;
                case FwkReceiver::DISPLAY_EVENT_HOTPLUG: {
                    auto ret = mCallback->onHotplug(timestamp, event.hotplug.connected);
                    if (!ret.isOk()) {
                        LOG(ERROR) << "AttachedEvent handleEvent fails on onHotplug callback"
                                   << " because of " << ret.description();
                        return 0;  // remove the callback
                    }
                } break;
                default: {
                    LOG(ERROR) << "AttachedEvent handleEvent unknown type: " << type;
                }
            }
        }
    }

    return 1; // keep on going
}