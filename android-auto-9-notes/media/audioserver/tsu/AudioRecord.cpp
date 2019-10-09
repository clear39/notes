//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libaudioclient/AudioRecord.cpp

///////////////////////////////////////////////////////////////////////////////////////////////////
// static
status_t AudioRecord::getMinFrameCount(
        size_t* frameCount,
        uint32_t sampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask)
{
    if (frameCount == NULL) {
        return BAD_VALUE;
    }

    size_t size;
    status_t status = AudioSystem::getInputBufferSize(sampleRate, format, channelMask, &size);
    if (status != NO_ERROR) {
        ALOGE("AudioSystem could not query the input buffer size for sampleRate %u, format %#x, " "channelMask %#x; status %d", sampleRate, format, channelMask, status);
        return status;
    }

    // We double the size of input buffer for ping pong use of record buffer.
    // Assumes audio_is_linear_pcm(format)
    if ((*frameCount = (size * 2) / (audio_channel_count_from_in_mask(channelMask) *  audio_bytes_per_sample(format))) == 0) {
        ALOGE("Unsupported configuration: sampleRate %u, format %#x, channelMask %#x", sampleRate, format, channelMask);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t AudioSystem::getInputBufferSize(uint32_t sampleRate, audio_format_t format,audio_channel_mask_t channelMask, size_t* buffSize)
{
    const sp<AudioFlingerClient> afc = getAudioFlingerClient();
    if (afc == 0) {
        return NO_INIT;
    }
    return afc->getInputBufferSize(sampleRate, format, channelMask, buffSize);
}

const sp<AudioSystem::AudioFlingerClient> AudioSystem::getAudioFlingerClient()
{
    // calling get_audio_flinger() will initialize gAudioFlingerClient if needed
    const sp<IAudioFlinger>& af = AudioSystem::get_audio_flinger();
    if (af == 0) return 0;
    Mutex::Autolock _l(gLock);
    return gAudioFlingerClient;
}

const sp<IAudioFlinger> AudioSystem::get_audio_flinger()
{
    sp<IAudioFlinger> af;
    sp<AudioFlingerClient> afc;
    {
        Mutex::Autolock _l(gLock);
        if (gAudioFlinger == 0) {
            sp<IServiceManager> sm = defaultServiceManager();
            sp<IBinder> binder;
            do {
                binder = sm->getService(String16("media.audio_flinger"));
                if (binder != 0)
                    break;
                ALOGW("AudioFlinger not published, waiting...");
                usleep(500000); // 0.5 s
            } while (true);

            if (gAudioFlingerClient == NULL) {
                /**
                 * AudioFlingerClient  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/include/media/AudioSystem.h
                */
                gAudioFlingerClient = new AudioFlingerClient();
            } else {
                if (gAudioErrorCallback) {
                    gAudioErrorCallback(NO_ERROR);
                }
            }
            binder->linkToDeath(gAudioFlingerClient);
            gAudioFlinger = interface_cast<IAudioFlinger>(binder);
            LOG_ALWAYS_FATAL_IF(gAudioFlinger == 0);
            afc = gAudioFlingerClient;
            // Make sure callbacks can be received by gAudioFlingerClient
            ProcessState::self()->startThreadPool();
        }
        af = gAudioFlinger;
    }
    if (afc != 0) {
        int64_t token = IPCThreadState::self()->clearCallingIdentity();
        af->registerClient(afc);
        IPCThreadState::self()->restoreCallingIdentity(token);
    }
    return af;
}


status_t AudioSystem::AudioFlingerClient::getInputBufferSize(uint32_t sampleRate, audio_format_t format,audio_channel_mask_t channelMask, size_t* buffSize)
{
    const sp<IAudioFlinger>& af = AudioSystem::get_audio_flinger();
    if (af == 0) {
        return PERMISSION_DENIED;
    }
    Mutex::Autolock _l(mLock);
    // Do we have a stale mInBuffSize or are we requesting the input buffer size for new values
    if ((mInBuffSize == 0) || (sampleRate != mInSamplingRate) || (format != mInFormat) || (channelMask != mInChannelMask)) {
        size_t inBuffSize = af->getInputBufferSize(sampleRate, format, channelMask);
        if (inBuffSize == 0) {
            ALOGE("AudioSystem::getInputBufferSize failed sampleRate %d format %#x channelMask %#x", sampleRate, format, channelMask);
            return BAD_VALUE;
        }
        // A benign race is possible here: we could overwrite a fresher cache entry
        // save the request params
        mInSamplingRate = sampleRate;
        mInFormat = format;
        mInChannelMask = channelMask;

        mInBuffSize = inBuffSize;
    }

    *buffSize = mInBuffSize;

    return NO_ERROR;
}


size_t AudioFlinger::getInputBufferSize(uint32_t sampleRate, audio_format_t format,audio_channel_mask_t channelMask) const
{
    status_t ret = initCheck();
    if (ret != NO_ERROR) {
        return 0;
    }
    if ((sampleRate == 0) || !audio_is_valid_format(format) || !audio_has_proportional_frames(format) || !audio_is_input_channel(channelMask)) {
        return 0;
    }

    AutoMutex lock(mHardwareLock);
    mHardwareStatus = AUDIO_HW_GET_INPUT_BUFFER_SIZE;
    audio_config_t config, proposed;
    memset(&proposed, 0, sizeof(proposed));
    proposed.sample_rate = sampleRate;
    proposed.channel_mask = channelMask;
    proposed.format = format;

    sp<DeviceHalInterface> dev = mPrimaryHardwareDev->hwDevice();
    size_t frames;
    for (;;) {
        // Note: config is currently a const parameter for get_input_buffer_size()
        // but we use a copy from proposed in case config changes from the call.
        config = proposed;
        status_t result = dev->getInputBufferSize(&config, &frames);
        if (result == OK && frames != 0) {
            break; // hal success, config is the result
        }
        // change one parameter of the configuration each iteration to a more "common" value
        // to see if the device will support it.
        if (proposed.format != AUDIO_FORMAT_PCM_16_BIT) {
            proposed.format = AUDIO_FORMAT_PCM_16_BIT;
        } else if (proposed.sample_rate != 44100) { // 44.1 is claimed as must in CDD as well as
            proposed.sample_rate = 44100;           // legacy AudioRecord.java. TODO: Query hw?
        } else {
            ALOGW("getInputBufferSize failed with minimum buffer size sampleRate %u, " "format %#x, channelMask 0x%X",sampleRate, format, channelMask);
            break; // retries failed, break out of loop with frames == 0.
        }
    }
    mHardwareStatus = AUDIO_HW_IDLE;
    if (frames > 0 && config.sample_rate != sampleRate) {
        frames = destinationFramesPossible(frames, sampleRate, config.sample_rate);
    }
    return frames; // may be converted to bytes at the Java level.
}



///////////////////////////////////////////////////////////////////////////////////////////////////
AudioRecord::AudioRecord(
        audio_source_t inputSource,
        uint32_t sampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask,
        const String16& opPackageName,
        size_t frameCount,
        callback_t cbf,
        void* user,
        uint32_t notificationFrames,
        audio_session_t sessionId /*=AUDIO_SESSION_ALLOCATE*/,
        transfer_type transferType /*=AudioRecord::TRANSFER_DEFAULT*/,
        audio_input_flags_t flags /*=AUDIO_INPUT_FLAG_NONE*/,
        uid_t uid,
        pid_t pid,
        const audio_attributes_t* pAttributes,  /* = NULL*/
        audio_port_handle_t selectedDeviceId /*= AUDIO_PORT_HANDLE_NONE*/)
    : mActive(false),
      mStatus(NO_INIT),
      mOpPackageName(opPackageName),
      mSessionId(AUDIO_SESSION_ALLOCATE),
      mPreviousPriority(ANDROID_PRIORITY_NORMAL),
      mPreviousSchedulingGroup(SP_DEFAULT),
      mProxy(NULL)
{
	//mServiceProxy = new IAudioRecordService();
    (void)set(inputSource, sampleRate, format, channelMask, frameCount, cbf, user,notificationFrames, false /*threadCanCallJava*/, sessionId, transferType, flags, uid, pid, pAttributes, selectedDeviceId);
}


status_t AudioRecord::set(
        audio_source_t inputSource,
        uint32_t sampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask,
        size_t frameCount,
        callback_t cbf,
        void* user,
        uint32_t notificationFrames,
        bool threadCanCallJava,
        audio_session_t sessionId,
        transfer_type transferType,
        audio_input_flags_t flags,
        uid_t uid,
        pid_t pid,
        const audio_attributes_t* pAttributes,  // nULl
        audio_port_handle_t selectedDeviceId)
{
    status_t status = NO_ERROR;
    uint32_t channelCount;
    pid_t callingPid;
    pid_t myPid;

    ALOGV("set(): inputSource %d, sampleRate %u, format %#x, channelMask %#x, frameCount %zu, "
          "notificationFrames %u, sessionId %d, transferType %d, flags %#x, opPackageName %s "
          "uid %d, pid %d",
          inputSource, sampleRate, format, channelMask, frameCount, notificationFrames,
          sessionId, transferType, flags, String8(mOpPackageName).string(), uid, pid);

    mSelectedDeviceId = selectedDeviceId; //    AUDIO_PORT_HANDLE_NONE

    switch (transferType) {
    case TRANSFER_DEFAULT:  // 执行这里
        if (cbf == NULL || threadCanCallJava) {
            transferType = TRANSFER_SYNC;
        } else {
            transferType = TRANSFER_CALLBACK;// 执行这里
        }
        break;
    case TRANSFER_CALLBACK:
        if (cbf == NULL) {
            ALOGE("Transfer type TRANSFER_CALLBACK but cbf == NULL");
            status = BAD_VALUE;
            goto exit;
        }
        break;
    case TRANSFER_OBTAIN:
    case TRANSFER_SYNC:
        break;
    default:
        ALOGE("Invalid transfer type %d", transferType);
        status = BAD_VALUE;
        goto exit;
    }
    mTransfer = transferType; //    TRANSFER_CALLBACK

    /*
	if(mServiceProxy != NULL && mServiceProxy->isValid()){
		if(mTransfer == TRANSFER_SYNC){
			mServiceProxy->nativeCreate(inputSource, sampleRate, format, channelMask, frameCount, cbf, user, notificationFrames, sessionId, threadCanCallJava, true);
			mStatus = NO_ERROR;
			return mStatus;
		} else {
			mServiceProxy->nativeCreate(inputSource, sampleRate, format, channelMask, frameCount, cbf, user, notificationFrames, sessionId, threadCanCallJava, false);
		}
	}
    */

    // invariant that mAudioRecord != 0 is true only after set() returns successfully
    if (mAudioRecord != 0) {
        ALOGE("Track already in use");
        status = INVALID_OPERATION;
        goto exit;
    }

    if (pAttributes == NULL) {
        /**
         *  执行这里
         * */
        memset(&mAttributes, 0, sizeof(audio_attributes_t));
        mAttributes.source = inputSource;
    } else {
        // stream type shouldn't be looked at, this track has audio attributes
        memcpy(&mAttributes, pAttributes, sizeof(audio_attributes_t));
        ALOGV("Building AudioRecord with attributes: source=%d flags=0x%x tags=[%s]",mAttributes.source, mAttributes.flags, mAttributes.tags);
    }

    mSampleRate = sampleRate;

    // these below should probably come from the audioFlinger too...
    if (format == AUDIO_FORMAT_DEFAULT) {
        format = AUDIO_FORMAT_PCM_16_BIT;
    }

    // validate parameters
    // AudioFlinger capture only supports linear PCM
    if (!audio_is_valid_format(format) || !audio_is_linear_pcm(format)) {
        ALOGE("Format %#x is not linear pcm", format);
        status = BAD_VALUE;
        goto exit;
    }
    mFormat = format;

    if (!audio_is_input_channel(channelMask)) {
        ALOGE("Invalid channel mask %#x", channelMask);
        status = BAD_VALUE;
        goto exit;
    }
    mChannelMask = channelMask;
    channelCount = audio_channel_count_from_in_mask(channelMask);
    mChannelCount = channelCount;

    if (audio_is_linear_pcm(format)) {
        mFrameSize = channelCount * audio_bytes_per_sample(format);
    } else {
        mFrameSize = sizeof(uint8_t);
    }

    // mFrameCount is initialized in createRecord_l
    mReqFrameCount = frameCount;

    mNotificationFramesReq = notificationFrames;
    // mNotificationFramesAct is initialized in createRecord_l

    mSessionId = sessionId;
    ALOGV("set(): mSessionId %d", mSessionId);

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

    mOrigFlags = mFlags = flags;
    mCbf = cbf;

    /**
     * 由于创建 AudioRecord 传递 AudioRecordCallbackFunction，所以不为空
     * threadCanCallJava 为false
    */
    if (cbf != NULL) {
        mAudioRecordThread = new AudioRecordThread(*this, threadCanCallJava);
        mAudioRecordThread->run("AudioRecord", ANDROID_PRIORITY_AUDIO);
        // thread begins in paused state, and will not reference us until start()
    }

    // create the IAudioRecord
    status = createRecord_l(0 /*epoch*/, mOpPackageName);

    if (status != NO_ERROR) {
        if (mAudioRecordThread != 0) {
            mAudioRecordThread->requestExit();   // see comment in AudioRecord.h
            mAudioRecordThread->requestExitAndWait();
            mAudioRecordThread.clear();
        }
        goto exit;
    }

    mUserData = user;
    // TODO: add audio hardware input latency here
    mLatency = (1000LL * mFrameCount) / mSampleRate;
    mMarkerPosition = 0;
    mMarkerReached = false;
    mNewPosition = 0;
    mUpdatePeriod = 0;
    AudioSystem::acquireAudioSessionId(mSessionId, -1);
    mSequence = 1;
    mObservedSequence = mSequence;
    mInOverrun = false;
    mFramesRead = 0;
    mFramesReadServerOffset = 0;

exit:
    mStatus = status;
    if (status != NO_ERROR) {
        mMediaMetrics.markError(status, __FUNCTION__);
    }
    return status;
}



// must be called with mLock held
status_t AudioRecord::createRecord_l(const Modulo<uint32_t> &epoch, const String16& opPackageName)
{
    const sp<IAudioFlinger>& audioFlinger = AudioSystem::get_audio_flinger();
    IAudioFlinger::CreateRecordInput input;
    IAudioFlinger::CreateRecordOutput output;
    audio_session_t originalSessionId;
    sp<media::IAudioRecord> record;
    void *iMemPointer;
    audio_track_cblk_t* cblk;
    status_t status;

    if (audioFlinger == 0) {
        ALOGE("Could not get audioflinger");
        status = NO_INIT;
        goto exit;
    }

    // mFlags (not mOrigFlags) is modified depending on whether fast request is accepted.
    // After fast request is denied, we will request again if IAudioRecord is re-created.

    // Now that we have a reference to an I/O handle and have not yet handed it off to AudioFlinger,
    // we must release it ourselves if anything goes wrong.

    // Client can only express a preference for FAST.  Server will perform additional tests.
    if (mFlags & AUDIO_INPUT_FLAG_FAST) {
        bool useCaseAllowed =
            // any of these use cases:
            // use case 1: callback transfer mode
            (mTransfer == TRANSFER_CALLBACK) ||
            // use case 2: blocking read mode
            // The default buffer capacity at 48 kHz is 2048 frames, or ~42.6 ms.
            // That's enough for double-buffering with our standard 20 ms rule of thumb for
            // the minimum period of a non-SCHED_FIFO thread.
            // This is needed so that AAudio apps can do a low latency non-blocking read from a
            // callback running with SCHED_FIFO.
            (mTransfer == TRANSFER_SYNC) ||
            // use case 3: obtain/release mode
            (mTransfer == TRANSFER_OBTAIN);
        if (!useCaseAllowed) {
            ALOGW("AUDIO_INPUT_FLAG_FAST denied, incompatible transfer = %s", convertTransferToText(mTransfer));
            mFlags = (audio_input_flags_t) (mFlags & ~(AUDIO_INPUT_FLAG_FAST | AUDIO_INPUT_FLAG_RAW));
        }
    }

    input.attr = mAttributes;
    input.config.sample_rate = mSampleRate;
    input.config.channel_mask = mChannelMask;
    input.config.format = mFormat;
    input.clientInfo.clientUid = mClientUid;
    input.clientInfo.clientPid = mClientPid;
    input.clientInfo.clientTid = -1;
    if (mFlags & AUDIO_INPUT_FLAG_FAST) {
        if (mAudioRecordThread != 0) {
            input.clientInfo.clientTid = mAudioRecordThread->getTid();
        }
    }
    input.opPackageName = opPackageName;

    input.flags = mFlags;
    // The notification frame count is the period between callbacks, as suggested by the client
    // but moderated by the server.  For record, the calculations are done entirely on server side.
    input.frameCount = mReqFrameCount;
    input.notificationFrameCount = mNotificationFramesReq;
    input.selectedDeviceId = mSelectedDeviceId;  // AUDIO_PORT_HANDLE_NONE
    input.sessionId = mSessionId;
    originalSessionId = mSessionId;
    /**
     * 通过 input 得到 output
     * 
     * **/
    record = audioFlinger->createRecord(input,output,&status);

    if (status != NO_ERROR) {
        ALOGE("AudioFlinger could not create record track, status: %d", status);
        goto exit;
    }
    ALOG_ASSERT(record != 0);

    // AudioFlinger now owns the reference to the I/O handle,
    // so we are no longer responsible for releasing it.

    mAwaitBoost = false;
    if (output.flags & AUDIO_INPUT_FLAG_FAST) {
        ALOGI("AUDIO_INPUT_FLAG_FAST successful; frameCount %zu -> %zu",mReqFrameCount, output.frameCount);
        mAwaitBoost = true;
    }
    mFlags = output.flags;
    mRoutedDeviceId = output.selectedDeviceId;
    mSessionId = output.sessionId;
    mSampleRate = output.sampleRate;

    if (output.cblk == 0) {
        ALOGE("Could not get control block");
        status = NO_INIT;
        goto exit;
    }
    iMemPointer = output.cblk ->pointer();
    if (iMemPointer == NULL) {
        ALOGE("Could not get control block pointer");
        status = NO_INIT;
        goto exit;
    }
    cblk = static_cast<audio_track_cblk_t*>(iMemPointer);

    // Starting address of buffers in shared memory.
    // The buffers are either immediately after the control block,
    // or in a separate area at discretion of server.
    void *buffers;
    if (output.buffers == 0) {
        buffers = cblk + 1;
    } else {
        buffers = output.buffers->pointer();
        if (buffers == NULL) {
            ALOGE("Could not get buffer pointer");
            status = NO_INIT;
            goto exit;
        }
    }

    // invariant that mAudioRecord != 0 is true only after set() returns successfully
    if (mAudioRecord != 0) {
        IInterface::asBinder(mAudioRecord)->unlinkToDeath(mDeathNotifier, this);
        mDeathNotifier.clear();
    }
    mAudioRecord = record;
    mCblkMemory = output.cblk;
    mBufferMemory = output.buffers;
    IPCThreadState::self()->flushCommands();

    mCblk = cblk;
    // note that output.frameCount is the (possibly revised) value of mReqFrameCount
    if (output.frameCount < mReqFrameCount || (mReqFrameCount == 0 && output.frameCount == 0)) {
        ALOGW("Requested frameCount %zu but received frameCount %zu", mReqFrameCount,  output.frameCount);
    }

    // Make sure that application is notified with sufficient margin before overrun.
    // The computation is done on server side.
    if (mNotificationFramesReq > 0 && output.notificationFrameCount != mNotificationFramesReq) {
        ALOGW("Server adjusted notificationFrames from %u to %zu for frameCount %zu", mNotificationFramesReq, output.notificationFrameCount, output.frameCount);
    }
    mNotificationFramesAct = (uint32_t)output.notificationFrameCount;

    //mInput != input includes the case where mInput == AUDIO_IO_HANDLE_NONE for first creation
    if (mDeviceCallback != 0 && mInput != output.inputId) {
        if (mInput != AUDIO_IO_HANDLE_NONE) {
            AudioSystem::removeAudioDeviceCallback(this, mInput);
        }
        AudioSystem::addAudioDeviceCallback(this, output.inputId);
    }

    // We retain a copy of the I/O handle, but don't own the reference
    mInput = output.inputId;
    mRefreshRemaining = true;

    mFrameCount = output.frameCount;
    // If IAudioRecord is re-created, don't let the requested frameCount
    // decrease.  This can confuse clients that cache frameCount().
    if (mFrameCount > mReqFrameCount) {
        mReqFrameCount = mFrameCount;
    }

    // update proxy
    mProxy = new AudioRecordClientProxy(cblk, buffers, mFrameCount, mFrameSize);
    mProxy->setEpoch(epoch);
    mProxy->setMinimum(mNotificationFramesAct);

    mDeathNotifier = new DeathNotifier(this);
    IInterface::asBinder(mAudioRecord)->linkToDeath(mDeathNotifier, this);

exit:
    mStatus = status;
    // sp<IAudioTrack> track destructor will cause releaseOutput() to be called by AudioFlinger
    return status;
}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audioflinger/AudioFlinger.cpp
sp<media::IAudioRecord> AudioFlinger::createRecord(const CreateRecordInput& input,CreateRecordOutput& output,status_t *status)
{
    sp<RecordThread::RecordTrack> recordTrack;
    sp<RecordHandle> recordHandle;
    sp<Client> client;
    status_t lStatus;
    audio_session_t sessionId = input.sessionId;
    audio_port_handle_t portId = AUDIO_PORT_HANDLE_NONE;

    output.cblk.clear();
    output.buffers.clear();
    output.inputId = AUDIO_IO_HANDLE_NONE;

    bool updatePid = (input.clientInfo.clientPid == -1);
    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    uid_t clientUid = input.clientInfo.clientUid;
    if (!isTrustedCallingUid(callingUid)) {
        ALOGW_IF(clientUid != callingUid,"%s uid %d tried to pass itself off as %d",__FUNCTION__, callingUid, clientUid);
        clientUid = callingUid;
        updatePid = true;
    }
    pid_t clientPid = input.clientInfo.clientPid;
    if (updatePid) {
        const pid_t callingPid = IPCThreadState::self()->getCallingPid();
        ALOGW_IF(clientPid != -1 && clientPid != callingPid,"%s uid %d pid %d tried to pass itself off as pid %d",__func__, callingUid, callingPid, clientPid);
        clientPid = callingPid;
    }

    // we don't yet support anything other than linear PCM
    if (!audio_is_valid_format(input.config.format) || !audio_is_linear_pcm(input.config.format)) {
        ALOGE("createRecord() invalid format %#x", input.config.format);
        lStatus = BAD_VALUE;
        goto Exit;
    }

    // further channel mask checks are performed by createRecordTrack_l()
    if (!audio_is_input_channel(input.config.channel_mask)) {
        ALOGE("createRecord() invalid channel mask %#x", input.config.channel_mask);
        lStatus = BAD_VALUE;
        goto Exit;
    }

    if (sessionId == AUDIO_SESSION_ALLOCATE) {
        sessionId = (audio_session_t) newAudioUniqueId(AUDIO_UNIQUE_ID_USE_SESSION);
    } else if (audio_unique_id_get_use(sessionId) != AUDIO_UNIQUE_ID_USE_SESSION) {
        lStatus = BAD_VALUE;
        goto Exit;
    }

    output.sessionId = sessionId;
    output.selectedDeviceId = input.selectedDeviceId;
    output.flags = input.flags;

    client = registerPid(clientPid);

    // Not a conventional loop, but a retry loop for at most two iterations total.
    // Try first maybe with FAST flag then try again without FAST flag if that fails.
    // Exits loop via break on no error of got exit on error
    // The sp<> references will be dropped when re-entering scope.
    // The lack of indentation is deliberate, to reduce code churn and ease merges.
    for (;;) {
    // release previously opened input if retrying.
    if (output.inputId != AUDIO_IO_HANDLE_NONE) {
        recordTrack.clear();
        AudioSystem::releaseInput(portId);
        output.inputId = AUDIO_IO_HANDLE_NONE;
        output.selectedDeviceId = input.selectedDeviceId;
        portId = AUDIO_PORT_HANDLE_NONE;
    }
    lStatus = AudioSystem::getInputForAttr(&input.attr, &output.inputId,
                                      sessionId,
                                    // FIXME compare to AudioTrack
                                      clientPid,
                                      clientUid,
                                      input.opPackageName,
                                      &input.config,
                                      output.flags, &output.selectedDeviceId, &portId);

    {
        Mutex::Autolock _l(mLock);
        RecordThread *thread = checkRecordThread_l(output.inputId);
        if (thread == NULL) {
            ALOGE("createRecord() checkRecordThread_l failed");
            lStatus = BAD_VALUE;
            goto Exit;
        }

        ALOGV("createRecord() lSessionId: %d input %d", sessionId, output.inputId);

        output.sampleRate = input.config.sample_rate;
        output.frameCount = input.frameCount;
        output.notificationFrameCount = input.notificationFrameCount;

        recordTrack = thread->createRecordTrack_l(client, input.attr, &output.sampleRate,
                                                  input.config.format, input.config.channel_mask,
                                                  &output.frameCount, sessionId,
                                                  &output.notificationFrameCount,
                                                  clientUid, &output.flags,
                                                  input.clientInfo.clientTid,
                                                  &lStatus, portId);
        LOG_ALWAYS_FATAL_IF((lStatus == NO_ERROR) && (recordTrack == 0));

        // lStatus == BAD_TYPE means FAST flag was rejected: request a new input from
        // audio policy manager without FAST constraint
        if (lStatus == BAD_TYPE) {
            continue;
        }

        if (lStatus != NO_ERROR) {
            goto Exit;
        }

        // Check if one effect chain was awaiting for an AudioRecord to be created on this
        // session and move it to this thread.
        sp<EffectChain> chain = getOrphanEffectChain_l(sessionId);
        if (chain != 0) {
            Mutex::Autolock _l(thread->mLock);
            thread->addEffectChain_l(chain);
        }
        break;
    }
    // End of retry loop.
    // The lack of indentation is deliberate, to reduce code churn and ease merges.
    }

    output.cblk = recordTrack->getCblk();
    output.buffers = recordTrack->getBuffers();

    // return handle to client
    recordHandle = new RecordHandle(recordTrack);

Exit:
    if (lStatus != NO_ERROR) {
        // remove local strong reference to Client before deleting the RecordTrack so that the
        // Client destructor is called by the TrackBase destructor with mClientLock held
        // Don't hold mClientLock when releasing the reference on the track as the
        // destructor will acquire it.
        {
            Mutex::Autolock _cl(mClientLock);
            client.clear();
        }
        recordTrack.clear();
        if (output.inputId != AUDIO_IO_HANDLE_NONE) {
            AudioSystem::releaseInput(portId);
        }
    }

    *status = lStatus;
    return recordHandle;
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libaudioclient/AudioSystem.cpp
status_t AudioSystem::getInputForAttr(const audio_attributes_t *attr,
                                audio_io_handle_t *input,
                                audio_session_t session,
                                pid_t pid,
                                uid_t uid,
                                const String16& opPackageName,
                                const audio_config_base_t *config,
                                audio_input_flags_t flags,
                                audio_port_handle_t *selectedDeviceId,
                                audio_port_handle_t *portId)
{
    const sp<IAudioPolicyService>& aps = AudioSystem::get_audio_policy_service();
    if (aps == 0) return NO_INIT;
    //  @   frameworks/av/services/audiopolicy/service/AudioPolicyInterfaceImpl.cpp
    return aps->getInputForAttr(
            attr, input, session, pid, uid, opPackageName,
            config, flags, selectedDeviceId, portId);
}

/**
 * 
 * 
 * @    system/media/audio/include/system/audio.h:312:typedef struct audio_config_base audio_config_base_t;
 * audio_config_base    @    system/media/audio/include/system/audio.h
 * 
 * 
 * system/media/audio/include/system/audio.h:45:typedef int audio_io_handle_t;
*/
status_t AudioPolicyService::getInputForAttr(const audio_attributes_t *attr,
                                             audio_io_handle_t *input,
                                             audio_session_t session,
                                             pid_t pid,
                                             uid_t uid,
                                             const String16& opPackageName,
                                             const audio_config_base_t *config,
                                             audio_input_flags_t flags,
                                             audio_port_handle_t *selectedDeviceId,
                                             audio_port_handle_t *portId)
{
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }

    // already checked by client, but double-check in case the client wrapper is bypassed
    /**
     * system/media/audio/include/system/audio-base.h:52:    AUDIO_SOURCE_UNPROCESSED = 9
     * system/media/audio/include/system/audio-base-utils.h:39:    AUDIO_SOURCE_MAX          = AUDIO_SOURCE_UNPROCESSED,
     * system/media/audio/include/system/audio-base-utils.h:40:    AUDIO_SOURCE_CNT          = AUDIO_SOURCE_MAX + 1,
    */
    if (attr->source < AUDIO_SOURCE_DEFAULT && attr->source >= AUDIO_SOURCE_CNT &&
            attr->source != AUDIO_SOURCE_HOTWORD && attr->source != AUDIO_SOURCE_FM_TUNER) {
        return BAD_VALUE;
    }

    bool updatePid = (pid == -1);
    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!isTrustedCallingUid(callingUid)) {
        ALOGW_IF(uid != (uid_t)-1 && uid != callingUid,"%s uid %d tried to pass itself off as %d", __FUNCTION__, callingUid, uid);
        uid = callingUid;
        updatePid = true;
    }

    if (updatePid) {
        const pid_t callingPid = IPCThreadState::self()->getCallingPid();
        ALOGW_IF(pid != (pid_t)-1 && pid != callingPid,"%s uid %d pid %d tried to pass itself off as pid %d",__func__, callingUid, callingPid, pid);
        pid = callingPid;
    }

    /**
     * @    frameworks/av/services/audioflinger/ServiceUtilities.cpp
    */
    // check calling permissions
    if (!recordingAllowed(opPackageName, pid, uid)) {
        ALOGE("%s permission denied: recording not allowed for uid %d pid %d",__func__, uid, pid);
        return PERMISSION_DENIED;
    }

    if ((attr->source == AUDIO_SOURCE_VOICE_UPLINK ||
        attr->source == AUDIO_SOURCE_VOICE_DOWNLINK ||
        attr->source == AUDIO_SOURCE_VOICE_CALL) &&
        !captureAudioOutputAllowed(pid, uid)) {
        return PERMISSION_DENIED;
    }

    if ((attr->source == AUDIO_SOURCE_HOTWORD) && !captureHotwordAllowed(pid, uid)) {
        return BAD_VALUE;
    }

    sp<AudioPolicyEffects>audioPolicyEffects;
    {
        status_t status;
        AudioPolicyInterface::input_type_t inputType;

        Mutex::Autolock _l(mLock);
        {
            AutoCallerClear acc;
            // the audio_in_acoustics_t parameter is ignored by get_input()
            status = mAudioPolicyManager->getInputForAttr(attr, input, session, uid,
                                                         config,
                                                         flags, selectedDeviceId,
                                                         &inputType, portId);
        }
        audioPolicyEffects = mAudioPolicyEffects;

        if (status == NO_ERROR) {
            // enforce permission (if any) required for each type of input
            switch (inputType) {
            case AudioPolicyInterface::API_INPUT_LEGACY:
                break;
            case AudioPolicyInterface::API_INPUT_TELEPHONY_RX:
                // FIXME: use the same permission as for remote submix for now.
            case AudioPolicyInterface::API_INPUT_MIX_CAPTURE:
                if (!captureAudioOutputAllowed(pid, uid)) {
                    ALOGE("getInputForAttr() permission denied: capture not allowed");
                    status = PERMISSION_DENIED;
                }
                break;
            case AudioPolicyInterface::API_INPUT_MIX_EXT_POLICY_REROUTE:
                if (!modifyAudioRoutingAllowed()) {
                    ALOGE("getInputForAttr() permission denied: modify audio routing not allowed");
                    status = PERMISSION_DENIED;
                }
                break;
            case AudioPolicyInterface::API_INPUT_INVALID:
            default:
                LOG_ALWAYS_FATAL("getInputForAttr() encountered an invalid input type %d",(int)inputType);
            }
        }

        if (status != NO_ERROR) {
            if (status == PERMISSION_DENIED) {
                AutoCallerClear acc;
                mAudioPolicyManager->releaseInput(*input, session);
            }
            return status;
        }

        sp<AudioRecordClient> client = new AudioRecordClient(*attr, *input, uid, pid, opPackageName, session);
        client->active = false;
        client->isConcurrent = false;
        client->isVirtualDevice = false; //TODO : update from APM->getInputForAttr()
        client->deviceId = *selectedDeviceId;
        mAudioRecordClients.add(*portId, client);
    }

    if (audioPolicyEffects != 0) {
        // create audio pre processors according to input source
        status_t status = audioPolicyEffects->addInputEffects(*input, attr->source, session);
        if (status != NO_ERROR && status != ALREADY_EXISTS) {
            ALOGW("Failed to add effects on input %d", *input);
        }
    }
    return NO_ERROR;
}

/**
 * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
*/
status_t AudioPolicyManager::getInputForAttr(const audio_attributes_t *attr,
                                             audio_io_handle_t *input,
                                             audio_session_t session,
                                             uid_t uid,
                                             const audio_config_base_t *config,
                                             audio_input_flags_t flags,
                                             audio_port_handle_t *selectedDeviceId,
                                             input_type_t *inputType,
                                             audio_port_handle_t *portId)
{
    ALOGV("getInputForAttr() source %d, sampling rate %d, format %#x, channel mask %#x,"
            "session %d, flags %#x",
          attr->source, config->sample_rate, config->format, config->channel_mask, session, flags);

    status_t status = NO_ERROR;
    // handle legacy remote submix case where the address was not always specified
    String8 address = String8("");
    audio_source_t halInputSource;
    audio_source_t inputSource = attr->source;
    AudioMix *policyMix = NULL;
    DeviceVector inputDevices;

    if (inputSource == AUDIO_SOURCE_DEFAULT) {
        inputSource = AUDIO_SOURCE_MIC;
    }

    // Explicit routing?
    sp<DeviceDescriptor> deviceDesc;
    if (*selectedDeviceId != AUDIO_PORT_HANDLE_NONE) {
        deviceDesc = mAvailableInputDevices.getDeviceFromId(*selectedDeviceId);
    }
    mInputRoutes.addRoute(session, SessionRoute::STREAM_TYPE_NA, inputSource, deviceDesc, uid);

    // special case for mmap capture: if an input IO handle is specified, we reuse this input if
    // possible
    if ((flags & AUDIO_INPUT_FLAG_MMAP_NOIRQ) == AUDIO_INPUT_FLAG_MMAP_NOIRQ &&
            *input != AUDIO_IO_HANDLE_NONE) {
        ssize_t index = mInputs.indexOfKey(*input);
        if (index < 0) {
            ALOGW("getInputForAttr() unknown MMAP input %d", *input);
            status = BAD_VALUE;
            goto error;
        }
        sp<AudioInputDescriptor> inputDesc = mInputs.valueAt(index);
        sp<AudioSession> audioSession = inputDesc->getAudioSession(session);
        if (audioSession == 0) {
            ALOGW("getInputForAttr() unknown session %d on input %d", session, *input);
            status = BAD_VALUE;
            goto error;
        }
        // For MMAP mode, the first call to getInputForAttr() is made on behalf of audioflinger.
        // The second call is for the first active client and sets the UID. Any further call
        // corresponds to a new client and is only permitted from the same UID.
        // If the first UID is silenced, allow a new UID connection and replace with new UID
        if (audioSession->openCount() == 1) {
            audioSession->setUid(uid);
        } else if (audioSession->uid() != uid) {
            if (!audioSession->isSilenced()) {
                ALOGW("getInputForAttr() bad uid %d for session %d uid %d",
                      uid, session, audioSession->uid());
                status = INVALID_OPERATION;
                goto error;
            }
            audioSession->setUid(uid);
            audioSession->setSilenced(false);
        }
        audioSession->changeOpenCount(1);
        *inputType = API_INPUT_LEGACY;
        if (*portId == AUDIO_PORT_HANDLE_NONE) {
            *portId = AudioPort::getNextUniqueId();
        }
        inputDevices = mAvailableInputDevices.getDevicesFromType(inputDesc->mDevice);
        *selectedDeviceId = inputDevices.size() > 0 ? inputDevices.itemAt(0)->getId()
                : AUDIO_PORT_HANDLE_NONE;
        ALOGI("%s reusing MMAP input %d for session %d", __FUNCTION__, *input, session);

        return NO_ERROR;
    }

    *input = AUDIO_IO_HANDLE_NONE;
    *inputType = API_INPUT_INVALID;

    halInputSource = inputSource;

    // TODO: check for existing client for this port ID
    if (*portId == AUDIO_PORT_HANDLE_NONE) {
        *portId = AudioPort::getNextUniqueId();
    }

    audio_devices_t device;

    ALOGV("getInputForAttr() tags %s",attr->tags);

    if (inputSource == AUDIO_SOURCE_REMOTE_SUBMIX &&
            strncmp(attr->tags, "addr=", strlen("addr=")) == 0) {
        status = mPolicyMixes.getInputMixForAttr(*attr, &policyMix);
        if (status != NO_ERROR) {
            goto error;
        }
        *inputType = API_INPUT_MIX_EXT_POLICY_REROUTE;
        device = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
        address = String8(attr->tags + strlen("addr="));
    } else {
        device = getDeviceAndMixForInputSource(inputSource, &policyMix);
        ALOGV("getInputForAttr() device %d",device);
        if (device == AUDIO_DEVICE_NONE) {
            ALOGW("getInputForAttr() could not find device for source %d", inputSource);
            status = BAD_VALUE;
            goto error;
        }
        if (policyMix != NULL) {
            address = policyMix->mDeviceAddress;
            ALOGV("getInputForAttr() address %s mMixType %d",address.c_str(),policyMix->mMixType );
            if (policyMix->mMixType == MIX_TYPE_RECORDERS) {
                // there is an external policy, but this input is attached to a mix of recorders,
                // meaning it receives audio injected into the framework, so the recorder doesn't
                // know about it and is therefore considered "legacy"
                *inputType = API_INPUT_LEGACY;
            } else {
                // recording a mix of players defined by an external policy, we're rerouting for
                // an external policy
                *inputType = API_INPUT_MIX_EXT_POLICY_REROUTE;
            }
        } else if (audio_is_remote_submix_device(device)) {
            address = String8("0");
            *inputType = API_INPUT_MIX_CAPTURE;
        } else if (device == AUDIO_DEVICE_IN_TELEPHONY_RX) {
            *inputType = API_INPUT_TELEPHONY_RX;
        } else {
            *inputType = API_INPUT_LEGACY;
        }

    }

    *input = getInputForDevice(device, address, session, uid, inputSource,
                               config, flags,
                               policyMix);
    if (*input == AUDIO_IO_HANDLE_NONE) {
        status = INVALID_OPERATION;
        goto error;
    }

    inputDevices = mAvailableInputDevices.getDevicesFromType(device);
    *selectedDeviceId = inputDevices.size() > 0 ? inputDevices.itemAt(0)->getId()
            : AUDIO_PORT_HANDLE_NONE;

    ALOGV("getInputForAttr() returns input %d type %d selectedDeviceId %d",
            *input, *inputType, *selectedDeviceId);

    return NO_ERROR;

error:
    mInputRoutes.removeRoute(session);
    return status;
}














//////////////////////////////////////////////////////////////////////////////////////////////
status_t AudioRecord::start(AudioSystem::sync_event_t event, audio_session_t triggerSession)
{
    ALOGV("start, sync event %d trigger session %d", event, triggerSession);
    /*
	if(mServiceProxy != NULL && mServiceProxy->isValid()){
		mServiceProxy->start();
		if(mTransfer == TRANSFER_SYNC){
			return NO_ERROR;
		}
	}
    */
    AutoMutex lock(mLock);
    if (mActive) {
        return NO_ERROR;
    }

    // discard data in buffer
    const uint32_t framesFlushed = mProxy->flush();
    mFramesReadServerOffset -= mFramesRead + framesFlushed;
    mFramesRead = 0;
    mProxy->clearTimestamp();  // timestamp is invalid until next server push

    // reset current position as seen by client to 0
    mProxy->setEpoch(mProxy->getEpoch() - mProxy->getPosition());
    // force refresh of remaining frames by processAudioBuffer() as last
    // read before stop could be partial.
    mRefreshRemaining = true;

    mNewPosition = mProxy->getPosition() + mUpdatePeriod;
    int32_t flags = android_atomic_acquire_load(&mCblk->mFlags);

    // we reactivate markers (mMarkerPosition != 0) as the position is reset to 0.
    // This is legacy behavior.  This is not done in stop() to avoid a race condition
    // where the last marker event is issued twice.
    mMarkerReached = false;
    mActive = true;

    status_t status = NO_ERROR;
    if (!(flags & CBLK_INVALID)) {
        status = mAudioRecord->start(event, triggerSession).transactionError();
        if (status == DEAD_OBJECT) {
            flags |= CBLK_INVALID;
        }
    }
    if (flags & CBLK_INVALID) {
        status = restoreRecord_l("start");
    }

    if (status != NO_ERROR) {
        mActive = false;
        ALOGE("start() status %d", status);
    } else {
        sp<AudioRecordThread> t = mAudioRecordThread;
        if (t != 0) {
            t->resume();
        } else {
            mPreviousPriority = getpriority(PRIO_PROCESS, 0);
            get_sched_policy(0, &mPreviousSchedulingGroup);
            androidSetThreadPriority(0, ANDROID_PRIORITY_AUDIO);
        }

        // we've successfully started, log that time
        mMediaMetrics.logStart(systemTime());
    }

    if (status != NO_ERROR) {
        mMediaMetrics.markError(status, __FUNCTION__);
    }
    return status;
}














