
/**
 * 
*/ 
AudioFlinger::PlaybackThread::PlaybackThread(const sp<AudioFlinger>& audioFlinger,
                                             AudioStreamOut* output,
                                             audio_io_handle_t id,
                                             audio_devices_t device,
                                             type_t type,
                                             bool systemReady)
    :   ThreadBase(audioFlinger, id, device, AUDIO_DEVICE_NONE, type, systemReady),
        mNormalFrameCount(0), mSinkBuffer(NULL),
        /**
         *  @   frameworks/av/services/audioflinger/AudioFlinger.h:386:    
         *      static const bool kEnableExtendedPrecision = true;
         */
        mMixerBufferEnabled(AudioFlinger::kEnableExtendedPrecision), 
        mMixerBuffer(NULL),
        mMixerBufferSize(0),
        mMixerBufferFormat(AUDIO_FORMAT_INVALID),
        mMixerBufferValid(false),
        /**
         * frameworks/av/services/audioflinger/AudioFlinger.h:386:    
         * static const bool kEnableExtendedPrecision = true;
        */
        mEffectBufferEnabled(AudioFlinger::kEnableExtendedPrecision),
        mEffectBuffer(NULL),
        mEffectBufferSize(0),
        mEffectBufferFormat(AUDIO_FORMAT_INVALID),
        mEffectBufferValid(false),
        mSuspended(0), mBytesWritten(0),
        mFramesWritten(0),
        mSuspendedFrames(0),
        mActiveTracks(&this->mLocalLog),
        // mStreamTypes[] initialized in constructor body
        mTracks(type == MIXER),
        mOutput(output),
        mLastWriteTime(-1), mNumWrites(0), mNumDelayedWrites(0), mInWrite(false),
        mMixerStatus(MIXER_IDLE),
        mMixerStatusIgnoringFastTracks(MIXER_IDLE),
        mStandbyDelayNs(AudioFlinger::mStandbyTimeInNsecs),
        mBytesRemaining(0),
        mCurrentWriteLength(0),
        mUseAsyncWrite(false),
        mWriteAckSequence(0),
        mDrainSequence(0),
        mScreenState(AudioFlinger::mScreenState),
        // index 0 is reserved for normal mixer's submix
        mFastTrackAvailMask(((1 << FastMixerState::sMaxFastTracks) - 1) & ~1),
        mHwSupportsPause(false), mHwPaused(false), mFlushPending(false),
        mLeftVolFloat(-1.0), mRightVolFloat(-1.0)
{
    snprintf(mThreadName, kThreadNameLength, "AudioOut_%X", id);
    mNBLogWriter = audioFlinger->newWriter_l(kLogSize, mThreadName);

    // Assumes constructor is called by AudioFlinger with it's mLock held, but
    // it would be safer to explicitly pass initial masterVolume/masterMute as
    // parameter.
    //
    // If the HAL we are using has support for master volume or master mute,
    // then do not attenuate or mute during mixing (just leave the volume at 1.0
    // and the mute set to false).
    mMasterVolume = audioFlinger->masterVolume_l();
    mMasterMute = audioFlinger->masterMute_l();
    if (mOutput && mOutput->audioHwDev) {
        if (mOutput->audioHwDev->canSetMasterVolume()) {
            mMasterVolume = 1.0;
        }

        if (mOutput->audioHwDev->canSetMasterMute()) {
            mMasterMute = false;
        }
    }                   

    readOutputParameters_l();

    // ++ operator does not compile
    for (audio_stream_type_t stream = AUDIO_STREAM_MIN; stream < AUDIO_STREAM_FOR_POLICY_CNT;stream = (audio_stream_type_t) (stream + 1)) {
        mStreamTypes[stream].volume = 0.0f;
        mStreamTypes[stream].mute = mAudioFlinger->streamMute_l(stream);
    }
    // Audio patch volume is always max
    mStreamTypes[AUDIO_STREAM_PATCH].volume = 1.0f;
    mStreamTypes[AUDIO_STREAM_PATCH].mute = false;
}


void AudioFlinger::PlaybackThread::readOutputParameters_l()
{
    // unfortunately we have no way of recovering from errors here, hence the LOG_ALWAYS_FATAL
    mSampleRate = mOutput->getSampleRate();
    mChannelMask = mOutput->getChannelMask();

    if (!audio_is_output_channel(mChannelMask)) {
        LOG_ALWAYS_FATAL("HAL channel mask %#x not valid for output", mChannelMask);
    }

    if ((mType == MIXER || mType == DUPLICATING)  && !isValidPcmSinkChannelMask(mChannelMask)) {
        LOG_ALWAYS_FATAL("HAL channel mask %#x not supported for mixed output",mChannelMask);
    }
    mChannelCount = audio_channel_count_from_out_mask(mChannelMask);

    // Get actual HAL format.
    status_t result = mOutput->stream->getFormat(&mHALFormat);
    LOG_ALWAYS_FATAL_IF(result != OK, "Error when retrieving output stream format: %d", result);
    // Get format from the shim, which will be different than the HAL format
    // if playing compressed audio over HDMI passthrough.
    mFormat = mOutput->getFormat();
    if (!audio_is_valid_format(mFormat)) {
        LOG_ALWAYS_FATAL("HAL format %#x not valid for output", mFormat);
    }
    if ((mType == MIXER || mType == DUPLICATING)  && !isValidPcmSinkFormat(mFormat)) {
        LOG_FATAL("HAL format %#x not supported for mixed output",mFormat);
    }
    mFrameSize = mOutput->getFrameSize();
    result = mOutput->stream->getBufferSize(&mBufferSize);
    LOG_ALWAYS_FATAL_IF(result != OK,"Error when retrieving output stream buffer size: %d", result);
    mFrameCount = mBufferSize / mFrameSize;
    if (mFrameCount & 15) {
        ALOGW("HAL output buffer size is %zu frames but AudioMixer requires multiples of 16 frames",mFrameCount);
    }

    if (mOutput->flags & AUDIO_OUTPUT_FLAG_NON_BLOCKING) {
        if (mOutput->stream->setCallback(this) == OK) {
            mUseAsyncWrite = true;
            mCallbackThread = new AudioFlinger::AsyncCallbackThread(this);
        }
    }

    mHwSupportsPause = false;
    if (mOutput->flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        bool supportsPause = false, supportsResume = false;
        if (mOutput->stream->supportsPauseAndResume(&supportsPause, &supportsResume) == OK) {
            if (supportsPause && supportsResume) {
                mHwSupportsPause = true;
            } else if (supportsPause) {
                ALOGW("direct output implements pause but not resume");
            } else if (supportsResume) {
                ALOGW("direct output implements resume but not pause");
            }
        }
    }
    if (!mHwSupportsPause && mOutput->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) {
        LOG_ALWAYS_FATAL("HW_AV_SYNC requested but HAL does not implement pause and resume");
    }

    if (mType == DUPLICATING && mMixerBufferEnabled && mEffectBufferEnabled) {
        // For best precision, we use float instead of the associated output
        // device format (typically PCM 16 bit).
          //  system/media/audio/include/system/audio-base.h:146:    AUDIO_FORMAT_PCM_16_BIT            = 0x1u,        // (PCM | PCM_SUB_16_BIT)
          //  system/media/audio/include/system/audio-base.h:150:    AUDIO_FORMAT_PCM_FLOAT             = 0x5u,        // (PCM | PCM_SUB_FLOAT)
        mFormat = AUDIO_FORMAT_PCM_FLOAT;
        mFrameSize = mChannelCount * audio_bytes_per_sample(mFormat);
        mBufferSize = mFrameSize * mFrameCount;

        // TODO: We currently use the associated output device channel mask and sample rate.
        // (1) Perhaps use the ORed channel mask of all downstream MixerThreads
        // (if a valid mask) to avoid premature downmix.
        // (2) Perhaps use the maximum sample rate of all downstream MixerThreads
        // instead of the output device sample rate to avoid loss of high frequency information.
        // This may need to be updated as MixerThread/OutputTracks are added and not here.
    }

    // Calculate size of normal sink buffer relative to the HAL output buffer size
    double multiplier = 1.0;
    if (mType == MIXER && (kUseFastMixer == FastMixer_Static || kUseFastMixer == FastMixer_Dynamic)) {
        size_t minNormalFrameCount = (kMinNormalSinkBufferSizeMs * mSampleRate) / 1000;
        size_t maxNormalFrameCount = (kMaxNormalSinkBufferSizeMs * mSampleRate) / 1000;

        // round up minimum and round down maximum to nearest 16 frames to satisfy AudioMixer
        minNormalFrameCount = (minNormalFrameCount + 15) & ~15;
        maxNormalFrameCount = maxNormalFrameCount & ~15;
        if (maxNormalFrameCount < minNormalFrameCount) {
            maxNormalFrameCount = minNormalFrameCount;
        }
        multiplier = (double) minNormalFrameCount / (double) mFrameCount;
        if (multiplier <= 1.0) {
            multiplier = 1.0;
        } else if (multiplier <= 2.0) {
            if (2 * mFrameCount <= maxNormalFrameCount) {
                multiplier = 2.0;
            } else {
                multiplier = (double) maxNormalFrameCount / (double) mFrameCount;
            }
        } else {
            multiplier = floor(multiplier);
        }
    }
    mNormalFrameCount = multiplier * mFrameCount;
    // round up to nearest 16 frames to satisfy AudioMixer
    if (mType == MIXER || mType == DUPLICATING) {
        mNormalFrameCount = (mNormalFrameCount + 15) & ~15;
    }
    ALOGI("HAL output buffer size %zu frames, normal sink buffer size %zu frames", mFrameCount,mNormalFrameCount);

    // Check if we want to throttle the processing to no more than 2x normal rate
    mThreadThrottle = property_get_bool("af.thread.throttle", true /* default_value */);
    mThreadThrottleTimeMs = 0;
    mThreadThrottleEndMs = 0;
    mHalfBufferMs = mNormalFrameCount * 1000 / (2 * mSampleRate);

    // mSinkBuffer is the sink buffer.  Size is always multiple-of-16 frames.
    // Originally this was int16_t[] array, need to remove legacy implications.
    free(mSinkBuffer);
    mSinkBuffer = NULL;
    // For sink buffer size, we use the frame size from the downstream sink to avoid problems
    // with non PCM formats for compressed music, e.g. AAC, and Offload threads.
    const size_t sinkBufferSize = mNormalFrameCount * mFrameSize;



    /**
     * 注意这里创建缓存区
     * frameworks/av/services/audioflinger/Threads.h:812: 
     * void*                           mSinkBuffer;         // frame size aligned sink buffer
    */
    (void)posix_memalign(&mSinkBuffer, 32, sinkBufferSize);

    // We resize the mMixerBuffer according to the requirements of the sink buffer which drives the output.
    free(mMixerBuffer);
    mMixerBuffer = NULL;
    
    if (mMixerBufferEnabled) {// true
        //  system/media/audio/include/system/audio-base.h:150:    AUDIO_FORMAT_PCM_FLOAT             = 0x5u,        // (PCM | PCM_SUB_FLOAT)
        mMixerBufferFormat = AUDIO_FORMAT_PCM_FLOAT; // also valid: AUDIO_FORMAT_PCM_16_BIT.
        mMixerBufferSize = mNormalFrameCount * mChannelCount * audio_bytes_per_sample(mMixerBufferFormat);
        (void)posix_memalign(&mMixerBuffer, 32, mMixerBufferSize);
    }



    free(mEffectBuffer);
    mEffectBuffer = NULL;
    if (mEffectBufferEnabled) {// true，静态指定
        mEffectBufferFormat = EFFECT_BUFFER_FORMAT;
        mEffectBufferSize = mNormalFrameCount * mChannelCount * audio_bytes_per_sample(mEffectBufferFormat);
        (void)posix_memalign(&mEffectBuffer, 32, mEffectBufferSize);
    }

    // force reconfiguration of effect chains and engines to take new buffer size and audio
    // parameters into account
    // Note that mLock is not held when readOutputParameters_l() is called from the constructor
    // but in this case nothing is done below as no audio sessions have effect yet so it doesn't
    // matter.
    // create a copy of mEffectChains as calling moveEffectChain_l() can reorder some effect chains
    Vector< sp<EffectChain> > effectChains = mEffectChains;
    for (size_t i = 0; i < effectChains.size(); i ++) {
        mAudioFlinger->moveEffectChain_l(effectChains[i]->sessionId(), this, this, false);
    }
}



/**
 * status_t AudioFlinger::PlaybackThread::Track::start(AudioSystem::sync_event_t event = AudioSystem::SYNC_EVENT_NONE, audio_session_t triggerSession = AUDIO_SESSION_NONE)
 * 
*/
// addTrack_l() must be called with ThreadBase::mLock held
status_t AudioFlinger::PlaybackThread::addTrack_l(const sp<Track>& track)
{
    status_t status = ALREADY_EXISTS;

    if (mActiveTracks.indexOf(track) < 0) {
        // the track is newly added, make sure it fills up all its
        // buffers before playing. This is to ensure the client will
        // effectively get the latency it requested.
        if (track->isExternalTrack()) {//isExternalTrack  根据  track中的 mType 是否为  TYPE_OUTPUT 或者 TYPE_PATCH，正常多媒体播放都为 TYPE_DEFAULT
            TrackBase::track_state state = track->mState;
            mLock.unlock();
            status = AudioSystem::startOutput(mId, track->streamType(), track->sessionId());
            mLock.lock();
            // abort track was stopped/paused while we released the lock
            if (state != track->mState) {
                if (status == NO_ERROR) {
                    mLock.unlock();
                    AudioSystem::stopOutput(mId, track->streamType(),track->sessionId());
                    mLock.lock();
                }
                return INVALID_OPERATION;
            }
            // abort if start is rejected by audio policy manager
            if (status != NO_ERROR) {
                return PERMISSION_DENIED;
            }
#ifdef ADD_BATTERY_DATA
            // to track the speaker usage
            addBatteryData(IMediaPlayerService::kBatteryDataAudioFlingerStart);
#endif
        }

        // set retry count for buffer fill
        if (track->isOffloaded()) {
            if (track->isStopping_1()) {
                track->mRetryCount = kMaxTrackStopRetriesOffload;
            } else {
                track->mRetryCount = kMaxTrackStartupRetriesOffload;
            }
            track->mFillingUpStatus = mStandby ? Track::FS_FILLING : Track::FS_FILLED;
        } else {
            track->mRetryCount = kMaxTrackStartupRetries;
            track->mFillingUpStatus =  track->sharedBuffer() != 0 ? Track::FS_FILLED : Track::FS_FILLING;
        }

        track->mResetDone = false;
        track->mPresentationCompleteFrames = 0;
        /**
         * 
        */
        mActiveTracks.add(track);
        /***
         * 
         * 
        */
        sp<EffectChain> chain = getEffectChain_l(track->sessionId());
        if (chain != 0) {
            ALOGV("addTrack_l() starting track on chain %p for session %d", chain.get(), track->sessionId());
            chain->incActiveTrackCnt();
        }

        status = NO_ERROR;
    }

    onAddNewTrack_l();
    return status;
}

void AudioFlinger::PlaybackThread::onAddNewTrack_l()
{
    ALOGV("signal playback thread");
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




///////////////////////////////////////////////////////////////////////////////////////////


/*
The derived values that are cached:
 - mSinkBufferSize from frame count * frame size
 - mActiveSleepTimeUs from activeSleepTimeUs()
 - mIdleSleepTimeUs from idleSleepTimeUs()
 - mStandbyDelayNs from mActiveSleepTimeUs (DIRECT only) or forced to at least
   kDefaultStandbyTimeInNsecs when connected to an A2DP device.
 - maxPeriod from frame count and sample rate (MIXER only)

The parameters that affect these derived values are:
 - frame count
 - frame size
 - sample rate
 - device type: A2DP or not
 - device latency
 - format: PCM or not
 - active sleep time
 - idle sleep time
*/

void AudioFlinger::PlaybackThread::cacheParameters_l()
{
    mSinkBufferSize = mNormalFrameCount * mFrameSize;
    mActiveSleepTimeUs = activeSleepTimeUs();
    mIdleSleepTimeUs = idleSleepTimeUs();

    // make sure standby delay is not too short when connected to an A2DP sink to avoid
    // truncating audio when going to standby.
    mStandbyDelayNs = AudioFlinger::mStandbyTimeInNsecs;
    /**
     * 
    */
    if ((mOutDevice & AUDIO_DEVICE_OUT_ALL_A2DP) != 0) {
        if (mStandbyDelayNs < kDefaultStandbyTimeInNsecs) {
            mStandbyDelayNs = kDefaultStandbyTimeInNsecs;
        }
    }
}


effect_buffer_t *PlaybackThread::sinkBuffer() const { 
    /**
     * PlaybackThread::mSinkBuffer
    */
    return reinterpret_cast<effect_buffer_t *>(mSinkBuffer); 
};


////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
*/
bool AudioFlinger::PlaybackThread::threadLoop()
{
    /**
     * 
    */
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
    /**
     * 
    */
    cacheParameters_l();
    mSleepTimeUs = mIdleSleepTimeUs;

    if (mType == MIXER) {
        sleepTimeShift = 0;
    }

    CpuStats cpuStats;
    const String8 myName(String8::format("thread %p type %d TID %d", this, mType, gettid()));

    /**
     * 
    */
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
     * Silent(沉默的)
     * 
     *  第一步判断 mMasterMute 为false，
     * 
     * 第二步如果mMasterMute 为false 检测 mOutDevice 是否为 AUDIO_DEVICE_OUT_REMOTE_SUBMIX 类型，如果是，不理会 "ro.audio.silent" ;
     * 
     * 如果不是，执行第三步 再次检测系统属性"ro.audio.silent"是否不为0
     * 如果不为0，则 setMasterMute_l(true); 
     * 
     * setMasterMute_l 中 给 mMasterMute 赋值为 true
    */
    checkSilentMode_l();

    /**
     * 由有 PlaybackThread 继承 ThreadBase（继承Thread）
     * 
     * frameworks/av/services/audioflinger/Threads.h:22:class ThreadBase : public Thread {}
     * 
     * Thread::exitPending  为线程函数  @   system/core/libutils/Threads.cpp 
    */
    while (!exitPending())
    {
        /**
         * 这里用户唤醒 sMediaLogService 服务 
        */
        // Log merge requests are performed during AudioFlinger binder transactions, but
        // that does not cover audio playback. It's requested here for that reason.
        mAudioFlinger->requestLogMerge();


        /**
         * 默认 cpuStats 没有任何动作
         * 开启该功能 frameworks/av/services/audioflinger/Configuration.h:42://#define DEBUG_CPU_USAGE 10
        */
        cpuStats.sample(myName);

        Vector< sp<EffectChain> > effectChains;

        { // scope for mLock

            Mutex::Autolock _l(mLock);
            /**
             * 这里是在 ThreadBase中实现
             *  ConfigEvent 事件处理
            */
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
                // We always fetch the timestamp here because often the downstream  sink will block while writing.(我们总是在这里获取时间戳，因为通常下游接收器在写入时会阻塞。)
                /**
                 * frameworks/av/media/libaudioclient/include/media/AudioTimestamp.h
                */
                ExtendedTimestamp timestamp; // use private copy to fetch
                (void) mNormalSink->getTimestamp(timestamp);

                // We keep track of the last valid kernel position in case we are in underrun
                // and the normal mixer period is the same as the fast mixer period, or there
                // is some error from the HAL.
                if (mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] >= 0) {
                    mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL_LASTKERNELOK] =  mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL];
                    mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL_LASTKERNELOK] = mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL];

                    mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER_LASTKERNELOK] = mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER];
                    mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER_LASTKERNELOK] =  mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER];
                }

                if (timestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] >= 0) {
                    kernelLocationUpdate = true;
                } else {
                    ALOGVV("getTimestamp error - no valid kernel position");
                }

                // copy over kernel info
                mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL] =  timestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL]  + mSuspendedFrames; // add frames discarded when suspended
                mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] = timestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL];
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
                    /**
                     * 
                    */
                    mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER] = mLastWriteTime == -1 ? systemTime() : mLastWriteTime;
                }

                for (const sp<Track> &t : mActiveTracks) {
                    /**
                     * @    frameworks/av/services/audioflinger/PlaybackTracks.h
                     * 
                     * (mFlags & AUDIO_OUTPUT_FLAG_FAST) != 0;
                    */
                    if (!t->isFastTrack()) {
                        t->updateTrackFrameInfo( t->mAudioTrackServerProxy->framesReleased(),mFramesWritten, mTimestamp);
                    }
                }
            }
#if 0
            // logFormat example
            if (z % 100 == 0) {
                timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                LOGT("This is an integer %d, this is a float %f, this is my " "pid %p %% %s %t", 42, 3.14, "and this is a timestamp", ts);
                LOGT("A deceptive null-terminated string %\0");
            }
            ++z;
#endif

            /**
             * 该方法只有 DuplicatingThread 有实现
             *  所以这里没有任何动作
            */
            saveOutputTracks();



            if (mSignalPending) {
                // A signal was raised while we were unlocked
                mSignalPending = false;
            } else if (waitingAsyncCallback_l()) {//    waitingAsyncCallback_l 在 PlaybackThread实现 返回 false，只有OffloadThread才会有实现
                。。。。。。
            }

            /**
             * mActiveTracks 中没有激活的 Track，或者线程被挂起
             * systemTime() > mStandbyTimeNs 当前时间是否大于上一次计算得到的Standby时间
            */
            if ((!mActiveTracks.size() && systemTime() > mStandbyTimeNs) || isSuspended()) {
                /**
                 * return !mStandby;
                 * 如果 mStandby 为false ，处理 mStandby
                */
                // put audio hardware into standby after short delay
                if (shouldStandby_l()) {

                    /**
                     * 在 PlaybackThread 中实现
                     * 
                    */
                    threadLoop_standby();

                    // This is where we go into standby
                    if (!mStandby) {
                        /**
                         * frameworks/av/services/audioflinger/TypedLogger.h
                        */
                        LOG_AUDIO_STATE();
                    }
                    mStandby = true;
                }//     shouldStandby_l()

                if (!mActiveTracks.size() && mConfigEvents.isEmpty()) {
                    // we're about to wait, flush the binder command buffer
                    IPCThreadState::self()->flushCommands();
                   /**
                    * 同 saveOutputTracks 一样 只有 DuplicatingThread 有实现
                    *  所以这里没有任何动作
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
                   
                   /**
                    * mStandbyTimeNs为计算下一次standby时间
                   */
                    mStandbyTimeNs = systemTime() + mStandbyDelayNs;

                    mSleepTimeUs = mIdleSleepTimeUs;
                    if (mType == MIXER) {
                        sleepTimeShift = 0;
                    }

                    continue;
                }
            }// (!mActiveTracks.size() && systemTime() > mStandbyTimeNs) || isSuspended()

            /**
             * 
            */
            // mMixerStatusIgnoringFastTracks is also updated internally
            mMixerStatus = prepareTracks_l(&tracksToRemove);

            mActiveTracks.updatePowerState(this);

            updateMetadata_l();

            // prevent any changes in effect chain list and in each effect chain
            // during mixing and effect process as the audio buffers could be deleted
            // or modified if an effect is created or deleted
            lockEffectChains_l(effectChains);
        } // mLock scope ends
        /***
         *  正常播放日志结果
         * 01-13 08:15:52.985  3832  3847 V AudioFlinger_Thread: thread 0xf3703dc0 type MIXER TID 3847 threadLoop() mBytesRemaining:0,mMixerStatus:2(MIXER_TRACKS_READY)
         */
        ALOGVV("%s threadLoop() mBytesRemaining:%zu,mMixerStatus:%d",myName.c_str(),mBytesRemaining,mMixerStatus);

        if (mBytesRemaining == 0) {
            mCurrentWriteLength = 0;
            if (mMixerStatus == MIXER_TRACKS_READY) {//2
                // threadLoop_mix() sets mCurrentWriteLength
                threadLoop_mix();
            } else if ((mMixerStatus != MIXER_DRAIN_TRACK /*3*/)  && (mMixerStatus != MIXER_DRAIN_ALL /*4*/)) {
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
                    mono_blend(mMixerBuffer, mMixerBufferFormat, mChannelCount, mNormalFrameCount,  true /*limit*/);
                }

                memcpy_by_audio_format(buffer, format, mMixerBuffer, mMixerBufferFormat,  mNormalFrameCount * mChannelCount);
             }// mMixerBufferValid

            mBytesRemaining = mCurrentWriteLength;

            if (isSuspended()) {
                // Simulate write to HAL when suspended (e.g. BT SCO phone call).
                /**
                 * mSleepTimeUs = (uint32_t)(((mNormalFrameCount * 1000) / mSampleRate) * 1000)
                */
                mSleepTimeUs = suspendSleepTimeUs(); // assumes full buffer.
                const size_t framesRemaining = mBytesRemaining / mFrameSize;
                mBytesWritten += mBytesRemaining;
                mFramesWritten += framesRemaining;
                mSuspendedFrames += framesRemaining; // to adjust kernel HAL position
                mBytesRemaining = 0;
            }// isSuspended()

            // only process effects if we're going to write
            if (mSleepTimeUs == 0 && mType != OFFLOAD) {
                for (size_t i = 0; i < effectChains.size(); i ++) {
                    effectChains[i]->process_l();
                }
            }
        }//mBytesRemaining == 0

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
                mono_blend(mEffectBuffer, mEffectBufferFormat, mChannelCount, mNormalFrameCount,true /*limit*/);
            }

            memcpy_by_audio_format(mSinkBuffer, mFormat, mEffectBuffer, mEffectBufferFormat,mNormalFrameCount * mChannelCount);
        }

        // enable changes in effect chain
        unlockEffectChains(effectChains);

        if (!waitingAsyncCallback()) {//    waitingAsyncCallback_l 在 PlaybackThread实现 返回 false，只有OffloadThread才会有实现
            //执行这里
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
                } else if ((mMixerStatus == MIXER_DRAIN_TRACK) || (mMixerStatus == MIXER_DRAIN_ALL)) {
                    threadLoop_drain();
                }


                if (mType == MIXER && !mStandby) {
                    // write blocked detection
                    if (delta > maxPeriod) {
                        mNumDelayedWrites++;
                        if ((lastWriteFinished - lastWarning) > kWarningThrottleNs) {
                            ATRACE_NAME("underrun");
                            ALOGW("write blocked for %llu msecs, %d delayed writes, thread %p", (unsigned long long) ns2ms(delta), mNumDelayedWrites, this);
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
                        __builtin_sub_overflow(lastWriteFinished,previousLastWriteFinished, &deltaNs);
                        const int32_t deltaMs = deltaNs / 1000000;

                        const int32_t throttleMs = (int32_t)mHalfBufferMs - deltaMs;
                        if ((signed)mHalfBufferMs >= throttleMs && throttleMs > 0) {
                            usleep(throttleMs * 1000);
                            // notify of throttle start on verbose log
                            ALOGV_IF(mThreadThrottleEndMs == mThreadThrottleTimeMs,  "mixer(%p) throttle begin:" " ret(%zd) deltaMs(%d) requires sleep %d ms", this, ret, deltaMs, throttleMs);
                            mThreadThrottleTimeMs += throttleMs;
                            // Throttle must be attributed to the previous mixer loop's write time
                            // to allow back-to-back throttling.
                            lastWriteFinished += throttleMs * 1000000;
                        } else {
                            uint32_t diff = mThreadThrottleTimeMs - mThreadThrottleEndMs;
                            if (diff > 0) {
                                // notify of throttle end on debug log
                                // but prevent spamming for bluetooth
                                ALOGD_IF(!audio_is_a2dp_out_device(outDevice()) && !audio_is_hearing_aid_out_device(outDevice()), "mixer(%p) throttle end: throttle time(%u)", this, diff);
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
                    /**
                     * frameworks/av/services/audioflinger/Threads.h:643:    static const nsecs_t kMaxNextBufferDelayNs = 100000000;
                     * 
                    */
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

    }// while (!exitPending())

    threadLoop_exit();

    if (!mStandby) {
        threadLoop_standby();
        mStandby = true;
    }

    releaseWakeLock();

    ALOGV("Thread %p type %d exiting", this, mType);

    return false;
}









// post condition: mConfigEvents.isEmpty()
void AudioFlinger::ThreadBase::processConfigEvents_l()
{
    bool configChanged = false;

    while (!mConfigEvents.isEmpty()) {

        ALOGV("processConfigEvents_l() remaining events %zu", mConfigEvents.size());

        sp<ConfigEvent> event = mConfigEvents[0];
        mConfigEvents.removeAt(0);
        switch (event->mType) {
        case CFG_EVENT_PRIO: {
            PrioConfigEventData *data = (PrioConfigEventData *)event->mData.get();
            // FIXME Need to understand why this has to be done asynchronously
            int err = requestPriority(data->mPid, data->mTid, data->mPrio, data->mForApp, true /*asynchronous*/);
            if (err != 0) {
                ALOGW("Policy SCHED_FIFO priority %d is unavailable for pid %d tid %d; error %d",data->mPrio, data->mPid, data->mTid, err);
            }
        } break;
        case CFG_EVENT_IO: {
            IoConfigEventData *data = (IoConfigEventData *)event->mData.get();
            ioConfigChanged(data->mEvent, data->mPid);
        } break;
        case CFG_EVENT_SET_PARAMETER: {
            SetParameterConfigEventData *data = (SetParameterConfigEventData *)event->mData.get();
            if (checkForNewParameter_l(data->mKeyValuePairs, event->mStatus)) {
                configChanged = true;
                mLocalLog.log("CFG_EVENT_SET_PARAMETER: (%s) configuration changed", data->mKeyValuePairs.string());
            }
        } break;
        case CFG_EVENT_CREATE_AUDIO_PATCH: {
            const audio_devices_t oldDevice = getDevice();
            CreateAudioPatchConfigEventData *data =  (CreateAudioPatchConfigEventData *)event->mData.get();
            event->mStatus = createAudioPatch_l(&data->mPatch, &data->mHandle);
            const audio_devices_t newDevice = getDevice();
            mLocalLog.log("CFG_EVENT_CREATE_AUDIO_PATCH: old device %#x (%s) new device %#x (%s)",
                    (unsigned)oldDevice, devicesToString(oldDevice).c_str(),  (unsigned)newDevice, devicesToString(newDevice).c_str());
        } break;
        case CFG_EVENT_RELEASE_AUDIO_PATCH: {
            const audio_devices_t oldDevice = getDevice();
            ReleaseAudioPatchConfigEventData *data =  (ReleaseAudioPatchConfigEventData *)event->mData.get();
            event->mStatus = releaseAudioPatch_l(data->mHandle);
            const audio_devices_t newDevice = getDevice();
            mLocalLog.log("CFG_EVENT_RELEASE_AUDIO_PATCH: old device %#x (%s) new device %#x (%s)",
                    (unsigned)oldDevice, devicesToString(oldDevice).c_str(),
                    (unsigned)newDevice, devicesToString(newDevice).c_str());
        } break;
        default:
            ALOG_ASSERT(false, "processConfigEvents_l() unknown event type %d", event->mType);
            break;
        }
        {
            Mutex::Autolock _l(event->mLock);
            if (event->mWaitStatus) {
                event->mWaitStatus = false;
                event->mCond.signal();
            }
        }
        ALOGV_IF(mConfigEvents.isEmpty(), "processConfigEvents_l() DONE thread %p", this);
    }

    if (configChanged) {
        cacheParameters_l();
    }
}









////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
*/
// shared by MIXER and DIRECT, overridden by DUPLICATING ()
ssize_t AudioFlinger::PlaybackThread::threadLoop_write()
{
    LOG_HIST_TS();
    mInWrite = true;
    ssize_t bytesWritten;
    /**
     * 
    */
    const size_t offset = mCurrentWriteLength - mBytesRemaining;

    // If an NBAIO sink is present, use it to write the normal mixer's submix
    if (mNormalSink != 0) {
        /**
         * count 为 通过mBytesRemaining 和 mFrameSize 折算成 多少帧
        */
        const size_t count = mBytesRemaining / mFrameSize;

        ATRACE_BEGIN("write");
        // update the setpoint when AudioFlinger::mScreenState changes
        uint32_t screenState = AudioFlinger::mScreenState;
        if (screenState != mScreenState) {
            mScreenState = screenState;
            MonoPipe *pipe = (MonoPipe *)mPipeSink.get();
            if (pipe != NULL) {
                pipe->setAvgFrames((mScreenState & 1) ? (pipe->maxFrames() * 7) / 8 : mNormalFrameCount * 2);
            }
        }
        /***
         * framesWritten 返回当前写入了多少数据帧
        */
        ssize_t framesWritten = mNormalSink->write((char *)mSinkBuffer + offset, count);

        ATRACE_END();
        /**
         * bytesWritten 保存当前写入的数据总数
        */
        if (framesWritten > 0) {
            bytesWritten = framesWritten * mFrameSize;
        } else {
            bytesWritten = framesWritten;
        }
    // otherwise use the HAL / AudioStreamOut directly
    } else {
        // Direct output and offload threads
        if (mUseAsyncWrite) {
            ALOGW_IF(mWriteAckSequence & 1, "threadLoop_write(): out of sequence write request");
            mWriteAckSequence += 2;
            mWriteAckSequence |= 1;
            ALOG_ASSERT(mCallbackThread != 0);
            mCallbackThread->setWriteBlocked(mWriteAckSequence);
        }
        // FIXME We should have an implementation of timestamps for direct output threads.
        // They are used e.g for multichannel PCM playback over HDMI.
        bytesWritten = mOutput->write((char *)mSinkBuffer + offset, mBytesRemaining);

        if (mUseAsyncWrite &&  ((bytesWritten < 0) || (bytesWritten == (ssize_t)mBytesRemaining))) {
            // do not wait for async callback in case of error of full write
            mWriteAckSequence &= ~1;
            ALOG_ASSERT(mCallbackThread != 0);
            mCallbackThread->setWriteBlocked(mWriteAckSequence);
        }
    }

    mNumWrites++;
    mInWrite = false;
    mStandby = false;
    return bytesWritten;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * AudioFlinger::ThreadBase::processConfigEvents_l()   //CFG_EVENT_CREATE_AUDIO_PATCH
 * -->AudioFlinger::MixerThread::createAudioPatch_l(const struct audio_patch *patch,audio_patch_handle_t *handle)
 * -----> 
*/
status_t AudioFlinger::PlaybackThread::createAudioPatch_l(const struct audio_patch *patch,audio_patch_handle_t *handle)
{
    status_t status = NO_ERROR;

    // store new device and send to effects
    audio_devices_t type = AUDIO_DEVICE_NONE;
    for (unsigned int i = 0; i < patch->num_sinks; i++) {
        type |= patch->sinks[i].ext.device.type;
    }

#ifdef ADD_BATTERY_DATA
    // when changing the audio output device, call addBatteryData to notify
    // the change
    if (mOutDevice != type) {
        uint32_t params = 0;
        // check whether speaker is on
        if (type & AUDIO_DEVICE_OUT_SPEAKER) {
            params |= IMediaPlayerService::kBatteryDataSpeakerOn;
        }

        audio_devices_t deviceWithoutSpeaker = AUDIO_DEVICE_OUT_ALL & ~AUDIO_DEVICE_OUT_SPEAKER;
        // check if any other device (except speaker) is on
        if (type & deviceWithoutSpeaker) {
            params |= IMediaPlayerService::kBatteryDataOtherAudioDeviceOn;
        }

        if (params != 0) {
            addBatteryData(params);
        }
    }
#endif

    for (size_t i = 0; i < mEffectChains.size(); i++) {
        mEffectChains[i]->setDevice_l(type);
    }

    // mPrevOutDevice is the latest device set by createAudioPatch_l(). It is not set when
    // the thread is created so that the first patch creation triggers an ioConfigChanged callback
    bool configChanged = mPrevOutDevice != type;
    mOutDevice = type;
    mPatch = *patch;

    /**
     * hardware/libhardware/include/hardware/audio.h:57:#define AUDIO_DEVICE_API_VERSION_2_0 HARDWARE_DEVICE_API_VERSION(2, 0)
     * hardware/libhardware/include/hardware/audio.h:58:#define AUDIO_DEVICE_API_VERSION_3_0 HARDWARE_DEVICE_API_VERSION(3, 0)
     * 由于 (mOutput->audioHwDev->supportsAudioPatches() 中是判断 Module的版本（AUDIO_DEVICE_API_VERSION_2_0=2.0） 大于等于 AUDIO_DEVICE_API_VERSION_3_0（3.0）
     * 这里false
     * 
     * 01-01 00:00:10.731  1776  1810 V AudioFlinger_Thread: createAudioPatch_l supportsAudioPatches:false
    */
    if (mOutput->audioHwDev->supportsAudioPatches()) {
        sp<DeviceHalInterface> hwDevice = mOutput->audioHwDev->hwDevice();
        status = hwDevice->createAudioPatch(patch->num_sources,patch->sources, patch->num_sinks,patch->sinks, handle);
    } else {
        char *address;
        // 01-01 00:00:10.731  1776  1810 V AudioFlinger_Thread: createAudioPatch_l address:bus0_media_out
        if (strcmp(patch->sinks[0].ext.device.address, "") != 0) {
            //FIXME: we only support address on first sink with HAL version < 3.0
            address = audio_device_address_to_parameter(patch->sinks[0].ext.device.type,patch->sinks[0].ext.device.address);
        } else {
            address = (char *)calloc(1, 1);
        }
        AudioParameter param = AudioParameter(String8(address));
        free(address);
        param.addInt(String8(AudioParameter::keyRouting), (int)type);
        status = mOutput->stream->setParameters(param.toString());
        *handle = AUDIO_PATCH_HANDLE_NONE;
    }
    if (configChanged) {
        mPrevOutDevice = type;
        sendIoConfigEvent_l(AUDIO_OUTPUT_CONFIG_CHANGED);
    }
    return status;
}
