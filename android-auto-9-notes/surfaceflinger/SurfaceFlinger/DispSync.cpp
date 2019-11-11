
//  @   frameworks/native/services/surfaceflinger/DispSync.cpp
/**
 * SurfaceFlinger::SurfaceFlinger()
 * -->SurfaceFlinger::SurfaceFlinger(SurfaceFlinger::SkipInitializationTag)
 * ---> mPrimaryDispSync("PrimaryDispSync")
*/
DispSync::DispSync(const char* name)
      : mName(name), mRefreshSkipCount(0), mThread(new DispSyncThread(name)) {}


/////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * SurfaceFlinger::SurfaceFlinger()
 * --> mPrimaryDispSync.init(SurfaceFlinger::hasSyncFramework, SurfaceFlinger::dispSyncPresentTimeOffset);
*/
void DispSync::init(bool hasSyncFramework /* = true*/, int64_t dispSyncPresentTimeOffset /*= 0*/) {
    mIgnorePresentFences = !hasSyncFramework;  // false
    mPresentTimeOffset = dispSyncPresentTimeOffset;  // 0
    /**
     * system/core/libutils/include/utils/ThreadDefs.h:59:    PRIORITY_URGENT_DISPLAY = ANDROID_PRIORITY_URGENT_DISPLAY,
     * system/core/libsystem/include/system/thread_defs.h:56:    ANDROID_PRIORITY_URGENT_DISPLAY =  HAL_PRIORITY_URGENT_DISPLAY,
     * system/core/libsystem/include/system/graphics.h:58:#define HAL_PRIORITY_URGENT_DISPLAY     (-8)
     * 
     * system/core/libsystem/include/system/thread_defs.h:72:    ANDROID_PRIORITY_MORE_FAVORABLE = -1,
    */
    mThread->run("DispSync", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);  // -9

    // set DispSync to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(mThread->getTid(), SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO for DispSyncThread");
    }

    reset();
    beginResync();

    /*
    frameworks/native/services/surfaceflinger/DispSync.cpp:46:static const bool kTraceDetailedInfo = false;
    */
    if (kTraceDetailedInfo) {
        // If we're not getting present fences then the ZeroPhaseTracer
        // would prevent HW vsync event from ever being turned off.
        // Even if we're just ignoring the fences, the zero-phase tracing is
        // not needed because any time there is an event registered we will
        // turn on the HW vsync events.
        if (!mIgnorePresentFences && kEnableZeroPhaseTracer) {
            mZeroPhaseTracer = std::make_unique<ZeroPhaseTracer>();
            addEventListener("ZeroPhaseTracer", 0, mZeroPhaseTracer.get());
        }
    }
}


void DispSync::reset() {
    Mutex::Autolock lock(mMutex);

    mPhase = 0;
    mReferenceTime = 0;
    mModelUpdated = false;
    mNumResyncSamples = 0;
    mFirstResyncSample = 0;
    mNumResyncSamplesSincePresent = 0;
    resetErrorLocked();
}

void DispSync::resetErrorLocked() {
    mPresentSampleOffset = 0;
    mError = 0;
    mZeroErrSamplesCount = 0;
    /**
     * frameworks/native/services/surfaceflinger/DispSync.h:137:    enum { NUM_PRESENT_SAMPLES = 8 };
    */
    for (size_t i = 0; i < NUM_PRESENT_SAMPLES; i++) {
        mPresentFences[i] = FenceTime::NO_FENCE;
    }
}


void DispSync::beginResync() {
    Mutex::Autolock lock(mMutex);
    ALOGV("[%s] beginResync", mName);
    mModelUpdated = false;
    mNumResyncSamples = 0;
}