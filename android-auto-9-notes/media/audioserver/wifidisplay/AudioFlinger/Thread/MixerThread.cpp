










void AudioFlinger::PlaybackThread::setStreamVolume(audio_stream_type_t stream, float value)
{
    Mutex::Autolock _l(mLock);
    mStreamTypes[stream].volume = value;
    broadcast_l();
}

void AudioFlinger::ThreadBase::broadcast_l()
{
    // Thread could be blocked waiting for async
    // so signal it to handle state changes immediately
    // If threadLoop is currently unlocked a signal of mWaitWorkCV will
    // be lost so we also flag to prevent it blocking on mWaitWorkCV
    mSignalPending = true;
    mWaitWorkCV.broadcast();
}







//MixerThread::threadLoop 分析////////////////////////////////////////////////////////////////////////////////////
/**
 * 由于MixerThread 继承 PlaybackThread
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
            for (size_t i = 0; i < effectChains.size(); i ++) {
                effectChains[i]->process_l();
            }
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
