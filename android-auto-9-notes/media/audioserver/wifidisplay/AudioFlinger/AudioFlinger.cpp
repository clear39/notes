//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audioflinger/AudioFlinger.cpp

/**
 * 
 * 
*/
status_t AudioFlinger::openInput(audio_module_handle_t module,
                                          audio_io_handle_t *input,
                                          audio_config_t *config,
                                          audio_devices_t *devices,
                                          const String8& address,
                                          audio_source_t source,
                                          audio_input_flags_t flags)
{
    Mutex::Autolock _l(mLock);

    if (*devices == AUDIO_DEVICE_NONE) {
        return BAD_VALUE;
    }

    ALOGI("openInput() this %p, module %d Device %#x, SamplingRate %d, Format %#08x, "
                  "Channels %#x, flags %#x,addr %s",
                  this, module,
                  (devices != NULL) ? *devices : 0,
                  config->sample_rate,
                  config->format,
                  config->channel_mask,
                  flags,
                  address.string());

    sp<ThreadBase> thread = openInput_l(module, input, config, *devices, address, source, flags);

    if (thread != 0) {
        // notify client processes of the new input creation
        thread->ioConfigChanged(AUDIO_INPUT_OPENED);
        return NO_ERROR;
    }
    return NO_INIT;
}

sp<AudioFlinger::ThreadBase> AudioFlinger::openInput_l(audio_module_handle_t module,
                                                         audio_io_handle_t *input,
                                                         audio_config_t *config,
                                                         audio_devices_t devices,
                                                         const String8& address,
                                                         audio_source_t source,
                                                         audio_input_flags_t flags)
{
    AudioHwDevice *inHwDev = findSuitableHwDev_l(module, devices);
    if (inHwDev == NULL) {
        *input = AUDIO_IO_HANDLE_NONE;
        return 0;
    }

    // Audio Policy can request a specific handle for hardware hotword.
    // The goal here is not to re-open an already opened input.
    // It is to use a pre-assigned I/O handle.
    if (*input == AUDIO_IO_HANDLE_NONE) {
        *input = nextUniqueId(AUDIO_UNIQUE_ID_USE_INPUT);
    } else if (audio_unique_id_get_use(*input) != AUDIO_UNIQUE_ID_USE_INPUT) {
        ALOGE("openInput_l() requested input handle %d is invalid", *input);
        return 0;
    } else if (mRecordThreads.indexOfKey(*input) >= 0) {
        // This should not happen in a transient state with current design.
        ALOGE("openInput_l() requested input handle %d is already assigned", *input);
        return 0;
    }

    audio_config_t halconfig = *config;
    sp<DeviceHalInterface> inHwHal = inHwDev->hwDevice();
    sp<StreamInHalInterface> inStream;
    status_t status = inHwHal->openInputStream(
            *input, devices, &halconfig, flags, address.string(), source, &inStream);
    ALOGV("openInput_l() openInputStream returned input %p, devices %#x, SamplingRate %d"
           ", Format %#x, Channels %#x, flags %#x, status %d addr %s",
            inStream.get(),
            devices,
            halconfig.sample_rate,
            halconfig.format,
            halconfig.channel_mask,
            flags,
            status, address.string());

    // If the input could not be opened with the requested parameters and we can handle the
    // conversion internally, try to open again with the proposed parameters.
    if (status == BAD_VALUE &&
        audio_is_linear_pcm(config->format) &&
        audio_is_linear_pcm(halconfig.format) &&
        (halconfig.sample_rate <= AUDIO_RESAMPLER_DOWN_RATIO_MAX * config->sample_rate) &&
        (audio_channel_count_from_in_mask(halconfig.channel_mask) <= FCC_8) &&
        (audio_channel_count_from_in_mask(config->channel_mask) <= FCC_8)) {
        // FIXME describe the change proposed by HAL (save old values so we can log them here)
        ALOGV("openInput_l() reopening with proposed sampling rate and channel mask");
        inStream.clear();
        status = inHwHal->openInputStream(
                *input, devices, &halconfig, flags, address.string(), source, &inStream);
        // FIXME log this new status; HAL should not propose any further changes
    }

    if (status == NO_ERROR && inStream != 0) {
        AudioStreamIn *inputStream = new AudioStreamIn(inHwDev, inStream, flags);
        if ((flags & AUDIO_INPUT_FLAG_MMAP_NOIRQ) != 0) {
            sp<MmapCaptureThread> thread =
                    new MmapCaptureThread(this, *input,
                                          inHwDev, inputStream,
                                          primaryOutputDevice_l(), devices, mSystemReady);
            mMmapThreads.add(*input, thread);
            ALOGV("openInput_l() created mmap capture thread: ID %d thread %p", *input,
                    thread.get());
            return thread;
        } else {
#ifdef TEE_SINK
            // Try to re-use most recently used Pipe to archive a copy of input for dumpsys,
            // or (re-)create if current Pipe is idle and does not match the new format
            sp<NBAIO_Sink> teeSink;
            enum {
                TEE_SINK_NO,    // don't copy input
                TEE_SINK_NEW,   // copy input using a new pipe
                TEE_SINK_OLD,   // copy input using an existing pipe
            } kind;
            NBAIO_Format format = Format_from_SR_C(halconfig.sample_rate,
                    audio_channel_count_from_in_mask(halconfig.channel_mask), halconfig.format);
            if (!mTeeSinkInputEnabled) {
                kind = TEE_SINK_NO;
            } else if (!Format_isValid(format)) {
                kind = TEE_SINK_NO;
            } else if (mRecordTeeSink == 0) {
                kind = TEE_SINK_NEW;
            } else if (mRecordTeeSink->getStrongCount() != 1) {
                kind = TEE_SINK_NO;
            } else if (Format_isEqual(format, mRecordTeeSink->format())) {
                kind = TEE_SINK_OLD;
            } else {
                kind = TEE_SINK_NEW;
            }
            switch (kind) {
            case TEE_SINK_NEW: {
                Pipe *pipe = new Pipe(mTeeSinkInputFrames, format);
                size_t numCounterOffers = 0;
                const NBAIO_Format offers[1] = {format};
                ssize_t index = pipe->negotiate(offers, 1, NULL, numCounterOffers);
                ALOG_ASSERT(index == 0);
                PipeReader *pipeReader = new PipeReader(*pipe);
                numCounterOffers = 0;
                index = pipeReader->negotiate(offers, 1, NULL, numCounterOffers);
                ALOG_ASSERT(index == 0);
                mRecordTeeSink = pipe;
                mRecordTeeSource = pipeReader;
                teeSink = pipe;
                }
                break;
            case TEE_SINK_OLD:
                teeSink = mRecordTeeSink;
                break;
            case TEE_SINK_NO:
            default:
                break;
            }
#endif

            // Start record thread
            // RecordThread requires both input and output device indication to forward to audio
            // pre processing modules
            sp<RecordThread> thread = new RecordThread(this,
                                      inputStream,
                                      *input,
                                      primaryOutputDevice_l(),
                                      devices,
                                      mSystemReady
#ifdef TEE_SINK
                                      , teeSink
#endif
                                      );
            mRecordThreads.add(*input, thread);
            ALOGV("openInput_l() created record thread: ID %d thread %p", *input, thread.get());
            return thread;
        }
    }

    *input = AUDIO_IO_HANDLE_NONE;
    return 0;
}




















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
        ALOGW_IF(clientUid != callingUid,
                "%s uid %d tried to pass itself off as %d",
                __FUNCTION__, callingUid, clientUid);
        clientUid = callingUid;
        updatePid = true;
    }
    pid_t clientPid = input.clientInfo.clientPid;
    if (updatePid) {
        const pid_t callingPid = IPCThreadState::self()->getCallingPid();
        ALOGW_IF(clientPid != -1 && clientPid != callingPid,
                 "%s uid %d pid %d tried to pass itself off as pid %d",
                 __func__, callingUid, callingPid, clientPid);
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
    /**
     * 
     * AudioSystem::getInputForAttr 会触发 AudioFlinger::openInput_l 调用 创建线程
     * 
     * 这里主要得到 output.inputId，output.selectedDeviceId， portId 值
     * 
     * output.inputId 是由 AudioFlinger::openInput_l 创建用于标记 创建的对应线程
     * output.selectedDeviceId 
     * portId
     * 
    */
    lStatus = AudioSystem::getInputForAttr(&input.attr, &output.inputId,
                                      sessionId,   // 已经创建
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
                                                  &output.frameCount, sessionId,    // 已经创建 
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





audio_io_handle_t AudioFlinger::openDuplicateOutput(audio_io_handle_t output1,audio_io_handle_t output2)
{
    ALOGV("openDuplicateOutput() output1:%#x output2:%#x",output1,output2);

    Mutex::Autolock _l(mLock);
    MixerThread *thread1 = checkMixerThread_l(output1);
    MixerThread *thread2 = checkMixerThread_l(output2);

    if (thread1 == NULL || thread2 == NULL) {
        ALOGW("openDuplicateOutput() wrong output mixer type for output %d or %d", output1,output2);
        return AUDIO_IO_HANDLE_NONE;
    }

    audio_io_handle_t id = nextUniqueId(AUDIO_UNIQUE_ID_USE_OUTPUT);
    DuplicatingThread *thread = new DuplicatingThread(this, thread1, id, mSystemReady);
    thread->addOutputTrack(thread2);
    mPlaybackThreads.add(id, thread);
    // notify client processes of the new output creation
    thread->ioConfigChanged(AUDIO_OUTPUT_OPENED);
    return id;
}