
//  

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
    if ((mType == MIXER || mType == DUPLICATING)
            && !isValidPcmSinkChannelMask(mChannelMask)) {
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
     * 
    */
    (void)posix_memalign(&mSinkBuffer, 32, sinkBufferSize);

    // We resize the mMixerBuffer according to the requirements of the sink buffer which
    // drives the output.
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
    if (mEffectBufferEnabled) {// true
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
