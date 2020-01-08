
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libaudioclient/AudioTrack.cpp
/**
 * 
*/
status_t AudioTrack::set(
        audio_stream_type_t streamType,
        uint32_t sampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask,
        size_t frameCount,
        audio_output_flags_t flags,
        callback_t cbf,
        void* user,
        int32_t notificationFrames,
        const sp<IMemory>& sharedBuffer,
        bool threadCanCallJava,
        audio_session_t sessionId,
        transfer_type transferType,
        const audio_offload_info_t *offloadInfo,
        uid_t uid,
        pid_t pid,
        const audio_attributes_t* pAttributes,
        bool doNotReconnect,
        float maxRequiredSpeed,
        audio_port_handle_t selectedDeviceId)
{
    status_t status;
    uint32_t channelCount;
    pid_t callingPid;
    pid_t myPid;

    ALOGV("set(): streamType %d, sampleRate %u, format %#x, channelMask %#x, frameCount %zu, "
          "flags #%x, notificationFrames %d, sessionId %d, transferType %d, uid %d, pid %d",
          streamType, sampleRate, format, channelMask, frameCount, flags, notificationFrames,
          sessionId, transferType, uid, pid);

    mThreadCanCallJava = threadCanCallJava;
    mSelectedDeviceId = selectedDeviceId;
    mSessionId = sessionId;

    switch (transferType) {
    case TRANSFER_DEFAULT:
        if (sharedBuffer != 0) {
            transferType = TRANSFER_SHARED;
        } else if (cbf == NULL || threadCanCallJava) {
            transferType = TRANSFER_SYNC;
        } else {
            transferType = TRANSFER_CALLBACK;
        }
        break;
    case TRANSFER_CALLBACK:
        if (cbf == NULL || sharedBuffer != 0) {
            ALOGE("Transfer type TRANSFER_CALLBACK but cbf == NULL || sharedBuffer != 0");
            status = BAD_VALUE;
            goto exit;
        }
        break;
    case TRANSFER_OBTAIN:
    case TRANSFER_SYNC:
        if (sharedBuffer != 0) {
            ALOGE("Transfer type TRANSFER_OBTAIN but sharedBuffer != 0");
            status = BAD_VALUE;
            goto exit;
        }
        break;
    case TRANSFER_SHARED:
        if (sharedBuffer == 0) {
            ALOGE("Transfer type TRANSFER_SHARED but sharedBuffer == 0");
            status = BAD_VALUE;
            goto exit;
        }
        break;
    default:
        ALOGE("Invalid transfer type %d", transferType);
        status = BAD_VALUE;
        goto exit;
    }
    mSharedBuffer = sharedBuffer;
    mTransfer = transferType;
    mDoNotReconnect = doNotReconnect;

    ALOGV_IF(sharedBuffer != 0, "sharedBuffer: %p, size: %zu", sharedBuffer->pointer(),sharedBuffer->size());

    ALOGV("set() streamType %d frameCount %zu flags %04x", streamType, frameCount, flags);

    // invariant that mAudioTrack != 0 is true only after set() returns successfully
    if (mAudioTrack != 0) {
        ALOGE("Track already in use");
        status = INVALID_OPERATION;
        goto exit;
    }

    // handle default values first.
    if (streamType == AUDIO_STREAM_DEFAULT) {
        streamType = AUDIO_STREAM_MUSIC;
    }
    if (pAttributes == NULL) {
        if (uint32_t(streamType) >= AUDIO_STREAM_PUBLIC_CNT) {
            ALOGE("Invalid stream type %d", streamType);
            status = BAD_VALUE;
            goto exit;
        }
        mStreamType = streamType;

    } else {
        // stream type shouldn't be looked at, this track has audio attributes
        memcpy(&mAttributes, pAttributes, sizeof(audio_attributes_t));
        ALOGV("Building AudioTrack with attributes: usage=%d content=%d flags=0x%x tags=[%s]", mAttributes.usage, mAttributes.content_type, mAttributes.flags, mAttributes.tags);
        mStreamType = AUDIO_STREAM_DEFAULT;
        if ((mAttributes.flags & AUDIO_FLAG_HW_AV_SYNC) != 0) {
            flags = (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_HW_AV_SYNC);
        }
        if ((mAttributes.flags & AUDIO_FLAG_LOW_LATENCY) != 0) {
            flags = (audio_output_flags_t) (flags | AUDIO_OUTPUT_FLAG_FAST);
        }
        // check deep buffer after flags have been modified above
        if (flags == AUDIO_OUTPUT_FLAG_NONE && (mAttributes.flags & AUDIO_FLAG_DEEP_BUFFER) != 0) {
            flags = AUDIO_OUTPUT_FLAG_DEEP_BUFFER;
        }
    }

    // these below should probably come from the audioFlinger too...
    if (format == AUDIO_FORMAT_DEFAULT) {
        format = AUDIO_FORMAT_PCM_16_BIT;
    } else if (format == AUDIO_FORMAT_IEC61937) { // HDMI pass-through?
        mAttributes.flags |= AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO;
    }

    // validate parameters
    if (!audio_is_valid_format(format)) {
        ALOGE("Invalid format %#x", format);
        status = BAD_VALUE;
        goto exit;
    }
    mFormat = format;

    if (!audio_is_output_channel(channelMask)) {
        ALOGE("Invalid channel mask %#x", channelMask);
        status = BAD_VALUE;
        goto exit;
    }
    mChannelMask = channelMask;
    channelCount = audio_channel_count_from_out_mask(channelMask);
    mChannelCount = channelCount;

    // force direct flag if format is not linear PCM
    // or offload was requested
    if ((flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)  || !audio_is_linear_pcm(format)) {
        ALOGV( (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)
                    ? "Offload request, forcing to Direct Output"
                    : "Not linear PCM, forcing to Direct Output");
        flags = (audio_output_flags_t)
                // FIXME why can't we allow direct AND fast?
                ((flags | AUDIO_OUTPUT_FLAG_DIRECT) & ~AUDIO_OUTPUT_FLAG_FAST);
    }

    // force direct flag if HW A/V sync requested
    if ((flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) != 0) {
        flags = (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_DIRECT);
    }

    if (flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        if (audio_has_proportional_frames(format)) {
            mFrameSize = channelCount * audio_bytes_per_sample(format);
        } else {
            mFrameSize = sizeof(uint8_t);
        }
    } else {
        ALOG_ASSERT(audio_has_proportional_frames(format));
        mFrameSize = channelCount * audio_bytes_per_sample(format);
        // createTrack will return an error if PCM format is not supported by server,
        // so no need to check for specific PCM formats here
    }

    // sampling rate must be specified for direct outputs
    if (sampleRate == 0 && (flags & AUDIO_OUTPUT_FLAG_DIRECT) != 0) {
        status = BAD_VALUE;
        goto exit;
    }
    mSampleRate = sampleRate;
    mOriginalSampleRate = sampleRate;
    mPlaybackRate = AUDIO_PLAYBACK_RATE_DEFAULT;
    // 1.0 <= mMaxRequiredSpeed <= AUDIO_TIMESTRETCH_SPEED_MAX
    mMaxRequiredSpeed = min(max(maxRequiredSpeed, 1.0f), AUDIO_TIMESTRETCH_SPEED_MAX);

    // Make copy of input parameter offloadInfo so that in the future:
    //  (a) createTrack_l doesn't need it as an input parameter
    //  (b) we can support re-creation of offloaded tracks
    if (offloadInfo != NULL) {
        mOffloadInfoCopy = *offloadInfo;
        mOffloadInfo = &mOffloadInfoCopy;
    } else {
        mOffloadInfo = NULL;
        memset(&mOffloadInfoCopy, 0, sizeof(audio_offload_info_t));
    }

    mVolume[AUDIO_INTERLEAVE_LEFT] = 1.0f;
    mVolume[AUDIO_INTERLEAVE_RIGHT] = 1.0f;
    mSendLevel = 0.0f;
    // mFrameCount is initialized in createTrack_l
    mReqFrameCount = frameCount;
    if (notificationFrames >= 0) {
        mNotificationFramesReq = notificationFrames;
        mNotificationsPerBufferReq = 0;
    } else {
        if (!(flags & AUDIO_OUTPUT_FLAG_FAST)) {
            ALOGE("notificationFrames=%d not permitted for non-fast track",notificationFrames);
            status = BAD_VALUE;
            goto exit;
        }
        if (frameCount > 0) {
            ALOGE("notificationFrames=%d not permitted with non-zero frameCount=%zu",
                    notificationFrames, frameCount);
            status = BAD_VALUE;
            goto exit;
        }
        mNotificationFramesReq = 0;
        const uint32_t minNotificationsPerBuffer = 1;
        const uint32_t maxNotificationsPerBuffer = 8;
        mNotificationsPerBufferReq = min(maxNotificationsPerBuffer,
                max((uint32_t) -notificationFrames, minNotificationsPerBuffer));
        ALOGW_IF(mNotificationsPerBufferReq != (uint32_t) -notificationFrames,
                "notificationFrames=%d clamped to the range -%u to -%u",
                notificationFrames, minNotificationsPerBuffer, maxNotificationsPerBuffer);
    }
    mNotificationFramesAct = 0;
    callingPid = IPCThreadState::self()->getCallingPid();
    myPid = getpid();
    if (uid == AUDIO_UID_INVALID || (callingPid != myPid)) {
        mClientUid = IPCThreadState::self()->getCallingUid();
    } else {
        mClientUid = uid;
    }
    if (pid == -1 || (callingPid != myPid)) {
        mClientPid = callingPid;
    } else {
        mClientPid = pid;
    }
    mAuxEffectId = 0;
    mOrigFlags = mFlags = flags;
    mCbf = cbf;

    if (cbf != NULL) {
        mAudioTrackThread = new AudioTrackThread(*this, threadCanCallJava);
        mAudioTrackThread->run("AudioTrack", ANDROID_PRIORITY_AUDIO, 0 /*stack*/);
        // thread begins in paused state, and will not reference us until start()
    }
    /**
     * 
     * 
     * 
    */
    // create the IAudioTrack
    status = createTrack_l();

    if (status != NO_ERROR) {
        if (mAudioTrackThread != 0) {
            mAudioTrackThread->requestExit();   // see comment in AudioTrack.h
            mAudioTrackThread->requestExitAndWait();
            mAudioTrackThread.clear();
        }
        goto exit;
    }

    mUserData = user;
    mLoopCount = 0;
    mLoopStart = 0;
    mLoopEnd = 0;
    mLoopCountNotified = 0;
    mMarkerPosition = 0;
    mMarkerReached = false;
    mNewPosition = 0;
    mUpdatePeriod = 0;
    mPosition = 0;
    mReleased = 0;
    mStartNs = 0;
    mStartFromZeroUs = 0;
    AudioSystem::acquireAudioSessionId(mSessionId, mClientPid);
    mSequence = 1;
    mObservedSequence = mSequence;
    mInUnderrun = false;
    mPreviousTimestampValid = false;
    mTimestampStartupGlitchReported = false;
    mRetrogradeMotionReported = false;
    mPreviousLocation = ExtendedTimestamp::LOCATION_INVALID;
    mStartTs.mPosition = 0;
    mUnderrunCountOffset = 0;
    mFramesWritten = 0;
    mFramesWrittenServerOffset = 0;
    mFramesWrittenAtRestore = -1; // -1 is a unique initializer.
    mVolumeHandler = new media::VolumeHandler();

exit:
    mStatus = status;
    return status;
}


status_t AudioTrack::createTrack_l()
{
    status_t status;
    bool callbackAdded = false;

    const sp<IAudioFlinger>& audioFlinger = AudioSystem::get_audio_flinger();
    if (audioFlinger == 0) {
        ALOGE("Could not get audioflinger");
        status = NO_INIT;
        goto exit;
    }

    {
    // mFlags (not mOrigFlags) is modified depending on whether fast request is accepted.
    // After fast request is denied, we will request again if IAudioTrack is re-created.
    // Client can only express a preference for FAST.  Server will perform additional tests.
    if (mFlags & AUDIO_OUTPUT_FLAG_FAST) {
        // either of these use cases:
        // use case 1: shared buffer
        bool sharedBuffer = mSharedBuffer != 0;
        bool transferAllowed =
            // use case 2: callback transfer mode
            (mTransfer == TRANSFER_CALLBACK) ||
            // use case 3: obtain/release mode
            (mTransfer == TRANSFER_OBTAIN) ||
            // use case 4: synchronous write
            ((mTransfer == TRANSFER_SYNC) && mThreadCanCallJava);

        bool fastAllowed = sharedBuffer || transferAllowed;
        if (!fastAllowed) {
            ALOGW("AUDIO_OUTPUT_FLAG_FAST denied by client, not shared buffer and transfer = %s",
                  convertTransferToText(mTransfer));
            mFlags = (audio_output_flags_t) (mFlags & ~AUDIO_OUTPUT_FLAG_FAST);
        }
    }

    IAudioFlinger::CreateTrackInput input;
    if (mStreamType != AUDIO_STREAM_DEFAULT) {
        stream_type_to_audio_attributes(mStreamType, &input.attr);
    } else {
        input.attr = mAttributes;
    }
    input.config = AUDIO_CONFIG_INITIALIZER;
    input.config.sample_rate = mSampleRate;
    input.config.channel_mask = mChannelMask;
    input.config.format = mFormat;
    input.config.offload_info = mOffloadInfoCopy;
    input.clientInfo.clientUid = mClientUid;
    input.clientInfo.clientPid = mClientPid;
    input.clientInfo.clientTid = -1;
    if (mFlags & AUDIO_OUTPUT_FLAG_FAST) {
        // It is currently meaningless to request SCHED_FIFO for a Java thread.  Even if the
        // application-level code follows all non-blocking design rules, the language runtime
        // doesn't also follow those rules, so the thread will not benefit overall.
        if (mAudioTrackThread != 0 && !mThreadCanCallJava) {
            input.clientInfo.clientTid = mAudioTrackThread->getTid();
        }
    }
    input.sharedBuffer = mSharedBuffer;
    input.notificationsPerBuffer = mNotificationsPerBufferReq;
    input.speed = 1.0;
    if (audio_has_proportional_frames(mFormat) && mSharedBuffer == 0 &&
            (mFlags & AUDIO_OUTPUT_FLAG_FAST) == 0) {
        input.speed  = !isPurePcmData_l() || isOffloadedOrDirect_l() ? 1.0f :
                        max(mMaxRequiredSpeed, mPlaybackRate.mSpeed);
    }
    input.flags = mFlags;
    input.frameCount = mReqFrameCount;
    input.notificationFrameCount = mNotificationFramesReq;
    input.selectedDeviceId = mSelectedDeviceId;
    input.sessionId = mSessionId;

    IAudioFlinger::CreateTrackOutput output;

    sp<IAudioTrack> track = audioFlinger->createTrack(input,
                                                      output,
                                                      &status);

    if (status != NO_ERROR || output.outputId == AUDIO_IO_HANDLE_NONE) {
        ALOGE("AudioFlinger could not create track, status: %d output %d", status, output.outputId);
        if (status == NO_ERROR) {
            status = NO_INIT;
        }
        goto exit;
    }
    ALOG_ASSERT(track != 0);

    mFrameCount = output.frameCount;
    mNotificationFramesAct = (uint32_t)output.notificationFrameCount;
    mRoutedDeviceId = output.selectedDeviceId;
    mSessionId = output.sessionId;

    mSampleRate = output.sampleRate;
    if (mOriginalSampleRate == 0) {
        mOriginalSampleRate = mSampleRate;
    }

    mAfFrameCount = output.afFrameCount;
    mAfSampleRate = output.afSampleRate;
    mAfLatency = output.afLatencyMs;

    mLatency = mAfLatency + (1000LL * mFrameCount) / mSampleRate;

    // AudioFlinger now owns the reference to the I/O handle,
    // so we are no longer responsible for releasing it.

    // FIXME compare to AudioRecord
    sp<IMemory> iMem = track->getCblk();
    if (iMem == 0) {
        ALOGE("Could not get control block");
        status = NO_INIT;
        goto exit;
    }
    void *iMemPointer = iMem->pointer();
    if (iMemPointer == NULL) {
        ALOGE("Could not get control block pointer");
        status = NO_INIT;
        goto exit;
    }
    // invariant that mAudioTrack != 0 is true only after set() returns successfully
    if (mAudioTrack != 0) {
        IInterface::asBinder(mAudioTrack)->unlinkToDeath(mDeathNotifier, this);
        mDeathNotifier.clear();
    }
    mAudioTrack = track;
    mCblkMemory = iMem;
    IPCThreadState::self()->flushCommands();

    audio_track_cblk_t* cblk = static_cast<audio_track_cblk_t*>(iMemPointer);
    mCblk = cblk;

    mAwaitBoost = false;
    if (mFlags & AUDIO_OUTPUT_FLAG_FAST) {
        if (output.flags & AUDIO_OUTPUT_FLAG_FAST) {
            ALOGI("AUDIO_OUTPUT_FLAG_FAST successful; frameCount %zu -> %zu",
                  mReqFrameCount, mFrameCount);
            if (!mThreadCanCallJava) {
                mAwaitBoost = true;
            }
        } else {
            ALOGW("AUDIO_OUTPUT_FLAG_FAST denied by server; frameCount %zu -> %zu", mReqFrameCount,
                  mFrameCount);
        }
    }
    mFlags = output.flags;

    //mOutput != output includes the case where mOutput == AUDIO_IO_HANDLE_NONE for first creation
    if (mDeviceCallback != 0 && mOutput != output.outputId) {
        if (mOutput != AUDIO_IO_HANDLE_NONE) {
            AudioSystem::removeAudioDeviceCallback(this, mOutput);
        }
        AudioSystem::addAudioDeviceCallback(this, output.outputId);
        callbackAdded = true;
    }

    // We retain a copy of the I/O handle, but don't own the reference
    mOutput = output.outputId;
    mRefreshRemaining = true;

    // Starting address of buffers in shared memory.  If there is a shared buffer, buffers
    // is the value of pointer() for the shared buffer, otherwise buffers points
    // immediately after the control block.  This address is for the mapping within client
    // address space.  AudioFlinger::TrackBase::mBuffer is for the server address space.
    void* buffers;
    if (mSharedBuffer == 0) {
        buffers = cblk + 1;
    } else {
        buffers = mSharedBuffer->pointer();
        if (buffers == NULL) {
            ALOGE("Could not get buffer pointer");
            status = NO_INIT;
            goto exit;
        }
    }

    mAudioTrack->attachAuxEffect(mAuxEffectId);

    // If IAudioTrack is re-created, don't let the requested frameCount
    // decrease.  This can confuse clients that cache frameCount().
    if (mFrameCount > mReqFrameCount) {
        mReqFrameCount = mFrameCount;
    }

    // reset server position to 0 as we have new cblk.
    mServer = 0;

    // update proxy
    if (mSharedBuffer == 0) {
        mStaticProxy.clear();
        mProxy = new AudioTrackClientProxy(cblk, buffers, mFrameCount, mFrameSize);
    } else {
        mStaticProxy = new StaticAudioTrackClientProxy(cblk, buffers, mFrameCount, mFrameSize);
        mProxy = mStaticProxy;
    }

    mProxy->setVolumeLR(gain_minifloat_pack(
            gain_from_float(mVolume[AUDIO_INTERLEAVE_LEFT]),
            gain_from_float(mVolume[AUDIO_INTERLEAVE_RIGHT])));

    mProxy->setSendLevel(mSendLevel);
    const uint32_t effectiveSampleRate = adjustSampleRate(mSampleRate, mPlaybackRate.mPitch);
    const float effectiveSpeed = adjustSpeed(mPlaybackRate.mSpeed, mPlaybackRate.mPitch);
    const float effectivePitch = adjustPitch(mPlaybackRate.mPitch);
    mProxy->setSampleRate(effectiveSampleRate);

    AudioPlaybackRate playbackRateTemp = mPlaybackRate;
    playbackRateTemp.mSpeed = effectiveSpeed;
    playbackRateTemp.mPitch = effectivePitch;
    mProxy->setPlaybackRate(playbackRateTemp);
    mProxy->setMinimum(mNotificationFramesAct);

    mDeathNotifier = new DeathNotifier(this);
    IInterface::asBinder(mAudioTrack)->linkToDeath(mDeathNotifier, this);

    }

exit:
    if (status != NO_ERROR && callbackAdded) {
        // note: mOutput is always valid is callbackAdded is true
        AudioSystem::removeAudioDeviceCallback(this, mOutput);
    }

    mStatus = status;

    // sp<IAudioTrack> track destructor will cause releaseOutput() to be called by AudioFlinger
    return status;
}



/**
 * 
*/
status_t AudioTrack::start()
{
    AutoMutex lock(mLock);

    if (mState == STATE_ACTIVE) {
        return INVALID_OPERATION;
    }

    mInUnderrun = true;

    State previousState = mState;
    if (previousState == STATE_PAUSED_STOPPING) {
        mState = STATE_STOPPING;
    } else {
        mState = STATE_ACTIVE;
    }
    (void) updateAndGetPosition_l();

    // save start timestamp
    if (isOffloadedOrDirect_l()) {
        if (getTimestamp_l(mStartTs) != OK) {
            mStartTs.mPosition = 0;
        }
    } else {
        if (getTimestamp_l(&mStartEts) != OK) {
            mStartEts.clear();
        }
    }
    mStartNs = systemTime(); // save this for timestamp adjustment after starting.
    if (previousState == STATE_STOPPED || previousState == STATE_FLUSHED) {
        // reset current position as seen by client to 0
        mPosition = 0;
        mPreviousTimestampValid = false;
        mTimestampStartupGlitchReported = false;
        mRetrogradeMotionReported = false;
        mPreviousLocation = ExtendedTimestamp::LOCATION_INVALID;

        if (!isOffloadedOrDirect_l()
                && mStartEts.mTimeNs[ExtendedTimestamp::LOCATION_SERVER] > 0) {
            // Server side has consumed something, but is it finished consuming?
            // It is possible since flush and stop are asynchronous that the server
            // is still active at this point.
            ALOGV("start: server read:%lld  cumulative flushed:%lld  client written:%lld",
                    (long long)(mFramesWrittenServerOffset + mStartEts.mPosition[ExtendedTimestamp::LOCATION_SERVER]),
                    (long long)mStartEts.mFlushed,
                    (long long)mFramesWritten);
            // mStartEts is already adjusted by mFramesWrittenServerOffset, so we delta adjust.
            mFramesWrittenServerOffset -= mStartEts.mPosition[ExtendedTimestamp::LOCATION_SERVER];
        }
        mFramesWritten = 0;
        mProxy->clearTimestamp(); // need new server push for valid timestamp
        mMarkerReached = false;

        // For offloaded tracks, we don't know if the hardware counters are really zero here,
        // since the flush is asynchronous and stop may not fully drain.
        // We save the time when the track is started to later verify whether
        // the counters are realistic (i.e. start from zero after this time).
        mStartFromZeroUs = mStartNs / 1000;

        // force refresh of remaining frames by processAudioBuffer() as last
        // write before stop could be partial.
        mRefreshRemaining = true;
    }
    mNewPosition = mPosition + mUpdatePeriod;
    int32_t flags = android_atomic_and(~(CBLK_STREAM_END_DONE | CBLK_DISABLED), &mCblk->mFlags);

    status_t status = NO_ERROR;
    if (!(flags & CBLK_INVALID)) {
        status = mAudioTrack->start();
        if (status == DEAD_OBJECT) {
            flags |= CBLK_INVALID;
        }
    }
    if (flags & CBLK_INVALID) {
        status = restoreTrack_l("start");
    }

    // resume or pause the callback thread as needed.
    sp<AudioTrackThread> t = mAudioTrackThread;
    if (status == NO_ERROR) {
        if (t != 0) {
            if (previousState == STATE_STOPPING) {
                mProxy->interrupt();
            } else {
                t->resume();
            }
        } else {
            mPreviousPriority = getpriority(PRIO_PROCESS, 0);
            get_sched_policy(0, &mPreviousSchedulingGroup);
            androidSetThreadPriority(0, ANDROID_PRIORITY_AUDIO);
        }

        // Start our local VolumeHandler for restoration purposes.
        mVolumeHandler->setStarted();
    } else {
        ALOGE("start() status %d", status);
        mState = previousState;
        if (t != 0) {
            if (previousState != STATE_STOPPING) {
                t->pause();
            }
        } else {
            setpriority(PRIO_PROCESS, 0, mPreviousPriority);
            set_sched_policy(0, mPreviousSchedulingGroup);
        }
    }

    return status;
}

/**
 * 
*/
ssize_t AudioTrack::write(const void* buffer, size_t userSize, bool blocking)
{
    if (mTransfer != TRANSFER_SYNC) {
        return INVALID_OPERATION;
    }

    if (isDirect()) {
        AutoMutex lock(mLock);
        int32_t flags = android_atomic_and(
                            ~(CBLK_UNDERRUN | CBLK_LOOP_CYCLE | CBLK_LOOP_FINAL | CBLK_BUFFER_END),
                            &mCblk->mFlags);
        if (flags & CBLK_INVALID) {
            return DEAD_OBJECT;
        }
    }

    if (ssize_t(userSize) < 0 || (buffer == NULL && userSize != 0)) {
        // Sanity-check: user is most-likely passing an error code, and it would
        // make the return value ambiguous (actualSize vs error).
        ALOGE("AudioTrack::write(buffer=%p, size=%zu (%zd)", buffer, userSize, userSize);
        return BAD_VALUE;
    }

    size_t written = 0;
    Buffer audioBuffer;

    while (userSize >= mFrameSize) {
        audioBuffer.frameCount = userSize / mFrameSize;

        status_t err = obtainBuffer(&audioBuffer,blocking ? &ClientProxy::kForever : &ClientProxy::kNonBlocking);
        if (err < 0) {
            if (written > 0) {
                break;
            }
            if (err == TIMED_OUT || err == -EINTR) {
                err = WOULD_BLOCK;
            }
            return ssize_t(err);
        }

        size_t toWrite = audioBuffer.size;
        memcpy(audioBuffer.i8, buffer, toWrite);
        buffer = ((const char *) buffer) + toWrite;
        userSize -= toWrite;
        written += toWrite;

        releaseBuffer(&audioBuffer);
    }

    if (written > 0) {
        mFramesWritten += written / mFrameSize;
    }
    return written;
}



status_t AudioTrack::obtainBuffer(Buffer* audioBuffer, int32_t waitCount, size_t *nonContig)
{
    if (audioBuffer == NULL) {
        if (nonContig != NULL) {
            *nonContig = 0;
        }
        return BAD_VALUE;
    }
    if (mTransfer != TRANSFER_OBTAIN) {
        audioBuffer->frameCount = 0;
        audioBuffer->size = 0;
        audioBuffer->raw = NULL;
        if (nonContig != NULL) {
            *nonContig = 0;
        }
        return INVALID_OPERATION;
    }

    const struct timespec *requested;
    struct timespec timeout;
    if (waitCount == -1) {
        requested = &ClientProxy::kForever;
    } else if (waitCount == 0) {
        requested = &ClientProxy::kNonBlocking;
    } else if (waitCount > 0) {
        long long ms = WAIT_PERIOD_MS * (long long) waitCount;
        timeout.tv_sec = ms / 1000;
        timeout.tv_nsec = (int) (ms % 1000) * 1000000;
        requested = &timeout;
    } else {
        ALOGE("%s invalid waitCount %d", __func__, waitCount);
        requested = NULL;
    }
    return obtainBuffer(audioBuffer, requested, NULL /*elapsed*/, nonContig);
}

status_t AudioTrack::obtainBuffer(Buffer* audioBuffer, const struct timespec *requested,
        struct timespec *elapsed, size_t *nonContig)
{
    // previous and new IAudioTrack sequence numbers are used to detect track re-creation
    uint32_t oldSequence = 0;
    uint32_t newSequence;

    Proxy::Buffer buffer;
    status_t status = NO_ERROR;

    static const int32_t kMaxTries = 5;
    int32_t tryCounter = kMaxTries;

    do {
        // obtainBuffer() is called with mutex unlocked, so keep extra references to these fields to
        // keep them from going away if another thread re-creates the track during obtainBuffer()
        sp<AudioTrackClientProxy> proxy;
        sp<IMemory> iMem;

        {   // start of lock scope
            AutoMutex lock(mLock);

            newSequence = mSequence;
            // did previous obtainBuffer() fail due to media server death or voluntary invalidation?
            if (status == DEAD_OBJECT) {
                // re-create track, unless someone else has already done so
                if (newSequence == oldSequence) {
                    status = restoreTrack_l("obtainBuffer");
                    if (status != NO_ERROR) {
                        buffer.mFrameCount = 0;
                        buffer.mRaw = NULL;
                        buffer.mNonContig = 0;
                        break;
                    }
                }
            }
            oldSequence = newSequence;

            if (status == NOT_ENOUGH_DATA) {
                restartIfDisabled();
            }

            // Keep the extra references
            proxy = mProxy;
            iMem = mCblkMemory;

            if (mState == STATE_STOPPING) {
                status = -EINTR;
                buffer.mFrameCount = 0;
                buffer.mRaw = NULL;
                buffer.mNonContig = 0;
                break;
            }

            // Non-blocking if track is stopped or paused
            if (mState != STATE_ACTIVE) {
                requested = &ClientProxy::kNonBlocking;
            }

        }   // end of lock scope

        buffer.mFrameCount = audioBuffer->frameCount;
        // FIXME starts the requested timeout and elapsed over from scratch
        status = proxy->obtainBuffer(&buffer, requested, elapsed);
    } while (((status == DEAD_OBJECT) || (status == NOT_ENOUGH_DATA)) && (tryCounter-- > 0));

    audioBuffer->frameCount = buffer.mFrameCount;
    audioBuffer->size = buffer.mFrameCount * mFrameSize;
    audioBuffer->raw = buffer.mRaw;
    if (nonContig != NULL) {
        *nonContig = buffer.mNonContig;
    }
    return status;
}



void AudioTrack::releaseBuffer(const Buffer* audioBuffer)
{
    // FIXME add error checking on mode, by adding an internal version
    if (mTransfer == TRANSFER_SHARED) {
        return;
    }

    size_t stepCount = audioBuffer->size / mFrameSize;
    if (stepCount == 0) {
        return;
    }

    Proxy::Buffer buffer;
    buffer.mFrameCount = stepCount;
    buffer.mRaw = audioBuffer->raw;

    AutoMutex lock(mLock);
    mReleased += stepCount;
    mInUnderrun = false;
    mProxy->releaseBuffer(&buffer);

    // restart track if it was disabled by audioflinger due to previous underrun
    restartIfDisabled();
}


void AudioTrack::restartIfDisabled()
{
    int32_t flags = android_atomic_and(~CBLK_DISABLED, &mCblk->mFlags);
    if ((mState == STATE_ACTIVE) && (flags & CBLK_DISABLED)) {
        ALOGW("releaseBuffer() track %p disabled due to previous underrun, restarting", this);
        // FIXME ignoring status
        mAudioTrack->start();
    }
}




