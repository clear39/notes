//  @   frameworks/native/services/surfaceflinger/MessageQueue.cpp

/**
 * 
 * 这里 impl::MessageQueue
 * 
 * 
 * void SurfaceFlinger::onFirstRef()
 * --> mEventQueue->init(this);
*/
void MessageQueue::init(const sp<SurfaceFlinger>& flinger) {
    mFlinger = flinger;
    mLooper = new Looper(true);
    mHandler = new Handler(*this);
}

/**
 * 
 * 这里 impl::MessageQueue
 * 
 * 
 * SurfaceFlinger::init()
 * --> mEventQueue->setEventThread(mSFEventThread.get());
*/
void MessageQueue::setEventThread(android::EventThread* eventThread) {
    if (mEventThread == eventThread) {
        return;
    }
    /**
     * gui::BitTube mEventTube;
    */
    if (mEventTube.getFd() >= 0) {
        mLooper->removeFd(mEventTube.getFd());
    }

    mEventThread = eventThread;
    /**
     * 
    */
    mEvents = eventThread->createEventConnection();
    /**
     * 
    */
    mEvents->stealReceiveChannel(&mEventTube);
    mLooper->addFd(mEventTube.getFd(), 0, Looper::EVENT_INPUT, MessageQueue::cb_eventReceiver,this);
}


int MessageQueue::cb_eventReceiver(int fd, int events, void* data) {
    MessageQueue* queue = reinterpret_cast<MessageQueue*>(data);
    return queue->eventReceiver(fd, events);
}

int MessageQueue::eventReceiver(int /*fd*/, int /*events*/) {
    ssize_t n;
    DisplayEventReceiver::Event buffer[8];
    /**
     * 这里 的 DisplayEventReceiver为gui封装下的
    */
    while ((n = DisplayEventReceiver::getEvents(&mEventTube, buffer, 8)) > 0) {
        for (int i = 0; i < n; i++) {
            if (buffer[i].header.type == DisplayEventReceiver::DISPLAY_EVENT_VSYNC) {
                mHandler->dispatchInvalidate();
                break;
            }
        }
    }
    return 1;
}

//  @   frameworks/native/libs/gui/DisplayEventReceiver.cpp
ssize_t DisplayEventReceiver::getEvents(gui::BitTube* dataChannel,Event* events, size_t count)
{
    return gui::BitTube::recvObjects(dataChannel, events, count);
}

//  @   frameworks/native/libs/gui/BitTube.cpp
ssize_t BitTube::recvObjects(BitTube* tube, void* events, size_t count, size_t objSize) {
    char* vaddr = reinterpret_cast<char*>(events);
    ssize_t size = tube->read(vaddr, count * objSize);

    // should never happen because of SOCK_SEQPACKET
    LOG_ALWAYS_FATAL_IF((size >= 0) && (size % static_cast<ssize_t>(objSize)),
                        "BitTube::recvObjects(count=%zu, size=%zu), res=%zd (partial events were " "received!)",count, objSize, size);

    // ALOGE_IF(size<0, "error %d receiving %d events", size, count);
    return size < 0 ? size : size / static_cast<ssize_t>(objSize);
}

ssize_t BitTube::read(void* vaddr, size_t size) {
    ssize_t err, len;
    do {
        len = ::recv(mReceiveFd, vaddr, size, MSG_DONTWAIT);
        err = len < 0 ? errno : 0;
    } while (err == EINTR);
    if (err == EAGAIN || err == EWOULDBLOCK) {
        // EAGAIN means that we have non-blocking I/O but there was no data to be read. Nothing the
        // client should care about.
        return 0;
    }
    return err == 0 ? len : -err;
}