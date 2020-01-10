


// --- MediaLogNotifier ---
// Thread in charge of notifying MediaLogService to start merging.
// Receives requests from AudioFlinger's binder activity. It is used to reduce the amount of
// binder calls to MediaLogService in case of bursts of AudioFlinger binder calls.
class MediaLogNotifier : public Thread {

}

/**
 * @    frameworks/av/services/audioflinger/AudioFlinger.cpp
 * 
*/

// ----------------------------------------------------------------------------
AudioFlinger::MediaLogNotifier::MediaLogNotifier()
    : mPendingRequests(false) {}



/**
 * AudioFlinger::PlaybackThread::threadLoop()
 * --> mAudioFlinger->requestLogMerge();
 * ----> mMediaLogNotifier->requestMerge();
*/
void AudioFlinger::MediaLogNotifier::requestMerge() {
    AutoMutex _l(mMutex);
    mPendingRequests = true;
    mCond.signal();
}


/**
 * 在 AudioFlinger 构造函数中启动线程
*/
bool AudioFlinger::MediaLogNotifier::threadLoop() {
    // Should already have been checked, but just in case
    if (sMediaLogService == 0) {
        return false;
    }
    // Wait until there are pending requests
    {
        AutoMutex _l(mMutex);
        mPendingRequests = false; // to ignore past requests
        while (!mPendingRequests) {
            mCond.wait(mMutex);
            // TODO may also need an exitPending check
        }
        mPendingRequests = false;
    }
    // Execute the actual MediaLogService binder call and ignore extra requests for a while
    sMediaLogService->requestMergeWakeup();
    usleep(kPostTriggerSleepPeriod);
    return true;
}



