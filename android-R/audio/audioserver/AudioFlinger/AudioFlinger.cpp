
// @ frameworks/av/services/audioflinger/AudioFlinger.cpp

sp<IAudioTrack> AudioFlinger::createTrack(const CreateTrackInput& input, CreateTrackOutput& output, status_t *status)
{
    sp<PlaybackThread::Track> track;
    sp<TrackHandle> trackHandle;
    sp<Client> client;
    status_t lStatus;
    audio_stream_type_t streamType;
    audio_port_handle_t portId = AUDIO_PORT_HANDLE_NONE;
    std::vector<audio_io_handle_t> secondaryOutputs;

    bool updatePid = (input.clientInfo.clientPid == -1);
    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    uid_t clientUid = input.clientInfo.clientUid;
    audio_io_handle_t effectThreadId = AUDIO_IO_HANDLE_NONE;
    std::vector<int> effectIds;
    audio_attributes_t localAttr = input.attr;

    if (!isAudioServerOrMediaServerUid(callingUid)) {
        ALOGW_IF(clientUid != callingUid,
                "%s uid %d tried to pass itself off as %d",
                __FUNCTION__, callingUid, clientUid);
        clientUid = callingUid;
        updatePid = true;
    }
    pid_t clientPid = input.clientInfo.clientPid;
    const pid_t callingPid = IPCThreadState::self()->getCallingPid();
    if (updatePid) {
        ALOGW_IF(clientPid != -1 && clientPid != callingPid,
                 "%s uid %d pid %d tried to pass itself off as pid %d", __func__, callingUid, callingPid, clientPid);
        clientPid = callingPid;
    }

    audio_session_t sessionId = input.sessionId;
    if (sessionId == AUDIO_SESSION_ALLOCATE) {
        sessionId = (audio_session_t) newAudioUniqueId(AUDIO_UNIQUE_ID_USE_SESSION);
    } else if (audio_unique_id_get_use(sessionId) != AUDIO_UNIQUE_ID_USE_SESSION) {
        lStatus = BAD_VALUE;
        goto Exit;
    }

    output.sessionId = sessionId;
    output.outputId = AUDIO_IO_HANDLE_NONE;
    output.selectedDeviceId = input.selectedDeviceId;
    lStatus = AudioSystem::getOutputForAttr(&localAttr, &output.outputId, sessionId, &streamType,
                                            clientPid, clientUid, &input.config, input.flags,
                                            &output.selectedDeviceId, &portId, &secondaryOutputs);

    if (lStatus != NO_ERROR || output.outputId == AUDIO_IO_HANDLE_NONE) {
        ALOGE("createTrack() getOutputForAttr() return error %d or invalid output handle", lStatus);
        goto Exit;
    }
    // client AudioTrack::set already implements AUDIO_STREAM_DEFAULT => AUDIO_STREAM_MUSIC,
    // but if someone uses binder directly they could bypass that and cause us to crash
    if (uint32_t(streamType) >= AUDIO_STREAM_CNT) {
        ALOGE("createTrack() invalid stream type %d", streamType);
        lStatus = BAD_VALUE;
        goto Exit;
    }

    // further channel mask checks are performed by createTrack_l() depending on the thread type
    if (!audio_is_output_channel(input.config.channel_mask)) {
        ALOGE("createTrack() invalid channel mask %#x", input.config.channel_mask);
        lStatus = BAD_VALUE;
        goto Exit;
    }

    // further format checks are performed by createTrack_l() depending on the thread type
    if (!audio_is_valid_format(input.config.format)) {
        ALOGE("createTrack() invalid format %#x", input.config.format);
        lStatus = BAD_VALUE;
        goto Exit;
    }

    {
        Mutex::Autolock _l(mLock);
        PlaybackThread *thread = checkPlaybackThread_l(output.outputId);
        if (thread == NULL) {
            ALOGE("no playback thread found for output handle %d", output.outputId);
            lStatus = BAD_VALUE;
            goto Exit;
        }

        client = registerPid(clientPid);

        PlaybackThread *effectThread = NULL;
        // check if an effect chain with the same session ID is present on another
        // output thread and move it here.
        for (size_t i = 0; i < mPlaybackThreads.size(); i++) {
            sp<PlaybackThread> t = mPlaybackThreads.valueAt(i);
            if (mPlaybackThreads.keyAt(i) != output.outputId) {
                uint32_t sessions = t->hasAudioSession(sessionId);
                if (sessions & ThreadBase::EFFECT_SESSION) {
                    effectThread = t.get();
                    break;
                }
            }
        }
        ALOGV("createTrack() sessionId: %d", sessionId);

        output.sampleRate = input.config.sample_rate;
        output.frameCount = input.frameCount;
        output.notificationFrameCount = input.notificationFrameCount;
        output.flags = input.flags;

        track = thread->createTrack_l(client, streamType, localAttr, &output.sampleRate,
                                      input.config.format, input.config.channel_mask,
                                      &output.frameCount, &output.notificationFrameCount,
                                      input.notificationsPerBuffer, input.speed,
                                      input.sharedBuffer, sessionId, &output.flags,
                                      callingPid, input.clientInfo.clientTid, clientUid,
                                      &lStatus, portId, input.audioTrackCallback);

        LOG_ALWAYS_FATAL_IF((lStatus == NO_ERROR) && (track == 0));
        // we don't abort yet if lStatus != NO_ERROR; there is still work to be done regardless

        output.afFrameCount = thread->frameCount();
        output.afSampleRate = thread->sampleRate();
        output.afLatencyMs = thread->latency();
        output.portId = portId;

        if (lStatus == NO_ERROR) {
            // Connect secondary outputs. Failure on a secondary output must not imped the primary
            // Any secondary output setup failure will lead to a desync between the AP and AF until
            // the track is destroyed.
            TeePatches teePatches;
            for (audio_io_handle_t secondaryOutput : secondaryOutputs) {
                PlaybackThread *secondaryThread = checkPlaybackThread_l(secondaryOutput);
                if (secondaryThread == NULL) {
                    ALOGE("no playback thread found for secondary output %d", output.outputId);
                    continue;
                }

                size_t sourceFrameCount = thread->frameCount() * output.sampleRate / thread->sampleRate();
                size_t sinkFrameCount = secondaryThread->frameCount() * output.sampleRate / secondaryThread->sampleRate();
                // If the secondary output has just been opened, the first secondaryThread write
                // will not block as it will fill the empty startup buffer of the HAL,
                // so a second sink buffer needs to be ready for the immediate next blocking write.
                // Additionally, have a margin of one main thread buffer as the scheduling jitter
                // can reorder the writes (eg if thread A&B have the same write intervale,
                // the scheduler could schedule AB...BA)
                size_t frameCountToBeReady = 2 * sinkFrameCount + sourceFrameCount;
                // Total secondary output buffer must be at least as the read frames plus
                // the margin of a few buffers on both sides in case the
                // threads scheduling has some jitter.
                // That value should not impact latency as the secondary track is started before
                // its buffer is full, see frameCountToBeReady.
                size_t frameCount = frameCountToBeReady + 2 * (sourceFrameCount + sinkFrameCount);
                // The frameCount should also not be smaller than the secondary thread min frame
                // count
                size_t minFrameCount = AudioSystem::calculateMinFrameCount(
                            [&] { Mutex::Autolock _l(secondaryThread->mLock);
                                  return secondaryThread->latency_l(); }(),
                            secondaryThread->mNormalFrameCount,
                            secondaryThread->mSampleRate,
                            output.sampleRate,
                            input.speed);
                frameCount = std::max(frameCount, minFrameCount);

                using namespace std::chrono_literals;
                auto inChannelMask = audio_channel_mask_out_to_in(input.config.channel_mask);
                sp patchRecord = new RecordThread::PatchRecord(nullptr /* thread */,
                                                               output.sampleRate,
                                                               inChannelMask,
                                                               input.config.format,
                                                               frameCount,
                                                               NULL /* buffer */,
                                                               (size_t)0 /* bufferSize */,
                                                               AUDIO_INPUT_FLAG_DIRECT,
                                                               0ns /* timeout */);
                status_t status = patchRecord->initCheck();
                if (status != NO_ERROR) {
                    ALOGE("Secondary output patchRecord init failed: %d", status);
                    continue;
                }

                // TODO: We could check compatibility of the secondaryThread with the PatchTrack
                // for fast usage: thread has fast mixer, sample rate matches, etc.;
                // for now, we exclude fast tracks by removing the Fast flag.
                const audio_output_flags_t outputFlags =
                        (audio_output_flags_t)(output.flags & ~AUDIO_OUTPUT_FLAG_FAST);
                sp patchTrack = new PlaybackThread::PatchTrack(secondaryThread,
                                                               streamType,
                                                               output.sampleRate,
                                                               input.config.channel_mask,
                                                               input.config.format,
                                                               frameCount,
                                                               patchRecord->buffer(),
                                                               patchRecord->bufferSize(),
                                                               outputFlags,
                                                               0ns /* timeout */,
                                                               frameCountToBeReady);
                status = patchTrack->initCheck();
                if (status != NO_ERROR) {
                    ALOGE("Secondary output patchTrack init failed: %d", status);
                    continue;
                }
                teePatches.push_back({patchRecord, patchTrack});
                secondaryThread->addPatchTrack(patchTrack);
                // In case the downstream patchTrack on the secondaryThread temporarily outlives
                // our created track, ensure the corresponding patchRecord is still alive.
                patchTrack->setPeerProxy(patchRecord, true /* holdReference */);
                patchRecord->setPeerProxy(patchTrack, false /* holdReference */);
            }
            track->setTeePatches(std::move(teePatches));
        }

        // move effect chain to this output thread if an effect on same session was waiting
        // for a track to be created
        if (lStatus == NO_ERROR && effectThread != NULL) {
            // no risk of deadlock because AudioFlinger::mLock is held
            Mutex::Autolock _dl(thread->mLock);
            Mutex::Autolock _sl(effectThread->mLock);
            if (moveEffectChain_l(sessionId, effectThread, thread) == NO_ERROR) {
                effectThreadId = thread->id();
                effectIds = thread->getEffectIds_l(sessionId);
            }
        }

        // Look for sync events awaiting for a session to be used.
        for (size_t i = 0; i < mPendingSyncEvents.size(); i++) {
            if (mPendingSyncEvents[i]->triggerSession() == sessionId) {
                if (thread->isValidSyncEvent(mPendingSyncEvents[i])) {
                    if (lStatus == NO_ERROR) {
                        (void) track->setSyncEvent(mPendingSyncEvents[i]);
                    } else {
                        mPendingSyncEvents[i]->cancel();
                    }
                    mPendingSyncEvents.removeAt(i);
                    i--;
                }
            }
        }

        setAudioHwSyncForSession_l(thread, sessionId);
    }

    if (lStatus != NO_ERROR) {
        // remove local strong reference to Client before deleting the Track so that the
        // Client destructor is called by the TrackBase destructor with mClientLock held
        // Don't hold mClientLock when releasing the reference on the track as the
        // destructor will acquire it.
        {
            Mutex::Autolock _cl(mClientLock);
            client.clear();
        }
        track.clear();
        goto Exit;
    }

    // effectThreadId is not NONE if an effect chain corresponding to the track session
    // was found on another thread and must be moved on this thread
    if (effectThreadId != AUDIO_IO_HANDLE_NONE) {
        AudioSystem::moveEffectsToIo(effectIds, effectThreadId);
    }

    // return handle to client
    trackHandle = new TrackHandle(track);

Exit:
    if (lStatus != NO_ERROR && output.outputId != AUDIO_IO_HANDLE_NONE) {
        AudioSystem::releaseOutput(portId);
    }
    *status = lStatus;
    return trackHandle;
}