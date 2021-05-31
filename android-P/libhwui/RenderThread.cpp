// 	@	frameworks/base/libs/hwui/renderthread/RenderThread.cpp

RenderThread& RenderThread::getInstance() {
    // This is a pointer because otherwise __cxa_finalize
    // will try to delete it like a Good Citizen but that causes us to crash
    // because we don't want to delete the RenderThread normally.
    static RenderThread* sInstance = new RenderThread();
    gHasRenderThreadInstance = true;
    return *sInstance;
}

RenderThread::RenderThread()
        : ThreadBase()
        , mVsyncSource(nullptr)
        , mVsyncRequested(false)
        , mFrameCallbackTaskPending(false)
        , mRenderState(nullptr)
        , mEglManager(nullptr)
        , mVkManager(nullptr) {
    Properties::load();
    start("RenderThread");
}


bool RenderThread::threadLoop() {
	// 
    setpriority(PRIO_PROCESS, 0, PRIORITY_DISPLAY);
    if (gOnStartHook) {
        gOnStartHook();
    }
    initThreadLocals();

    while (true) {
        waitForWork();
        //	
        processQueue();

        if (mPendingRegistrationFrameCallbacks.size() && !mFrameCallbackTaskPending) {
            drainDisplayEventQueue();
            mFrameCallbacks.insert(mPendingRegistrationFrameCallbacks.begin(),
                                   mPendingRegistrationFrameCallbacks.end());
            mPendingRegistrationFrameCallbacks.clear();
            requestVsync();
        }

        if (!mFrameCallbackTaskPending && !mVsyncRequested && mFrameCallbacks.size()) {
            // TODO: Clean this up. This is working around an issue where a combination
            // of bad timing and slow drawing can result in dropping a stale vsync
            // on the floor (correct!) but fails to schedule to listen for the
            // next vsync (oops), so none of the callbacks are run.
            requestVsync();
        }
    }

    return false;
}

void ThreadBase::waitForWork() {
    nsecs_t nextWakeup;
    {
        std::unique_lock lock{mLock};
        nextWakeup = mQueue.nextWakeup(lock);
    }
    int timeout = -1;
    if (nextWakeup < std::numeric_limits<nsecs_t>::max()) {
        timeout = ns2ms(nextWakeup - WorkQueue::clock::now());
        if (timeout < 0) timeout = 0;
    }
    int result = mLooper->pollOnce(timeout);
    LOG_ALWAYS_FATAL_IF(result == Looper::POLL_ERROR, "RenderThread Looper POLL_ERROR!");
}


void ThreadBase::processQueue() { mQueue.process(); }