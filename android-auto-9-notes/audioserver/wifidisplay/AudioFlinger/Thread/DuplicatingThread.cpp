//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audioflinger/Threads.cpp
/**
 * **/
AudioFlinger::DuplicatingThread::DuplicatingThread(const sp<AudioFlinger>& audioFlinger,AudioFlinger::MixerThread* mainThread, audio_io_handle_t id, bool systemReady)
    :   MixerThread(audioFlinger, mainThread->getOutput(), id, mainThread->outDevice(),systemReady, DUPLICATING),
        mWaitTimeMs(UINT_MAX)
{
    ALOGV("addOutputTrack() mainThread %p", mainThread);
    addOutputTrack(mainThread);
}


void AudioFlinger::DuplicatingThread::addOutputTrack(MixerThread *thread)
{
    Mutex::Autolock _l(mLock);
    // The downstream MixerThread consumes thread->frameCount() amount of frames per mix pass.
    // Adjust for thread->sampleRate() to determine minimum buffer frame count.
    // Then triple buffer because Threads do not run synchronously and may not be clock locked.
    const size_t frameCount = 3 * sourceFramesNeeded(mSampleRate, thread->frameCount(), thread->sampleRate());
    // TODO: Consider asynchronous sample rate conversion to handle clock disparity
    // from different OutputTracks and their associated MixerThreads (e.g. one may
    // nearly empty and the other may be dropping data).

    sp<OutputTrack> outputTrack = new OutputTrack(thread,
                                            this,
                                            mSampleRate,
                                            mFormat,
                                            mChannelMask,
                                            frameCount,
                                            IPCThreadState::self()->getCallingUid());

    status_t status = outputTrack != 0 ? outputTrack->initCheck() : (status_t) NO_MEMORY;
    if (status != NO_ERROR) {
        ALOGE("addOutputTrack() initCheck failed %d", status);
        return;
    }

    /**
     * 这里会唤醒线程thread执行
    */
    thread->setStreamVolume(AUDIO_STREAM_PATCH, 1.0f);

    /**
     * 
     * 
    */
    mOutputTracks.add(outputTrack);

    ALOGV("addOutputTrack() track %p, on thread %p", outputTrack.get(), thread);

    updateWaitTime_l();
}


// caller must hold mLock
void AudioFlinger::DuplicatingThread::updateWaitTime_l()
{
    mWaitTimeMs = UINT_MAX;
    for (size_t i = 0; i < mOutputTracks.size(); i++) {
        sp<ThreadBase> strong = mOutputTracks[i]->thread().promote();
        if (strong != 0) {
            uint32_t waitTimeMs = (strong->frameCount() * 2 * 1000) / strong->sampleRate();
            if (waitTimeMs < mWaitTimeMs) {
                mWaitTimeMs = waitTimeMs;
            }
        }
    }
}

/**
 * 这里会在 DuplicatingThread::cacheParameters_l --> MixerThread::cacheParameters_l  ---> PlaybackThread::cacheParameters_l
*/
uint32_t AudioFlinger::DuplicatingThread::activeSleepTimeUs() const {   return (mWaitTimeMs * 1000) / 2;      }

/**
 * 没有看见使用
*/
uint32_t AudioFlinger::DuplicatingThread::waitTimeMs() const { return mWaitTimeMs; }










//DuplicatingThread::threadLoop 分析////////////////////////////////////////////////////////////////////////////////////
/**
 * 由于DuplicatingThread 继承 MixerThread ,
 * 而且MixerThread 继承 PlaybackThread
*/
bool AudioFlinger::PlaybackThread::threadLoop()
{
    tlNBLogWriter = mNBLogWriter.get();

    Vector< sp<Track> > tracksToRemove;

    mStandbyTimeNs = systemTime();
    nsecs_t lastWriteFinished = -1; // time last server write completed
    int64_t lastFramesWritten = -1; // track changes in timestamp server frames written

    // MIXER
    nsecs_t lastWarning = 0;

    // DUPLICATING
    // FIXME could this be made local to while loop?
    writeFrames = 0;

    cacheParameters_l();
    mSleepTimeUs = mIdleSleepTimeUs;

    if (mType == MIXER) {
        sleepTimeShift = 0;
    }

    CpuStats cpuStats;
    const String8 myName(String8::format("thread %p type %s TID %d", this, threadTypeToString(mType), gettid()));

    acquireWakeLock();

    // mNBLogWriter logging APIs can only be called by a single thread, typically the
    // thread associated with this PlaybackThread.
    // If you want to share the mNBLogWriter with other threads (for example, binder threads)
    // then all such threads must agree to hold a common mutex before logging.
    // So if you need to log when mutex is unlocked, set logString to a non-NULL string,
    // and then that string will be logged at the next convenient opportunity.
    // See reference to logString below.
    const char *logString = NULL;

    // Estimated time for next buffer to be written to hal. This is used only on
    // suspended mode (for now) to help schedule the wait time until next iteration.
    nsecs_t timeLoopNextNs = 0;

    /**
     * void AudioFlinger::PlaybackThread::checkSilentMode_l()
     * 如果 为 AUDIO_DEVICE_OUT_REMOTE_SUBMIX 线程 不做任何处理
     * 目前 ro.audio.silent 系统属性默认为 空，同样不做任何处理
    */
    checkSilentMode_l();

    while (!exitPending())
    {
        ALOGVV("%s threadLoop()",myName.c_str());
        // Log merge requests are performed during AudioFlinger binder transactions, but
        // that does not cover audio playback. It's requested here for that reason.
        mAudioFlinger->requestLogMerge();

        cpuStats.sample(myName);

        Vector< sp<EffectChain> > effectChains;

        { // scope for mLock

            Mutex::Autolock _l(mLock);

            processConfigEvents_l();

            // See comment at declaration of logString for why this is done under mLock
            if (logString != NULL) {
                mNBLogWriter->logTimestamp();
                mNBLogWriter->log(logString);
                logString = NULL;
            }

            // Gather the framesReleased counters for all active tracks,
            // and associate with the sink frames written out.  We need
            // this to convert the sink timestamp to the track timestamp.
            bool kernelLocationUpdate = false;
            /**
             * 
             * 
             * **/
            if (mNormalSink != 0) {
                // Note: The DuplicatingThread may not have a mNormalSink.
                // We always fetch the timestamp here because often the downstream
                // sink will block while writing.
                ExtendedTimestamp timestamp; // use private copy to fetch
                (void) mNormalSink->getTimestamp(timestamp);

                // We keep track of the last valid kernel position in case we are in underrun
                // and the normal mixer period is the same as the fast mixer period, or there
                // is some error from the HAL.
                if (mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] >= 0) {
                    mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL_LASTKERNELOK] =
                            mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL];
                    mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL_LASTKERNELOK] =
                            mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL];

                    mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER_LASTKERNELOK] =
                            mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER];
                    mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER_LASTKERNELOK] =
                            mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER];
                }

                if (timestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] >= 0) {
                    kernelLocationUpdate = true;
                } else {
                    ALOGVV("getTimestamp error - no valid kernel position");
                }

                // copy over kernel info
                mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL] =
                        timestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL]
                        + mSuspendedFrames; // add frames discarded when suspended
                mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] =
                        timestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL];
            }
            // mFramesWritten for non-offloaded tracks are contiguous
            // even after standby() is called. This is useful for the track frame
            // to sink frame mapping.
            bool serverLocationUpdate = false;
            if (mFramesWritten != lastFramesWritten) {
                serverLocationUpdate = true;
                lastFramesWritten = mFramesWritten;
            }
            // Only update timestamps if there is a meaningful change.
            // Either the kernel timestamp must be valid or we have written something.
            if (kernelLocationUpdate || serverLocationUpdate) {
                if (serverLocationUpdate) {
                    // use the time before we called the HAL write - it is a bit more accurate
                    // to when the server last read data than the current time here.
                    //
                    // If we haven't written anything, mLastWriteTime will be -1
                    // and we use systemTime().
                    mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER] = mFramesWritten;
                    mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER] = mLastWriteTime == -1
                            ? systemTime() : mLastWriteTime;
                }

                for (const sp<Track> &t : mActiveTracks) {
                    if (!t->isFastTrack()) {
                        t->updateTrackFrameInfo(
                                t->mAudioTrackServerProxy->framesReleased(),
                                mFramesWritten,
                                mTimestamp);
                    }
                }
            }
            /**
             * void AudioFlinger::DuplicatingThread::saveOutputTracks() { outputTracks = mOutputTracks; }
            */
            saveOutputTracks();
            if (mSignalPending) {
                // A signal was raised while we were unlocked
                mSignalPending = false;
            } else if (waitingAsyncCallback_l()) {
                if (exitPending()) {
                    break;
                }
                bool released = false;
                if (!keepWakeLock()) {
                    releaseWakeLock_l();
                    released = true;
                }

                const int64_t waitNs = computeWaitTimeNs_l();
                ALOGV("wait async completion (wait time: %lld)", (long long)waitNs);
                status_t status = mWaitWorkCV.waitRelative(mLock, waitNs);
                if (status == TIMED_OUT) {
                    mSignalPending = true; // if timeout recheck everything
                }
                ALOGV("async completion/wake");
                if (released) {
                    acquireWakeLock_l();
                }
                mStandbyTimeNs = systemTime() + mStandbyDelayNs;
                mSleepTimeUs = 0;

                continue;
            }
            if ((!mActiveTracks.size() && systemTime() > mStandbyTimeNs) || isSuspended()) {
                // put audio hardware into standby after short delay
                if (shouldStandby_l()) {

                    threadLoop_standby();

                    // This is where we go into standby
                    if (!mStandby) {
                        LOG_AUDIO_STATE();
                    }
                    mStandby = true;
                }

                if (!mActiveTracks.size() && mConfigEvents.isEmpty()) {
                    // we're about to wait, flush the binder command buffer
                    IPCThreadState::self()->flushCommands();
                    /**
                     * 
                    */
                    clearOutputTracks();

                    if (exitPending()) {
                        break;
                    }

                    releaseWakeLock_l();
                    // wait until we have something to do...
                    ALOGV("%s going to sleep", myName.string());
                    mWaitWorkCV.wait(mLock);
                    ALOGV("%s waking up", myName.string());
                    acquireWakeLock_l();

                    mMixerStatus = MIXER_IDLE;
                    mMixerStatusIgnoringFastTracks = MIXER_IDLE;
                    mBytesWritten = 0;
                    mBytesRemaining = 0;
                    checkSilentMode_l();

                    mStandbyTimeNs = systemTime() + mStandbyDelayNs;
                    mSleepTimeUs = mIdleSleepTimeUs;
                    if (mType == MIXER) {
                        sleepTimeShift = 0;
                    }

                    continue;
                }
            }
            // mMixerStatusIgnoringFastTracks is also updated internally
            mMixerStatus = prepareTracks_l(&tracksToRemove);

            mActiveTracks.updatePowerState(this);

            updateMetadata_l();

            // prevent any changes in effect chain list and in each effect chain
            // during mixing and effect process as the audio buffers could be deleted
            // or modified if an effect is created or deleted
            lockEffectChains_l(effectChains);
        } // mLock scope ends

        ALOGVV("%s threadLoop() mBytesRemaining:%zu,mMixerStatus:%d",myName.c_str(),mBytesRemaining,mMixerStatus);

        /*  
            //  @   frameworks/av/services/audioflinger/Threads.h
            enum mixer_state {
                MIXER_IDLE,             // no active tracks
                MIXER_TRACKS_ENABLED,   // at least one active track, but no track has any data ready
                MIXER_TRACKS_READY,      // at least one active track, and at least one track has data                                                                                                                
                MIXER_DRAIN_TRACK,      // drain currently playing track
                MIXER_DRAIN_ALL,        // fully drain the hardware
                // standby mode does not have an enum value
                // suspend by audio policy manager is orthogonal to mixer state
            };  
         * 
        */
        if (mBytesRemaining == 0) {
            mCurrentWriteLength = 0;
            if (mMixerStatus == MIXER_TRACKS_READY) { // 2
                // threadLoop_mix() sets mCurrentWriteLength
                threadLoop_mix();
            } else if ((mMixerStatus != MIXER_DRAIN_TRACK)
                        && (mMixerStatus != MIXER_DRAIN_ALL)) {
                // threadLoop_sleepTime sets mSleepTimeUs to 0 if data
                // must be written to HAL
                threadLoop_sleepTime();
                if (mSleepTimeUs == 0) {
                    mCurrentWriteLength = mSinkBufferSize;
                }
            }
            // Either threadLoop_mix() or threadLoop_sleepTime() should have set
            // mMixerBuffer with data if mMixerBufferValid is true and mSleepTimeUs == 0.
            // Merge mMixerBuffer data into mEffectBuffer (if any effects are valid)
            // or mSinkBuffer (if there are no effects).
            //
            // This is done pre-effects computation; if effects change to
            // support higher precision, this needs to move.
            //
            // mMixerBufferValid is only set true by MixerThread::prepareTracks_l().
            // TODO use mSleepTimeUs == 0 as an additional condition.
            if (mMixerBufferValid) {
                void *buffer = mEffectBufferValid ? mEffectBuffer : mSinkBuffer;
                audio_format_t format = mEffectBufferValid ? mEffectBufferFormat : mFormat;

                // mono blend occurs for mixer threads only (not direct or offloaded)
                // and is handled here if we're going directly to the sink.
                if (requireMonoBlend() && !mEffectBufferValid) {
                    mono_blend(mMixerBuffer, mMixerBufferFormat, mChannelCount, mNormalFrameCount,
                               true /*limit*/);
                }

                memcpy_by_audio_format(buffer, format, mMixerBuffer, mMixerBufferFormat,
                        mNormalFrameCount * mChannelCount);
            }

            mBytesRemaining = mCurrentWriteLength;
            if (isSuspended()) {
                // Simulate write to HAL when suspended (e.g. BT SCO phone call).
                mSleepTimeUs = suspendSleepTimeUs(); // assumes full buffer.
                const size_t framesRemaining = mBytesRemaining / mFrameSize;
                mBytesWritten += mBytesRemaining;
                mFramesWritten += framesRemaining;
                mSuspendedFrames += framesRemaining; // to adjust kernel HAL position
                mBytesRemaining = 0;
            }

            // only process effects if we're going to write
            if (mSleepTimeUs == 0 && mType != OFFLOAD) {
                for (size_t i = 0; i < effectChains.size(); i ++) {
                    effectChains[i]->process_l();
                }
            }
        }
        // Process effect chains for offloaded thread even if no audio
        // was read from audio track: process only updates effect state
        // and thus does have to be synchronized with audio writes but may have
        // to be called while waiting for async write callback
        if (mType == OFFLOAD) {
            .......
        }

        // Only if the Effects buffer is enabled and there is data in the
        // Effects buffer (buffer valid), we need to
        // copy into the sink buffer.
        // TODO use mSleepTimeUs == 0 as an additional condition.
        if (mEffectBufferValid) {
            //ALOGV("writing effect buffer to sink buffer format %#x", mFormat);

            if (requireMonoBlend()) {
                mono_blend(mEffectBuffer, mEffectBufferFormat, mChannelCount, mNormalFrameCount,
                           true /*limit*/);
            }

            memcpy_by_audio_format(mSinkBuffer, mFormat, mEffectBuffer, mEffectBufferFormat,
                    mNormalFrameCount * mChannelCount);
        }

        // enable changes in effect chain
        unlockEffectChains(effectChains);

        if (!waitingAsyncCallback()) {
            ALOGVV("%s threadLoop() mSleepTimeUs:%d mMixerStatus:%d",myName.c_str(),mSleepTimeUs,mMixerStatus);
            // mSleepTimeUs == 0 means we must write to audio hardware
            if (mSleepTimeUs == 0) {
                ssize_t ret = 0;
                // We save lastWriteFinished here, as previousLastWriteFinished,
                // for throttling. On thread start, previousLastWriteFinished will be
                // set to -1, which properly results in no throttling after the first write.
                nsecs_t previousLastWriteFinished = lastWriteFinished;
                nsecs_t delta = 0;
                if (mBytesRemaining) {
                    // FIXME rewrite to reduce number of system calls
                    mLastWriteTime = systemTime();  // also used for dumpsys
                    ret = threadLoop_write();
                    lastWriteFinished = systemTime();
                    delta = lastWriteFinished - mLastWriteTime;
                    if (ret < 0) {
                        mBytesRemaining = 0;
                    } else {
                        mBytesWritten += ret;
                        mBytesRemaining -= ret;
                        mFramesWritten += ret / mFrameSize;
                    }
                } else if ((mMixerStatus == MIXER_DRAIN_TRACK) ||
                        (mMixerStatus == MIXER_DRAIN_ALL)) {
                    threadLoop_drain();
                }
                if (mType == MIXER && !mStandby) {
                    // write blocked detection
                    if (delta > maxPeriod) {
                        mNumDelayedWrites++;
                        if ((lastWriteFinished - lastWarning) > kWarningThrottleNs) {
                            ATRACE_NAME("underrun");
                            ALOGW("write blocked for %llu msecs, %d delayed writes, thread %p",
                                    (unsigned long long) ns2ms(delta), mNumDelayedWrites, this);
                            lastWarning = lastWriteFinished;
                        }
                    }

                    if (mThreadThrottle
                            && mMixerStatus == MIXER_TRACKS_READY // we are mixing (active tracks)
                            && ret > 0) {                         // we wrote something
                        // Limit MixerThread data processing to no more than twice the
                        // expected processing rate.
                        //
                        // This helps prevent underruns with NuPlayer and other applications
                        // which may set up buffers that are close to the minimum size, or use
                        // deep buffers, and rely on a double-buffering sleep strategy to fill.
                        //
                        // The throttle smooths out sudden large data drains from the device,
                        // e.g. when it comes out of standby, which often causes problems with
                        // (1) mixer threads without a fast mixer (which has its own warm-up)
                        // (2) minimum buffer sized tracks (even if the track is full,
                        //     the app won't fill fast enough to handle the sudden draw).
                        //
                        // Total time spent in last processing cycle equals time spent in
                        // 1. threadLoop_write, as well as time spent in
                        // 2. threadLoop_mix (significant for heavy mixing, especially
                        //                    on low tier processors)

                        // it's OK if deltaMs (and deltaNs) is an overestimate.
                        nsecs_t deltaNs;
                        // deltaNs = lastWriteFinished - previousLastWriteFinished;
                        __builtin_sub_overflow(
                            lastWriteFinished,previousLastWriteFinished, &deltaNs);
                        const int32_t deltaMs = deltaNs / 1000000;

                        const int32_t throttleMs = (int32_t)mHalfBufferMs - deltaMs;
                        if ((signed)mHalfBufferMs >= throttleMs && throttleMs > 0) {
                            usleep(throttleMs * 1000);
                            // notify of throttle start on verbose log
                            ALOGV_IF(mThreadThrottleEndMs == mThreadThrottleTimeMs,
                                    "mixer(%p) throttle begin:"
                                    " ret(%zd) deltaMs(%d) requires sleep %d ms",
                                    this, ret, deltaMs, throttleMs);
                            mThreadThrottleTimeMs += throttleMs;
                            // Throttle must be attributed to the previous mixer loop's write time
                            // to allow back-to-back throttling.
                            lastWriteFinished += throttleMs * 1000000;
                        } else {
                            uint32_t diff = mThreadThrottleTimeMs - mThreadThrottleEndMs;
                            if (diff > 0) {
                                // notify of throttle end on debug log
                                // but prevent spamming for bluetooth
                                ALOGD_IF(!audio_is_a2dp_out_device(outDevice()) &&
                                         !audio_is_hearing_aid_out_device(outDevice()),
                                        "mixer(%p) throttle end: throttle time(%u)", this, diff);
                                mThreadThrottleEndMs = mThreadThrottleTimeMs;
                            }
                        }
                    }
                }

            } else {
                ATRACE_BEGIN("sleep");
                Mutex::Autolock _l(mLock);
                // suspended requires accurate metering of sleep time.
                if (isSuspended()) {
                    // advance by expected sleepTime
                    timeLoopNextNs += microseconds((nsecs_t)mSleepTimeUs);
                    const nsecs_t nowNs = systemTime();

                    // compute expected next time vs current time.
                    // (negative deltas are treated as delays).
                    nsecs_t deltaNs = timeLoopNextNs - nowNs;
                    if (deltaNs < -kMaxNextBufferDelayNs) {
                        // Delays longer than the max allowed trigger a reset.
                        ALOGV("DelayNs: %lld, resetting timeLoopNextNs", (long long) deltaNs);
                        deltaNs = microseconds((nsecs_t)mSleepTimeUs);
                        timeLoopNextNs = nowNs + deltaNs;
                    } else if (deltaNs < 0) {
                        // Delays within the max delay allowed: zero the delta/sleepTime
                        // to help the system catch up in the next iteration(s)
                        ALOGV("DelayNs: %lld, catching-up", (long long) deltaNs);
                        deltaNs = 0;
                    }
                    // update sleep time (which is >= 0)
                    mSleepTimeUs = deltaNs / 1000;
                }
                if (!mSignalPending && mConfigEvents.isEmpty() && !exitPending()) {
                    mWaitWorkCV.waitRelative(mLock, microseconds((nsecs_t)mSleepTimeUs));
                }
                ATRACE_END();
            }
        }

        // Finally let go of removed track(s), without the lock held
        // since we can't guarantee the destructors won't acquire that
        // same lock.  This will also mutate and push a new fast mixer state.
        threadLoop_removeTracks(tracksToRemove);
        tracksToRemove.clear();

        // FIXME I don't understand the need for this here;
        //       it was in the original code but maybe the
        //       assignment in saveOutputTracks() makes this unnecessary?
        clearOutputTracks();

        // Effect chains will be actually deleted here if they were removed from
        // mEffectChains list during mixing or effects processing
        effectChains.clear();

        // FIXME Note that the above .clear() is no longer necessary since effectChains
        // is now local to this block, but will keep it for now (at least until merge done).
    }

    threadLoop_exit();

    if (!mStandby) {
        threadLoop_standby();
        mStandby = true;
    }

    releaseWakeLock();

    ALOGV("Thread %p type %d exiting", myName.c_str(), mType);
    return false;
}


// prepareTracks_l() must be called with ThreadBase::mLock held
AudioFlinger::PlaybackThread::mixer_state AudioFlinger::MixerThread::prepareTracks_l(
        Vector< sp<Track> > *tracksToRemove)
{
    // clean up deleted track names in AudioMixer before allocating new tracks
    (void)mTracks.processDeletedTrackNames([this](int name) {
        // for each name, destroy it in the AudioMixer
        if (mAudioMixer->exists(name)) {
            mAudioMixer->destroy(name);
        }
    });
    mTracks.clearDeletedTrackNames();

    mixer_state mixerStatus = MIXER_IDLE;
    // find out which tracks need to be processed
    size_t count = mActiveTracks.size();
    size_t mixedTracks = 0;
    size_t tracksWithEffect = 0;
    // counts only _active_ fast tracks
    size_t fastTracks = 0;
    uint32_t resetMask = 0; // bit mask of fast tracks that need to be reset

    float masterVolume = mMasterVolume;
    bool masterMute = mMasterMute;

    if (masterMute) {
        masterVolume = 0;
    }
    // Delegate master volume control to effect in output mix effect chain if needed
    sp<EffectChain> chain = getEffectChain_l(AUDIO_SESSION_OUTPUT_MIX);
    if (chain != 0) {
        uint32_t v = (uint32_t)(masterVolume * (1 << 24));
        chain->setVolume_l(&v, &v);
        masterVolume = (float)((v + (1 << 23)) >> 24);
        chain.clear();
    }

    // prepare a new state to push
    FastMixerStateQueue *sq = NULL;
    FastMixerState *state = NULL;
    bool didModify = false;
    FastMixerStateQueue::block_t block = FastMixerStateQueue::BLOCK_UNTIL_PUSHED;
    bool coldIdle = false;
    if (mFastMixer != 0) {
        sq = mFastMixer->sq();
        state = sq->begin();
        coldIdle = state->mCommand == FastMixerState::COLD_IDLE;
    }

    mMixerBufferValid = false;  // mMixerBuffer has no valid data until appropriate tracks found.
    mEffectBufferValid = false; // mEffectBuffer has no valid data until tracks found.

    for (size_t i=0 ; i<count ; i++) {
        const sp<Track> t = mActiveTracks[i];

        // this const just means the local variable doesn't change
        Track* const track = t.get();

        // process fast tracks
        if (track->isFastTrack()) {

            // It's theoretically possible (though unlikely) for a fast track to be created
            // and then removed within the same normal mix cycle.  This is not a problem, as
            // the track never becomes active so it's fast mixer slot is never touched.
            // The converse, of removing an (active) track and then creating a new track
            // at the identical fast mixer slot within the same normal mix cycle,
            // is impossible because the slot isn't marked available until the end of each cycle.
            int j = track->mFastIndex;
            ALOG_ASSERT(0 < j && j < (int)FastMixerState::sMaxFastTracks);
            ALOG_ASSERT(!(mFastTrackAvailMask & (1 << j)));
            FastTrack *fastTrack = &state->mFastTracks[j];

            // Determine whether the track is currently in underrun condition,
            // and whether it had a recent underrun.
            FastTrackDump *ftDump = &mFastMixerDumpState.mTracks[j];
            FastTrackUnderruns underruns = ftDump->mUnderruns;
            uint32_t recentFull = (underruns.mBitFields.mFull -
                    track->mObservedUnderruns.mBitFields.mFull) & UNDERRUN_MASK;
            uint32_t recentPartial = (underruns.mBitFields.mPartial -
                    track->mObservedUnderruns.mBitFields.mPartial) & UNDERRUN_MASK;
            uint32_t recentEmpty = (underruns.mBitFields.mEmpty -
                    track->mObservedUnderruns.mBitFields.mEmpty) & UNDERRUN_MASK;
            uint32_t recentUnderruns = recentPartial + recentEmpty;
            track->mObservedUnderruns = underruns;
            // don't count underruns that occur while stopping or pausing
            // or stopped which can occur when flush() is called while active
            if (!(track->isStopping() || track->isPausing() || track->isStopped()) &&
                    recentUnderruns > 0) {
                // FIXME fast mixer will pull & mix partial buffers, but we count as a full underrun
                track->mAudioTrackServerProxy->tallyUnderrunFrames(recentUnderruns * mFrameCount);
            } else {
                track->mAudioTrackServerProxy->tallyUnderrunFrames(0);
            }

            // This is similar to the state machine for normal tracks,
            // with a few modifications for fast tracks.
            bool isActive = true;
            switch (track->mState) {
            case TrackBase::STOPPING_1:
                // track stays active in STOPPING_1 state until first underrun
                if (recentUnderruns > 0 || track->isTerminated()) {
                    track->mState = TrackBase::STOPPING_2;
                }
                break;
            case TrackBase::PAUSING:
                // ramp down is not yet implemented
                track->setPaused();
                break;
            case TrackBase::RESUMING:
                // ramp up is not yet implemented
                track->mState = TrackBase::ACTIVE;
                break;
            case TrackBase::ACTIVE:
                if (recentFull > 0 || recentPartial > 0) {
                    // track has provided at least some frames recently: reset retry count
                    track->mRetryCount = kMaxTrackRetries;
                }
                if (recentUnderruns == 0) {
                    // no recent underruns: stay active
                    break;
                }
                // there has recently been an underrun of some kind
                if (track->sharedBuffer() == 0) {
                    // were any of the recent underruns "empty" (no frames available)?
                    if (recentEmpty == 0) {
                        // no, then ignore the partial underruns as they are allowed indefinitely
                        break;
                    }
                    // there has recently been an "empty" underrun: decrement the retry counter
                    if (--(track->mRetryCount) > 0) {
                        break;
                    }
                    // indicate to client process that the track was disabled because of underrun;
                    // it will then automatically call start() when data is available
                    track->disable();
                    // remove from active list, but state remains ACTIVE [confusing but true]
                    isActive = false;
                    break;
                }
                // fall through
            case TrackBase::STOPPING_2:
            case TrackBase::PAUSED:
            case TrackBase::STOPPED:
            case TrackBase::FLUSHED:   // flush() while active
                // Check for presentation complete if track is inactive
                // We have consumed all the buffers of this track.
                // This would be incomplete if we auto-paused on underrun
                {
                    uint32_t latency = 0;
                    status_t result = mOutput->stream->getLatency(&latency);
                    ALOGE_IF(result != OK,
                            "Error when retrieving output stream latency: %d", result);
                    size_t audioHALFrames = (latency * mSampleRate) / 1000;
                    int64_t framesWritten = mBytesWritten / mFrameSize;
                    if (!(mStandby || track->presentationComplete(framesWritten, audioHALFrames))) {
                        // track stays in active list until presentation is complete
                        break;
                    }
                }
                if (track->isStopping_2()) {
                    track->mState = TrackBase::STOPPED;
                }
                if (track->isStopped()) {
                    // Can't reset directly, as fast mixer is still polling this track
                    //   track->reset();
                    // So instead mark this track as needing to be reset after push with ack
                    resetMask |= 1 << i;
                }
                isActive = false;
                break;
            case TrackBase::IDLE:
            default:
                LOG_ALWAYS_FATAL("unexpected track state %d", track->mState);
            }

            if (isActive) {
                // was it previously inactive?
                if (!(state->mTrackMask & (1 << j))) {
                    ExtendedAudioBufferProvider *eabp = track;
                    VolumeProvider *vp = track;
                    fastTrack->mBufferProvider = eabp;
                    fastTrack->mVolumeProvider = vp;
                    fastTrack->mChannelMask = track->mChannelMask;
                    fastTrack->mFormat = track->mFormat;
                    fastTrack->mGeneration++;
                    state->mTrackMask |= 1 << j;
                    didModify = true;
                    // no acknowledgement required for newly active tracks
                }
                sp<AudioTrackServerProxy> proxy = track->mAudioTrackServerProxy;
                // cache the combined master volume and stream type volume for fast mixer; this
                // lacks any synchronization or barrier so VolumeProvider may read a stale value
                const float vh = track->getVolumeHandler()->getVolume(
                        proxy->framesReleased()).first;
                float volume = masterVolume
                        * mStreamTypes[track->streamType()].volume
                        * vh;
                track->mCachedVolume = volume;
                gain_minifloat_packed_t vlr = proxy->getVolumeLR();
                float vlf = volume * float_from_gain(gain_minifloat_unpack_left(vlr));
                float vrf = volume * float_from_gain(gain_minifloat_unpack_right(vlr));
                track->setFinalVolume((vlf + vrf) / 2.f);
                ++fastTracks;
            } else {
                // was it previously active?
                if (state->mTrackMask & (1 << j)) {
                    fastTrack->mBufferProvider = NULL;
                    fastTrack->mGeneration++;
                    state->mTrackMask &= ~(1 << j);
                    didModify = true;
                    // If any fast tracks were removed, we must wait for acknowledgement
                    // because we're about to decrement the last sp<> on those tracks.
                    block = FastMixerStateQueue::BLOCK_UNTIL_ACKED;
                } else {
                    // ALOGW rather than LOG_ALWAYS_FATAL because it seems there are cases where an
                    // AudioTrack may start (which may not be with a start() but with a write()
                    // after underrun) and immediately paused or released.  In that case the
                    // FastTrack state hasn't had time to update.
                    // TODO Remove the ALOGW when this theory is confirmed.
                    ALOGW("fast track %d should have been active; "
                            "mState=%d, mTrackMask=%#x, recentUnderruns=%u, isShared=%d",
                            j, track->mState, state->mTrackMask, recentUnderruns,
                            track->sharedBuffer() != 0);
                    // Since the FastMixer state already has the track inactive, do nothing here.
                }
                tracksToRemove->add(track);
                // Avoids a misleading display in dumpsys
                track->mObservedUnderruns.mBitFields.mMostRecent = UNDERRUN_FULL;
            }
            continue;
        }

        {   // local variable scope to avoid goto warning

        audio_track_cblk_t* cblk = track->cblk();

        // The first time a track is added we wait
        // for all its buffers to be filled before processing it
        int name = track->name();

        // if an active track doesn't exist in the AudioMixer, create it.
        if (!mAudioMixer->exists(name)) {
            status_t status = mAudioMixer->create(
                    name,
                    track->mChannelMask,
                    track->mFormat,
                    track->mSessionId);
            if (status != OK) {
                ALOGW("%s: cannot create track name"
                        " %d, mask %#x, format %#x, sessionId %d in AudioMixer",
                        __func__, name, track->mChannelMask, track->mFormat, track->mSessionId);
                tracksToRemove->add(track);
                track->invalidate(); // consider it dead.
                continue;
            }
        }

        // make sure that we have enough frames to mix one full buffer.
        // enforce this condition only once to enable draining the buffer in case the client
        // app does not call stop() and relies on underrun to stop:
        // hence the test on (mMixerStatus == MIXER_TRACKS_READY) meaning the track was mixed
        // during last round
        size_t desiredFrames;
        const uint32_t sampleRate = track->mAudioTrackServerProxy->getSampleRate();
        AudioPlaybackRate playbackRate = track->mAudioTrackServerProxy->getPlaybackRate();

        desiredFrames = sourceFramesNeededWithTimestretch(
                sampleRate, mNormalFrameCount, mSampleRate, playbackRate.mSpeed);
        // TODO: ONLY USED FOR LEGACY RESAMPLERS, remove when they are removed.
        // add frames already consumed but not yet released by the resampler
        // because mAudioTrackServerProxy->framesReady() will include these frames
        desiredFrames += mAudioMixer->getUnreleasedFrames(track->name());

        uint32_t minFrames = 1;
        if ((track->sharedBuffer() == 0) && !track->isStopped() && !track->isPausing() &&
                (mMixerStatusIgnoringFastTracks == MIXER_TRACKS_READY)) {
            minFrames = desiredFrames;
        }

        size_t framesReady = track->framesReady();
        if (ATRACE_ENABLED()) {
            // I wish we had formatted trace names
            std::string traceName("nRdy");
            traceName += std::to_string(track->name());
            ATRACE_INT(traceName.c_str(), framesReady);
        }
        if ((framesReady >= minFrames) && track->isReady() &&
                !track->isPaused() && !track->isTerminated())
        {
            ALOGVV("track %d s=%08x [OK] on thread %p", name, cblk->mServer, this);

            mixedTracks++;

            // track->mainBuffer() != mSinkBuffer or mMixerBuffer means
            // there is an effect chain connected to the track
            chain.clear();
            if (track->mainBuffer() != mSinkBuffer &&
                    track->mainBuffer() != mMixerBuffer) {
                if (mEffectBufferEnabled) {
                    mEffectBufferValid = true; // Later can set directly.
                }
                chain = getEffectChain_l(track->sessionId());
                // Delegate volume control to effect in track effect chain if needed
                if (chain != 0) {
                    tracksWithEffect++;
                } else {
                    ALOGW("prepareTracks_l(): track %d attached to effect but no chain found on "
                            "session %d",
                            name, track->sessionId());
                }
            }


            int param = AudioMixer::VOLUME;
            if (track->mFillingUpStatus == Track::FS_FILLED) {
                // no ramp for the first volume setting
                track->mFillingUpStatus = Track::FS_ACTIVE;
                if (track->mState == TrackBase::RESUMING) {
                    track->mState = TrackBase::ACTIVE;
                    param = AudioMixer::RAMP_VOLUME;
                }
                mAudioMixer->setParameter(name, AudioMixer::RESAMPLE, AudioMixer::RESET, NULL);
                mLeftVolFloat = -1.0;
            // FIXME should not make a decision based on mServer
            } else if (cblk->mServer != 0) {
                // If the track is stopped before the first frame was mixed,
                // do not apply ramp
                param = AudioMixer::RAMP_VOLUME;
            }

            // compute volume for this track
            uint32_t vl, vr;       // in U8.24 integer format
            float vlf, vrf, vaf;   // in [0.0, 1.0] float format
            // read original volumes with volume control
            float typeVolume = mStreamTypes[track->streamType()].volume;
            float v = masterVolume * typeVolume;

            if (track->isPausing() || mStreamTypes[track->streamType()].mute) {
                vl = vr = 0;
                vlf = vrf = vaf = 0.;
                if (track->isPausing()) {
                    track->setPaused();
                }
            } else {
                sp<AudioTrackServerProxy> proxy = track->mAudioTrackServerProxy;
                gain_minifloat_packed_t vlr = proxy->getVolumeLR();
                vlf = float_from_gain(gain_minifloat_unpack_left(vlr));
                vrf = float_from_gain(gain_minifloat_unpack_right(vlr));
                // track volumes come from shared memory, so can't be trusted and must be clamped
                if (vlf > GAIN_FLOAT_UNITY) {
                    ALOGV("Track left volume out of range: %.3g", vlf);
                    vlf = GAIN_FLOAT_UNITY;
                }
                if (vrf > GAIN_FLOAT_UNITY) {
                    ALOGV("Track right volume out of range: %.3g", vrf);
                    vrf = GAIN_FLOAT_UNITY;
                }
                const float vh = track->getVolumeHandler()->getVolume(
                        track->mAudioTrackServerProxy->framesReleased()).first;
                // now apply the master volume and stream type volume and shaper volume
                vlf *= v * vh;
                vrf *= v * vh;
                // assuming master volume and stream type volume each go up to 1.0,
                // then derive vl and vr as U8.24 versions for the effect chain
                const float scaleto8_24 = MAX_GAIN_INT * MAX_GAIN_INT;
                vl = (uint32_t) (scaleto8_24 * vlf);
                vr = (uint32_t) (scaleto8_24 * vrf);
                // vl and vr are now in U8.24 format
                uint16_t sendLevel = proxy->getSendLevel_U4_12();
                // send level comes from shared memory and so may be corrupt
                if (sendLevel > MAX_GAIN_INT) {
                    ALOGV("Track send level out of range: %04X", sendLevel);
                    sendLevel = MAX_GAIN_INT;
                }
                // vaf is represented as [0.0, 1.0] float by rescaling sendLevel
                vaf = v * sendLevel * (1. / MAX_GAIN_INT);
            }

            track->setFinalVolume((vrf + vlf) / 2.f);

            // Delegate volume control to effect in track effect chain if needed
            if (chain != 0 && chain->setVolume_l(&vl, &vr)) {
                // Do not ramp volume if volume is controlled by effect
                param = AudioMixer::VOLUME;
                // Update remaining floating point volume levels
                vlf = (float)vl / (1 << 24);
                vrf = (float)vr / (1 << 24);
                track->mHasVolumeController = true;
            } else {
                // force no volume ramp when volume controller was just disabled or removed
                // from effect chain to avoid volume spike
                if (track->mHasVolumeController) {
                    param = AudioMixer::VOLUME;
                }
                track->mHasVolumeController = false;
            }

            // For dedicated VoIP outputs, let the HAL apply the stream volume. Track volume is
            // still applied by the mixer.
            if ((mOutput->flags & AUDIO_OUTPUT_FLAG_VOIP_RX) != 0) {
                v = mStreamTypes[track->streamType()].mute ? 0.0f : v;
                if (v != mLeftVolFloat) {
                    status_t result = mOutput->stream->setVolume(v, v);
                    ALOGE_IF(result != OK, "Error when setting output stream volume: %d", result);
                    if (result == OK) {
                        mLeftVolFloat = v;
                    }
                }
                // if stream volume was successfully sent to the HAL, mLeftVolFloat == v here and we
                // remove stream volume contribution from software volume.
                if (v != 0.0f && mLeftVolFloat == v) {
                   vlf = min(1.0f, vlf / v);
                   vrf = min(1.0f, vrf / v);
                   vaf = min(1.0f, vaf / v);
               }
            }
            // XXX: these things DON'T need to be done each time
            mAudioMixer->setBufferProvider(name, track);
            mAudioMixer->enable(name);

            mAudioMixer->setParameter(name, param, AudioMixer::VOLUME0, &vlf);
            mAudioMixer->setParameter(name, param, AudioMixer::VOLUME1, &vrf);
            mAudioMixer->setParameter(name, param, AudioMixer::AUXLEVEL, &vaf);
            mAudioMixer->setParameter(
                name,
                AudioMixer::TRACK,
                AudioMixer::FORMAT, (void *)track->format());
            mAudioMixer->setParameter(
                name,
                AudioMixer::TRACK,
                AudioMixer::CHANNEL_MASK, (void *)(uintptr_t)track->channelMask());
            mAudioMixer->setParameter(
                name,
                AudioMixer::TRACK,
                AudioMixer::MIXER_CHANNEL_MASK, (void *)(uintptr_t)mChannelMask);
            // limit track sample rate to 2 x output sample rate, which changes at re-configuration
            uint32_t maxSampleRate = mSampleRate * AUDIO_RESAMPLER_DOWN_RATIO_MAX;
            uint32_t reqSampleRate = track->mAudioTrackServerProxy->getSampleRate();
            if (reqSampleRate == 0) {
                reqSampleRate = mSampleRate;
            } else if (reqSampleRate > maxSampleRate) {
                reqSampleRate = maxSampleRate;
            }
            mAudioMixer->setParameter(
                name,
                AudioMixer::RESAMPLE,
                AudioMixer::SAMPLE_RATE,
                (void *)(uintptr_t)reqSampleRate);

            AudioPlaybackRate playbackRate = track->mAudioTrackServerProxy->getPlaybackRate();
            mAudioMixer->setParameter(
                name,
                AudioMixer::TIMESTRETCH,
                AudioMixer::PLAYBACK_RATE,
                &playbackRate);

            /*
             * Select the appropriate output buffer for the track.
             *
             * Tracks with effects go into their own effects chain buffer
             * and from there into either mEffectBuffer or mSinkBuffer.
             *
             * Other tracks can use mMixerBuffer for higher precision
             * channel accumulation.  If this buffer is enabled
             * (mMixerBufferEnabled true), then selected tracks will accumulate
             * into it.
             *
             */
            if (mMixerBufferEnabled
                    && (track->mainBuffer() == mSinkBuffer
                            || track->mainBuffer() == mMixerBuffer)) {
                mAudioMixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::MIXER_FORMAT, (void *)mMixerBufferFormat);
                mAudioMixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::MAIN_BUFFER, (void *)mMixerBuffer);
                // TODO: override track->mainBuffer()?
                mMixerBufferValid = true;
            } else {
                mAudioMixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::MIXER_FORMAT, (void *)EFFECT_BUFFER_FORMAT);
                mAudioMixer->setParameter(
                        name,
                        AudioMixer::TRACK,
                        AudioMixer::MAIN_BUFFER, (void *)track->mainBuffer());
            }
            mAudioMixer->setParameter(
                name,
                AudioMixer::TRACK,
                AudioMixer::AUX_BUFFER, (void *)track->auxBuffer());

            // reset retry count
            track->mRetryCount = kMaxTrackRetries;

            // If one track is ready, set the mixer ready if:
            //  - the mixer was not ready during previous round OR
            //  - no other track is not ready
            if (mMixerStatusIgnoringFastTracks != MIXER_TRACKS_READY ||
                    mixerStatus != MIXER_TRACKS_ENABLED) {
                mixerStatus = MIXER_TRACKS_READY;
            }
        } else {
            if (framesReady < desiredFrames && !track->isStopped() && !track->isPaused()) {
                ALOGV("track(%p) underrun,  framesReady(%zu) < framesDesired(%zd)",
                        track, framesReady, desiredFrames);
                track->mAudioTrackServerProxy->tallyUnderrunFrames(desiredFrames);
            } else {
                track->mAudioTrackServerProxy->tallyUnderrunFrames(0);
            }

            // clear effect chain input buffer if an active track underruns to avoid sending
            // previous audio buffer again to effects
            chain = getEffectChain_l(track->sessionId());
            if (chain != 0) {
                chain->clearInputBuffer();
            }

            ALOGVV("track %d s=%08x [NOT READY] on thread %p", name, cblk->mServer, this);
            if ((track->sharedBuffer() != 0) || track->isTerminated() ||
                    track->isStopped() || track->isPaused()) {
                // We have consumed all the buffers of this track.
                // Remove it from the list of active tracks.
                // TODO: use actual buffer filling status instead of latency when available from
                // audio HAL
                size_t audioHALFrames = (latency_l() * mSampleRate) / 1000;
                int64_t framesWritten = mBytesWritten / mFrameSize;
                if (mStandby || track->presentationComplete(framesWritten, audioHALFrames)) {
                    if (track->isStopped()) {
                        track->reset();
                    }
                    tracksToRemove->add(track);
                }
            } else {
                // No buffers for this track. Give it a few chances to
                // fill a buffer, then remove it from active list.
                if (--(track->mRetryCount) <= 0) {
                    ALOGI("BUFFER TIMEOUT: remove(%d) from active list on thread %p", name, this);
                    tracksToRemove->add(track);
                    // indicate to client process that the track was disabled because of underrun;
                    // it will then automatically call start() when data is available
                    track->disable();
                // If one track is not ready, mark the mixer also not ready if:
                //  - the mixer was ready during previous round OR
                //  - no other track is ready
                } else if (mMixerStatusIgnoringFastTracks == MIXER_TRACKS_READY ||
                                mixerStatus != MIXER_TRACKS_READY) {
                    mixerStatus = MIXER_TRACKS_ENABLED;
                }
            }
            mAudioMixer->disable(name);
        }

        }   // local variable scope to avoid goto warning

    }

    // Push the new FastMixer state if necessary
    bool pauseAudioWatchdog = false;
    if (didModify) {
        state->mFastTracksGen++;
        // if the fast mixer was active, but now there are no fast tracks, then put it in cold idle
        if (kUseFastMixer == FastMixer_Dynamic &&
                state->mCommand == FastMixerState::MIX_WRITE && state->mTrackMask <= 1) {
            state->mCommand = FastMixerState::COLD_IDLE;
            state->mColdFutexAddr = &mFastMixerFutex;
            state->mColdGen++;
            mFastMixerFutex = 0;
            if (kUseFastMixer == FastMixer_Dynamic) {
                mNormalSink = mOutputSink;
            }
            // If we go into cold idle, need to wait for acknowledgement
            // so that fast mixer stops doing I/O.
            block = FastMixerStateQueue::BLOCK_UNTIL_ACKED;
            pauseAudioWatchdog = true;
        }
    }
    if (sq != NULL) {
        sq->end(didModify);
        // No need to block if the FastMixer is in COLD_IDLE as the FastThread
        // is not active. (We BLOCK_UNTIL_ACKED when entering COLD_IDLE
        // when bringing the output sink into standby.)
        //
        // We will get the latest FastMixer state when we come out of COLD_IDLE.
        //
        // This occurs with BT suspend when we idle the FastMixer with
        // active tracks, which may be added or removed.
        sq->push(coldIdle ? FastMixerStateQueue::BLOCK_NEVER : block);
    }
#ifdef AUDIO_WATCHDOG
    if (pauseAudioWatchdog && mAudioWatchdog != 0) {
        mAudioWatchdog->pause();
    }
#endif

    // Now perform the deferred reset on fast tracks that have stopped
    while (resetMask != 0) {
        size_t i = __builtin_ctz(resetMask);
        ALOG_ASSERT(i < count);
        resetMask &= ~(1 << i);
        sp<Track> track = mActiveTracks[i];
        ALOG_ASSERT(track->isFastTrack() && track->isStopped());
        track->reset();
    }

    // Track destruction may occur outside of threadLoop once it is removed from active tracks.
    // Ensure the AudioMixer doesn't have a raw "buffer provider" pointer to the track if
    // it ceases to be active, to allow safe removal from the AudioMixer at the start
    // of prepareTracks_l(); this releases any outstanding buffer back to the track.
    // See also the implementation of destroyTrack_l().
    for (const auto &track : *tracksToRemove) {
        const int name = track->name();
        if (mAudioMixer->exists(name)) { // Normal tracks here, fast tracks in FastMixer.
            mAudioMixer->setBufferProvider(name, nullptr /* bufferProvider */);
        }
    }

    // remove all the tracks that need to be...
    removeTracks_l(*tracksToRemove);

    if (getEffectChain_l(AUDIO_SESSION_OUTPUT_MIX) != 0) {
        mEffectBufferValid = true;
    }

    if (mEffectBufferValid) {
        // as long as there are effects we should clear the effects buffer, to avoid
        // passing a non-clean buffer to the effect chain
        memset(mEffectBuffer, 0, mEffectBufferSize);
    }
    // sink or mix buffer must be cleared if all tracks are connected to an
    // effect chain as in this case the mixer will not write to the sink or mix buffer
    // and track effects will accumulate into it
    if ((mBytesRemaining == 0) && ((mixedTracks != 0 && mixedTracks == tracksWithEffect) ||
            (mixedTracks == 0 && fastTracks > 0))) {
        // FIXME as a performance optimization, should remember previous zero status
        if (mMixerBufferValid) {
            memset(mMixerBuffer, 0, mMixerBufferSize);
            // TODO: In testing, mSinkBuffer below need not be cleared because
            // the PlaybackThread::threadLoop() copies mMixerBuffer into mSinkBuffer
            // after mixing.
            //
            // To enforce this guarantee:
            // ((mixedTracks != 0 && mixedTracks == tracksWithEffect) ||
            // (mixedTracks == 0 && fastTracks > 0))
            // must imply MIXER_TRACKS_READY.
            // Later, we may clear buffers regardless, and skip much of this logic.
        }
        // FIXME as a performance optimization, should remember previous zero status
        memset(mSinkBuffer, 0, mNormalFrameCount * mFrameSize);
    }

    // if any fast tracks, then status is ready
    mMixerStatusIgnoringFastTracks = mixerStatus;
    if (fastTracks > 0) {
        mixerStatus = MIXER_TRACKS_READY;
    }
    return mixerStatus;
}



void AudioFlinger::DuplicatingThread::threadLoop_mix()
{
    // mix buffers...
    if (outputsReady(outputTracks)) {
        mAudioMixer->process();
    } else {
        if (mMixerBufferValid) {
            memset(mMixerBuffer, 0, mMixerBufferSize);
        } else {
            memset(mSinkBuffer, 0, mSinkBufferSize);
        }
    }
    mSleepTimeUs = 0;
    writeFrames = mNormalFrameCount;
    mCurrentWriteLength = mSinkBufferSize;
    mStandbyTimeNs = systemTime() + mStandbyDelayNs;
}


bool AudioFlinger::DuplicatingThread::outputsReady(const SortedVector< sp<OutputTrack> > &outputTracks)
{
    for (size_t i = 0; i < outputTracks.size(); i++) {
        sp<ThreadBase> thread = outputTracks[i]->thread().promote();
        if (thread == 0) {
            ALOGW("DuplicatingThread::outputsReady() could not promote thread on output track %p",
                    outputTracks[i].get());
            return false;
        }
        PlaybackThread *playbackThread = (PlaybackThread *)thread.get();
        // see note at standby() declaration
        if (playbackThread->standby() && !playbackThread->isSuspended()) {
            ALOGV("DuplicatingThread output track %p on thread %p Not Ready", outputTracks[i].get(),
                    thread.get());
            return false;
        }
    }
    return true;
}
















//////////////////////////////////////////////////////////////////////////////////////////////////
void AudioFlinger::PlaybackThread::ioConfigChanged(audio_io_config_event event, pid_t pid) {
    sp<AudioIoDescriptor> desc = new AudioIoDescriptor();
    ALOGV("PlaybackThread::ioConfigChanged, thread %p, event %d", this, event);

    desc->mIoHandle = mId;

    switch (event) {
    case AUDIO_OUTPUT_OPENED:      //  这里
    case AUDIO_OUTPUT_REGISTERED:
    case AUDIO_OUTPUT_CONFIG_CHANGED:
        desc->mPatch = mPatch;
        desc->mChannelMask = mChannelMask;
        desc->mSamplingRate = mSampleRate;
        desc->mFormat = mFormat;
        desc->mFrameCount = mNormalFrameCount; // FIXME see  AudioFlinger::frameCount(audio_io_handle_t)
        desc->mFrameCountHAL = mFrameCount;
        desc->mLatency = latency_l();
        break;

    case AUDIO_OUTPUT_CLOSED:
    default:
        break;
    }
    mAudioFlinger->ioConfigChanged(event, desc, pid);
}


ssize_t AudioFlinger::DuplicatingThread::threadLoop_write()
{
    ALOGV("DuplicatingThread::threadLoop_write()");
    for (size_t i = 0; i < outputTracks.size(); i++) {
        outputTracks[i]->write(mSinkBuffer, writeFrames);
    }
    mStandby = false;
    return (ssize_t)mSinkBufferSize;
}
