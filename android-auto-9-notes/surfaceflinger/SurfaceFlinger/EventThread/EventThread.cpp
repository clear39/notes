//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/surfaceflinger/EventThread.h
class EventThread : public android::EventThread, private VSyncSource::Callback {

}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/surfaceflinger/EventThread.cpp
EventThread::EventThread(VSyncSource* src, ResyncWithRateLimitCallback resyncWithRateLimitCallback,
                         InterceptVSyncsCallback interceptVSyncsCallback, const char* threadName)
      : mVSyncSource(src),
        mResyncWithRateLimitCallback(resyncWithRateLimitCallback),
        mInterceptVSyncsCallback(interceptVSyncsCallback) {

    for (auto& event : mVSyncEvent) {
        event.header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
        event.header.id = 0;
        event.header.timestamp = 0;
        event.vsync.count = 0;
    }

    mThread = std::thread(&EventThread::threadMain, this);

    pthread_setname_np(mThread.native_handle(), threadName);

    pid_t tid = pthread_gettid_np(mThread.native_handle());

    // Use SCHED_FIFO to minimize jitter
    constexpr int EVENT_THREAD_PRIORITY = 2;
    struct sched_param param = {0};
    param.sched_priority = EVENT_THREAD_PRIORITY;
    if (pthread_setschedparam(mThread.native_handle(), SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO for EventThread");
    }

    set_sched_policy(tid, SP_FOREGROUND);
}


/** 
 * SurfaceFlinger::init()
 * --> mEventQueue->setEventThread(mSFEventThread.get());
 * ---> mEvents = eventThread->createEventConnection();
*/
sp<BnDisplayEventConnection> EventThread::createEventConnection() const {
    return new Connection(const_cast<EventThread*>(this));
}

/** 
 * void EventThread::Connection::onFirstRef()
 * --> mEventThread->registerDisplayEventConnection(this);
*/
status_t EventThread::registerDisplayEventConnection( const sp<EventThread::Connection>& connection) {
    std::lock_guard<std::mutex> lock(mMutex);
    /**
     * 
    */
    mDisplayEventConnections.add(connection);
    mCondition.notify_all();
    return NO_ERROR;
}



void EventThread::threadMain() NO_THREAD_SAFETY_ANALYSIS {
    std::unique_lock<std::mutex> lock(mMutex);
    while (mKeepRunning) {
        DisplayEventReceiver::Event event;
        Vector<sp<EventThread::Connection> > signalConnections;
        signalConnections = waitForEventLocked(&lock, &event);

        // dispatch events to listeners...
        const size_t count = signalConnections.size();
        for (size_t i = 0; i < count; i++) {
            const sp<Connection>& conn(signalConnections[i]);
            // now see if we still need to report this event
            status_t err = conn->postEvent(event);
            if (err == -EAGAIN || err == -EWOULDBLOCK) {
                // The destination doesn't accept events anymore, it's probably
                // full. For now, we just drop the events on the floor.
                // FIXME: Note that some events cannot be dropped and would have
                // to be re-sent later.
                // Right-now we don't have the ability to do this.
                ALOGW("EventThread: dropping event (%08x) for connection %p", event.header.type,conn.get());
            } else if (err < 0) {
                // handle any other error on the pipe as fatal. the only
                // reasonable thing to do is to clean-up this connection.
                // The most common error we'll get here is -EPIPE.
                removeDisplayEventConnectionLocked(signalConnections[i]);
            }
        }
    }
}

// This will return when (1) a vsync event has been received, and (2) there was
// at least one connection interested in receiving it when we started waiting.
Vector<sp<EventThread::Connection> > EventThread::waitForEventLocked(std::unique_lock<std::mutex>* lock, DisplayEventReceiver::Event* event) {
    Vector<sp<EventThread::Connection> > signalConnections;

    while (signalConnections.isEmpty() && mKeepRunning) {
        bool eventPending = false;
        bool waitForVSync = false;

        size_t vsyncCount = 0;
        nsecs_t timestamp = 0;
        for (int32_t i = 0; i < DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES; i++) {
            timestamp = mVSyncEvent[i].header.timestamp;
            if (timestamp) {
                // we have a vsync event to dispatch
                if (mInterceptVSyncsCallback) {
                    mInterceptVSyncsCallback(timestamp);
                }
                *event = mVSyncEvent[i];
                mVSyncEvent[i].header.timestamp = 0;
                vsyncCount = mVSyncEvent[i].vsync.count;
                break;
            }
        }

        if (!timestamp) {
            // no vsync event, see if there are some other event
            eventPending = !mPendingEvents.isEmpty();
            if (eventPending) {
                // we have some other event to dispatch
                *event = mPendingEvents[0];
                mPendingEvents.removeAt(0);
            }
        }

        // find out connections waiting for events
        size_t count = mDisplayEventConnections.size();
        for (size_t i = 0; i < count;) {
            sp<Connection> connection(mDisplayEventConnections[i].promote());
            if (connection != nullptr) {
                bool added = false;
                if (connection->count >= 0) {
                    // we need vsync events because at least
                    // one connection is waiting for it
                    waitForVSync = true;
                    if (timestamp) {
                        // we consume the event only if it's time
                        // (ie: we received a vsync event)
                        if (connection->count == 0) {
                            // fired this time around
                            connection->count = -1;
                            signalConnections.add(connection);
                            added = true;
                        } else if (connection->count == 1 ||
                                   (vsyncCount % connection->count) == 0) {
                            // continuous event, and time to report it
                            signalConnections.add(connection);
                            added = true;
                        }
                    }
                }

                if (eventPending && !timestamp && !added) {
                    // we don't have a vsync event to process
                    // (timestamp==0), but we have some pending
                    // messages.
                    signalConnections.add(connection);
                }
                ++i;
            } else {
                // we couldn't promote this reference, the connection has
                // died, so clean-up!
                mDisplayEventConnections.removeAt(i);
                --count;
            }
        }

        // Here we figure out if we need to enable or disable vsyncs
        if (timestamp && !waitForVSync) {
            // we received a VSYNC but we have no clients
            // don't report it, and disable VSYNC events
            disableVSyncLocked();
        } else if (!timestamp && waitForVSync) {
            // we have at least one client, so we want vsync enabled
            // (TODO: this function is called right after we finish
            // notifying clients of a vsync, so this call will be made
            // at the vsync rate, e.g. 60fps.  If we can accurately
            // track the current state we could avoid making this call
            // so often.)
            enableVSyncLocked();
        }

        // note: !timestamp implies signalConnections.isEmpty(), because we
        // don't populate signalConnections if there's no vsync pending
        if (!timestamp && !eventPending) {
            // wait for something to happen
            if (waitForVSync) {
                // This is where we spend most of our time, waiting
                // for vsync events and new client registrations.
                //
                // If the screen is off, we can't use h/w vsync, so we
                // use a 16ms timeout instead.  It doesn't need to be
                // precise, we just need to keep feeding our clients.
                //
                // We don't want to stall if there's a driver bug, so we
                // use a (long) timeout when waiting for h/w vsync, and
                // generate fake events when necessary.
                bool softwareSync = mUseSoftwareVSync;
                auto timeout = softwareSync ? 16ms : 1000ms;
                if (mCondition.wait_for(*lock, timeout) == std::cv_status::timeout) {
                    if (!softwareSync) {
                        ALOGW("Timed out waiting for hw vsync; faking it");
                    }
                    // FIXME: how do we decide which display id the fake
                    // vsync came from ?
                    mVSyncEvent[0].header.type = DisplayEventReceiver::DISPLAY_EVENT_VSYNC;
                    mVSyncEvent[0].header.id = DisplayDevice::DISPLAY_PRIMARY;
                    mVSyncEvent[0].header.timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
                    mVSyncEvent[0].vsync.count++;
                }
            } else {
                // Nobody is interested in vsync, so we just want to sleep.
                // h/w vsync should be disabled, so this will wait until we
                // get a new connection, or an existing connection becomes
                // interested in receiving vsync again.
                mCondition.wait(*lock);
            }
        }
    }

    // here we're guaranteed to have a timestamp and some connections to signal
    // (The connections might have dropped out of mDisplayEventConnections
    // while we were asleep, but we'll still have strong references to them.)
    return signalConnections;
}