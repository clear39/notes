class PlaybackThread : public ThreadBase, public StreamOutHalInterfaceCallback,
    public VolumeInterface {

}


class MixerThread : public PlaybackThread {

}



//  @   frameworks/av/services/audioflinger/Threads.cpp
/**
 * type_t 标记 PlaybackThread 类型 @ /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audioflinger/Threads.h 
*/
AudioFlinger::MixerThread::MixerThread(const sp<AudioFlinger>& audioFlinger, AudioStreamOut* output,
        audio_io_handle_t id, audio_devices_t device, bool systemReady, type_t type /*= MIXER*/)
    :   PlaybackThread(audioFlinger, output, id, device, type, systemReady),
        // mAudioMixer below
        // mFastMixer below
        mFastMixerFutex(0),
        mMasterMono(false)
        // mOutputSink below
        // mPipeSink below
        // mNormalSink below
{
    ALOGV("MixerThread() id=%d device=%#x type=%d", id, device, type);
    ALOGV("mSampleRate=%u, mChannelMask=%#x, mChannelCount=%u, mFormat=%#x, mFrameSize=%zu, "
            "mFrameCount=%zu, mNormalFrameCount=%zu",
            mSampleRate, mChannelMask, mChannelCount, mFormat, mFrameSize, mFrameCount,
            mNormalFrameCount);



    mAudioMixer = new AudioMixer(mNormalFrameCount, mSampleRate);

    if (type == DUPLICATING) {
        // The Duplicating thread uses the AudioMixer and delivers data to OutputTracks
        // (downstream MixerThreads) in DuplicatingThread::threadLoop_write().
        // Do not create or use mFastMixer, mOutputSink, mPipeSink, or mNormalSink.
        return;
    }

    // create an NBAIO sink for the HAL output stream, and negotiate
    mOutputSink = new AudioStreamOutSink(output->stream);

    size_t numCounterOffers = 0;
    const NBAIO_Format offers[1] = {Format_from_SR_C(mSampleRate, mChannelCount, mFormat)};
#if !LOG_NDEBUG
    ssize_t index =
#else
    (void)
#endif
            mOutputSink->negotiate(offers, 1, NULL, numCounterOffers);
    
    ALOG_ASSERT(index == 0);

    // initialize fast mixer depending on configuration
    bool initFastMixer;
    
    switch (kUseFastMixer) { // 这里 kUseFastMixer = FastMixer_Static
    case FastMixer_Never:
        initFastMixer = false;
        break;
    case FastMixer_Always:
        initFastMixer = true;
        break;
    case FastMixer_Static:
    case FastMixer_Dynamic:
        //执行这里 frameworks/av/services/audioflinger/Threads.cpp:171:} kUseFastMixer = FastMixer_Static;
        // FastMixer was designed to operate with a HAL that pulls at a regular rate,
        // where the period is less than an experimentally determined threshold that can be
        // scheduled reliably with CFS. However, the BT A2DP HAL is
        // bursty (does not pull at a regular rate) and so cannot operate with FastMixer.
        initFastMixer = mFrameCount < mNormalFrameCount && (mOutDevice & AUDIO_DEVICE_OUT_ALL_A2DP) == 0;
        break;
    }

    ALOGW_IF(initFastMixer == false && mFrameCount < mNormalFrameCount, "FastMixer is preferred for this sink as frameCount %zu is less than threshold %zu", mFrameCount, mNormalFrameCount);
   

    /**
     * 通过dumpsys 命令打印  No FastMixer 所以 initFastMixer 为false
    */
   if (initFastMixer) {
        audio_format_t fastMixerFormat;
        if (mMixerBufferEnabled && mEffectBufferEnabled) {
            fastMixerFormat = AUDIO_FORMAT_PCM_FLOAT;
        } else {
            fastMixerFormat = AUDIO_FORMAT_PCM_16_BIT;
        }
        if (mFormat != fastMixerFormat) {
            // change our Sink format to accept our intermediate precision
            mFormat = fastMixerFormat;
            free(mSinkBuffer);
            mFrameSize = mChannelCount * audio_bytes_per_sample(mFormat);
            const size_t sinkBufferSize = mNormalFrameCount * mFrameSize;
            // 调用posix_memalign( )成功时会返回size字节的动态内存，并且这块内存的地址是alignment的倍数。参数alignment必须是2的幂，还是void指针的大小的倍数。返回的内存块的地址放在了memptr里面，函数返回值是0
            (void)posix_memalign(&mSinkBuffer, 32, sinkBufferSize);
        }

        // create a MonoPipe to connect our submix to FastMixer
        NBAIO_Format format = mOutputSink->format();
#ifdef TEE_SINK
        NBAIO_Format origformat = format;
#endif
        // adjust format to match that of the Fast Mixer
        ALOGV("format changed from %#x to %#x", format.mFormat, fastMixerFormat);
        format.mFormat = fastMixerFormat;
        format.mFrameSize = audio_bytes_per_sample(format.mFormat) * format.mChannelCount;

        // This pipe depth compensates for scheduling latency of the normal mixer thread.
        // When it wakes up after a maximum latency, it runs a few cycles quickly before
        // finally blocking.  Note the pipe implementation rounds up the request to a power of 2.
        MonoPipe *monoPipe = new MonoPipe(mNormalFrameCount * 4, format, true /*writeCanBlock*/);
        const NBAIO_Format offers[1] = {format};
        size_t numCounterOffers = 0;
#if !LOG_NDEBUG || defined(TEE_SINK)
        ssize_t index =
#else
        (void)
#endif
                monoPipe->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        monoPipe->setAvgFrames((mScreenState & 1) ? (monoPipe->maxFrames() * 7) / 8 : mNormalFrameCount * 2);
        mPipeSink = monoPipe;

#ifdef TEE_SINK
        if (mTeeSinkOutputEnabled) {
            // create a Pipe to archive a copy of FastMixer's output for dumpsys
            Pipe *teeSink = new Pipe(mTeeSinkOutputFrames, origformat);
            const NBAIO_Format offers2[1] = {origformat};
            numCounterOffers = 0;
            index = teeSink->negotiate(offers2, 1, NULL, numCounterOffers);
            ALOG_ASSERT(index == 0);
            mTeeSink = teeSink;
            PipeReader *teeSource = new PipeReader(*teeSink);
            numCounterOffers = 0;
            index = teeSource->negotiate(offers2, 1, NULL, numCounterOffers);
            ALOG_ASSERT(index == 0);
            mTeeSource = teeSource;
        }
#endif

        // create fast mixer and configure it initially with just one fast track for our submix
        mFastMixer = new FastMixer();
        FastMixerStateQueue *sq = mFastMixer->sq();
#ifdef STATE_QUEUE_DUMP
        sq->setObserverDump(&mStateQueueObserverDump);
        sq->setMutatorDump(&mStateQueueMutatorDump);
#endif
        FastMixerState *state = sq->begin();
        FastTrack *fastTrack = &state->mFastTracks[0];
        // wrap the source side of the MonoPipe to make it an AudioBufferProvider
        fastTrack->mBufferProvider = new SourceAudioBufferProvider(new MonoPipeReader(monoPipe));
        fastTrack->mVolumeProvider = NULL;
        fastTrack->mChannelMask = mChannelMask; // mPipeSink channel mask for audio to FastMixer
        fastTrack->mFormat = mFormat; // mPipeSink format for audio to FastMixer
        fastTrack->mGeneration++;
        state->mFastTracksGen++;
        state->mTrackMask = 1;
        // fast mixer will use the HAL output sink
        state->mOutputSink = mOutputSink.get();
        state->mOutputSinkGen++;
        state->mFrameCount = mFrameCount;
        state->mCommand = FastMixerState::COLD_IDLE;
        // already done in constructor initialization list
        //mFastMixerFutex = 0;
        state->mColdFutexAddr = &mFastMixerFutex;
        state->mColdGen++;
        state->mDumpState = &mFastMixerDumpState;
#ifdef TEE_SINK
        state->mTeeSink = mTeeSink.get();
#endif
        
        mFastMixerNBLogWriter = audioFlinger->newWriter_l(kFastMixerLogSize, "FastMixer");

        state->mNBLogWriter = mFastMixerNBLogWriter.get();
        sq->end();
        sq->push(FastMixerStateQueue::BLOCK_UNTIL_PUSHED);

        // start the fast mixer
        mFastMixer->run("FastMixer", PRIORITY_URGENT_AUDIO);

        pid_t tid = mFastMixer->getTid();

        sendPrioConfigEvent(getpid_cached, tid, kPriorityFastMixer, false /*forApp*/);

        stream()->setHalThreadPriority(kPriorityFastMixer);

#ifdef AUDIO_WATCHDOG
        // create and start the watchdog
        mAudioWatchdog = new AudioWatchdog();
        mAudioWatchdog->setDump(&mAudioWatchdogDump);
        mAudioWatchdog->run("AudioWatchdog", PRIORITY_URGENT_AUDIO);
        tid = mAudioWatchdog->getTid();
        sendPrioConfigEvent(getpid_cached, tid, kPriorityFastMixer, false /*forApp*/);
#endif

    }// initFastMixer = true

    switch (kUseFastMixer) {
    case FastMixer_Never:
    case FastMixer_Dynamic:
        mNormalSink = mOutputSink;
        break;
    case FastMixer_Always:
        mNormalSink = mPipeSink;
        break;
    case FastMixer_Static: //   frameworks/av/services/audioflinger/Threads.cpp:171:} kUseFastMixer = FastMixer_Static;
        // 执行这里 
        mNormalSink = initFastMixer ? mPipeSink : mOutputSink;// false
        break;
    }




}




/**
 * sp<IAudioTrack> AudioFlinger::createTrack(const CreateTrackInput& input,CreateTrackOutput& output,status_t *status)
 * 
*/
sp<AudioFlinger::PlaybackThread::Track> AudioFlinger::PlaybackThread::createTrack_l(
        const sp<AudioFlinger::Client>& client,
        audio_stream_type_t streamType,
        const audio_attributes_t& attr,
        uint32_t *pSampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask,
        size_t *pFrameCount,
        size_t *pNotificationFrameCount,
        uint32_t notificationsPerBuffer,
        float speed,
        const sp<IMemory>& sharedBuffer,
        audio_session_t sessionId,
        audio_output_flags_t *flags,
        pid_t tid,
        uid_t uid,
        status_t *status,
        audio_port_handle_t portId)
{
    size_t frameCount = *pFrameCount;
    size_t notificationFrameCount = *pNotificationFrameCount;
    sp<Track> track;
    status_t lStatus;
    audio_output_flags_t outputFlags = mOutput->flags;
    audio_output_flags_t requestedFlags = *flags;

    if (*pSampleRate == 0) {
        *pSampleRate = mSampleRate;
    }
    uint32_t sampleRate = *pSampleRate;

    // special case for FAST flag considered OK if fast mixer is present
    /**
     * hasFastMixer 判断 mFastMixer 是否为空 
     * @    system/media/audio/include/system/audio-base.h:360:    AUDIO_OUTPUT_FLAG_FAST             = 0x4,
    */
    if (hasFastMixer()) {
        outputFlags = (audio_output_flags_t)(outputFlags | AUDIO_OUTPUT_FLAG_FAST);
    }

    // Check if requested flags are compatible with output stream flags
    if ((*flags & outputFlags) != *flags) {
        ALOGW("createTrack_l(): mismatch between requested flags (%08x) and output flags (%08x)", *flags, outputFlags);
        *flags = (audio_output_flags_t)(*flags & outputFlags);
    }

    // client expresses a preference for FAST, but we get the final say
    if (*flags & AUDIO_OUTPUT_FLAG_FAST) {
      if (
            // PCM data
            audio_is_linear_pcm(format) &&
            // TODO: extract as a data library function that checks that a computationally
            // expensive downmixer is not required: isFastOutputChannelConversion()
            (channelMask == mChannelMask ||
                    mChannelMask != AUDIO_CHANNEL_OUT_STEREO ||
                    (channelMask == AUDIO_CHANNEL_OUT_MONO
                            /* && mChannelMask == AUDIO_CHANNEL_OUT_STEREO */)) &&
            // hardware sample rate
            (sampleRate == mSampleRate) &&
            // normal mixer has an associated fast mixer
            hasFastMixer() &&
            // there are sufficient fast track slots available
            (mFastTrackAvailMask != 0)
            // FIXME test that MixerThread for this fast track has a capable output HAL
            // FIXME add a permission test also?
        ) {
        // static tracks can have any nonzero framecount, streaming tracks check against minimum.
        if (sharedBuffer == 0) {
            // read the fast track multiplier property the first time it is needed
            int ok = pthread_once(&sFastTrackMultiplierOnce, sFastTrackMultiplierInit);
            if (ok != 0) {
                ALOGE("%s pthread_once failed: %d", __func__, ok);
            }
            frameCount = max(frameCount, mFrameCount * sFastTrackMultiplier); // incl framecount 0
        }

        // check compatibility with audio effects.
        { // scope for mLock
            Mutex::Autolock _l(mLock);
            for (audio_session_t session : {
                    AUDIO_SESSION_OUTPUT_STAGE,
                    AUDIO_SESSION_OUTPUT_MIX,
                    sessionId,
                }) {
                sp<EffectChain> chain = getEffectChain_l(session);
                if (chain.get() != nullptr) {
                    audio_output_flags_t old = *flags;
                    chain->checkOutputFlagCompatibility(flags);
                    if (old != *flags) {
                        ALOGV("AUDIO_OUTPUT_FLAGS denied by effect, session=%d old=%#x new=%#x", (int)session, (int)old, (int)*flags);
                    }
                }
            }
        }
        ALOGV_IF((*flags & AUDIO_OUTPUT_FLAG_FAST) != 0,"AUDIO_OUTPUT_FLAG_FAST accepted: frameCount=%zu mFrameCount=%zu",frameCount, mFrameCount);
      } else {
        ALOGV("AUDIO_OUTPUT_FLAG_FAST denied: sharedBuffer=%p frameCount=%zu "
                "mFrameCount=%zu format=%#x mFormat=%#x isLinear=%d channelMask=%#x "
                "sampleRate=%u mSampleRate=%u "
                "hasFastMixer=%d tid=%d fastTrackAvailMask=%#x",
                sharedBuffer.get(), frameCount, mFrameCount, format, mFormat,
                audio_is_linear_pcm(format),
                channelMask, sampleRate, mSampleRate, hasFastMixer(), tid, mFastTrackAvailMask);
        *flags = (audio_output_flags_t)(*flags & ~AUDIO_OUTPUT_FLAG_FAST);
      }

    }

    if (!audio_has_proportional_frames(format)) {
        if (sharedBuffer != 0) {
            // Same comment as below about ignoring frameCount parameter for set()
            frameCount = sharedBuffer->size();
        } else if (frameCount == 0) {
            frameCount = mNormalFrameCount;
        }
        if (notificationFrameCount != frameCount) {
            notificationFrameCount = frameCount;
        }
    } else if (sharedBuffer != 0) {
        // FIXME: Ensure client side memory buffers need
        // not have additional alignment beyond sample
        // (e.g. 16 bit stereo accessed as 32 bit frame).
        size_t alignment = audio_bytes_per_sample(format);
        if (alignment & 1) {
            // for AUDIO_FORMAT_PCM_24_BIT_PACKED (not exposed through Java).
            alignment = 1;
        }
        uint32_t channelCount = audio_channel_count_from_out_mask(channelMask);
        size_t frameSize = channelCount * audio_bytes_per_sample(format);
        if (channelCount > 1) {
            // More than 2 channels does not require stronger alignment than stereo
            alignment <<= 1;
        }
        if (((uintptr_t)sharedBuffer->pointer() & (alignment - 1)) != 0) {
            ALOGE("Invalid buffer alignment: address %p, channel count %u",sharedBuffer->pointer(), channelCount);
            lStatus = BAD_VALUE;
            goto Exit;
        }

        // When initializing a shared buffer AudioTrack via constructors,
        // there's no frameCount parameter.
        // But when initializing a shared buffer AudioTrack via set(),
        // there _is_ a frameCount parameter.  We silently ignore it.
        frameCount = sharedBuffer->size() / frameSize;
    } else {
        size_t minFrameCount = 0;
        // For fast tracks we try to respect the application's request for notifications per buffer.
        if (*flags & AUDIO_OUTPUT_FLAG_FAST) {
            if (notificationsPerBuffer > 0) {
                // Avoid possible arithmetic overflow during multiplication.
                if (notificationsPerBuffer > SIZE_MAX / mFrameCount) {
                    ALOGE("Requested notificationPerBuffer=%u ignored for HAL frameCount=%zu", notificationsPerBuffer, mFrameCount);
                } else {
                    minFrameCount = mFrameCount * notificationsPerBuffer;
                }
            }
        } else {
            // For normal PCM streaming tracks, update minimum frame count.
            // Buffer depth is forced to be at least 2 x the normal mixer frame count and
            // cover audio hardware latency.
            // This is probably too conservative, but legacy application code may depend on it.
            // If you change this calculation, also review the start threshold which is related.
            uint32_t latencyMs = latency_l();
            if (latencyMs == 0) {
                ALOGE("Error when retrieving output stream latency");
                lStatus = UNKNOWN_ERROR;
                goto Exit;
            }

            minFrameCount = AudioSystem::calculateMinFrameCount(latencyMs, mNormalFrameCount,mSampleRate, sampleRate, speed /*, 0 mNotificationsPerBufferReq*/);

        }
        if (frameCount < minFrameCount) {
            frameCount = minFrameCount;
        }
    }

    // Make sure that application is notified with sufficient margin before underrun.
    // The client can divide the AudioTrack buffer into sub-buffers,
    // and expresses its desire to server as the notification frame count.
    if (sharedBuffer == 0 && audio_is_linear_pcm(format)) {
        size_t maxNotificationFrames;
        if (*flags & AUDIO_OUTPUT_FLAG_FAST) {
            // notify every HAL buffer, regardless of the size of the track buffer
            maxNotificationFrames = mFrameCount;
        } else {
            // For normal tracks, use at least double-buffering if no sample rate conversion,
            // or at least triple-buffering if there is sample rate conversion
            const int nBuffering = sampleRate == mSampleRate ? 2 : 3;
            maxNotificationFrames = frameCount / nBuffering;
            // If client requested a fast track but this was denied, then use the smaller maximum.
            if (requestedFlags & AUDIO_OUTPUT_FLAG_FAST) {
                size_t maxNotificationFramesFastDenied = FMS_20 * sampleRate / 1000;
                if (maxNotificationFrames > maxNotificationFramesFastDenied) {
                    maxNotificationFrames = maxNotificationFramesFastDenied;
                }
            }
        }
        if (notificationFrameCount == 0 || notificationFrameCount > maxNotificationFrames) {
            if (notificationFrameCount == 0) {
                ALOGD("Client defaulted notificationFrames to %zu for frameCount %zu",maxNotificationFrames, frameCount);
            } else {
                ALOGW("Client adjusted notificationFrames from %zu to %zu for frameCount %zu",notificationFrameCount, maxNotificationFrames, frameCount);
            }
            notificationFrameCount = maxNotificationFrames;
        }
    }

    *pFrameCount = frameCount;
    *pNotificationFrameCount = notificationFrameCount;


    /***
     * mType 为线程创建的类型，这里 由构造函数 可知 mType = MIXER
     * */
    switch (mType) {
    case DIRECT:
        if (audio_is_linear_pcm(format)) { // TODO maybe use audio_has_proportional_frames()?
            if (sampleRate != mSampleRate || format != mFormat || channelMask != mChannelMask) {
                ALOGE("createTrack_l() Bad parameter: sampleRate %u format %#x, channelMask 0x%08x "  "for output %p with format %#x",sampleRate, format, channelMask, mOutput, mFormat);
                lStatus = BAD_VALUE;
                goto Exit;
            }
        }
        break;

    case OFFLOAD:
        if (sampleRate != mSampleRate || format != mFormat || channelMask != mChannelMask) {
            ALOGE("createTrack_l() Bad parameter: sampleRate %d format %#x, channelMask 0x%08x \""      "for output %p with format %#x",sampleRate, format, channelMask, mOutput, mFormat);
            lStatus = BAD_VALUE;
            goto Exit;
        }
        break;

    default:
        // 执行这里
        if (!audio_is_linear_pcm(format)) {
                ALOGE("createTrack_l() Bad parameter: format %#x \"" "for output %p with format %#x",format, mOutput, mFormat);
                lStatus = BAD_VALUE;
                goto Exit;
        }
        if (sampleRate > mSampleRate * AUDIO_RESAMPLER_DOWN_RATIO_MAX) {
            ALOGE("Sample rate out of range: %u mSampleRate %u", sampleRate, mSampleRate);
            lStatus = BAD_VALUE;
            goto Exit;
        }
        break;

    }

    lStatus = initCheck();
    if (lStatus != NO_ERROR) {
        ALOGE("createTrack_l() audio driver not initialized");
        goto Exit;
    }

    { // scope for mLock
        Mutex::Autolock _l(mLock);

        // all tracks in same audio session must share the same routing strategy otherwise
        // conflicts will happen when tracks are moved from one output to another by audio policy
        // manager
        /**
         * AudioSystem::getStrategyForStream
         * ---> AudioPolicyService::getStrategyForStream
         * ----> AudioPolicyManager::getStrategyForStream
         * -----> routing_strategy AudioPolicyManager::getStrategy(audio_stream_type_t stream)
         * ------> routing_strategy Engine::getStrategyForStream(audio_stream_type_t stream)
        */
        uint32_t strategy = AudioSystem::getStrategyForStream(streamType);
        for (size_t i = 0; i < mTracks.size(); ++i) {
            sp<Track> t = mTracks[i];
            // isExternalTrack 为 TYPE_DEFAULT
            if (t != 0 && t->isExternalTrack()) {
                uint32_t actual = AudioSystem::getStrategyForStream(t->streamType());
                if (sessionId == t->sessionId() && strategy != actual) {
                    ALOGE("createTrack_l() mismatched strategy; expected %u but found %u",strategy, actual);
                    lStatus = BAD_VALUE;
                    goto Exit;
                }
            }
        }

        /***
         * frameworks/av/services/audioflinger/PlaybackTracks.h:23:class Track : public TrackBase, public VolumeProvider {
         * frameworks/av/services/audioflinger/Tracks.cpp 
         * 
         * enum track_type {
         *     TYPE_DEFAULT,           //  Track 和 RecordTrack MmapTrack
         *     TYPE_OUTPUT,            //  OutputTrack
         *     TYPE_PATCH,     //  PatchRecord 和 PatchTrack
         * };  
         * 
         * */
        track = new Track(this, client, streamType, attr, sampleRate, format,channelMask, frameCount,nullptr /* buffer */, (size_t)0 /* bufferSize */, sharedBuffer,sessionId, uid, *flags, TrackBase::TYPE_DEFAULT, portId);

        lStatus = track != 0 ? track->initCheck() : (status_t) NO_MEMORY;
        if (lStatus != NO_ERROR) {
            ALOGE("createTrack_l() initCheck failed %d; no control block?", lStatus);
            // track must be cleared from the caller as the caller has the AF lock
            goto Exit;
        }
        mTracks.add(track);

        /**
         * 从 dumpsys获得信息 这里集合为空，不可能获得chain
        */
        sp<EffectChain> chain = getEffectChain_l(sessionId);
        if (chain != 0) {
            ALOGV("createTrack_l() setting main buffer %p", chain->inBuffer());
            track->setMainBuffer(chain->inBuffer());
            chain->setStrategy(AudioSystem::getStrategyForStream(track->streamType()));
            chain->incTrackCnt();
        }

        if ((*flags & AUDIO_OUTPUT_FLAG_FAST) && (tid != -1)) {
            pid_t callingPid = IPCThreadState::self()->getCallingPid();
            // we don't have CAP_SYS_NICE, nor do we want to have it as it's too powerful,
            // so ask activity manager to do this on our behalf
            sendPrioConfigEvent_l(callingPid, tid, kPriorityAudioApp, true /*forApp*/);
        }
    }

    lStatus = NO_ERROR;

Exit:
    *status = lStatus;
    return track;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
 * 
*/
// prepareTracks_l() must be called with ThreadBase::mLock held
AudioFlinger::PlaybackThread::mixer_state AudioFlinger::MixerThread::prepareTracks_l( Vector< sp<Track> > *tracksToRemove)
{
    /**
     * 将 mTracks成员mDeletedTrackNames 中的成员从mAudioMixer中销毁
    */
    // clean up deleted track names in AudioMixer before allocating new tracks
    (void)mTracks.processDeletedTrackNames([this](int name) {
        // for each name, destroy it in the AudioMixer
        if (mAudioMixer->exists(name)) {
            mAudioMixer->destroy(name);
        }
    });
    /**
     * 清空 mTracks的mDeletedTrackNames集合
    */
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


    /**
     * 由于集合为空  getEffectChain_l 获取不到成员chain
    */
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

    if (mFastMixer != 0) {// mFastMixer为空
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

        /**
         * @system/media/audio/include/system/audio-base.h:360:    AUDIO_OUTPUT_FLAG_FAST             = 0x4,
         *   isFastTrack 返回值 (mFlags & AUDIO_OUTPUT_FLAG_FAST) != 0; 
        */
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
            uint32_t recentFull = (underruns.mBitFields.mFull - track->mObservedUnderruns.mBitFields.mFull) & UNDERRUN_MASK;
            uint32_t recentPartial = (underruns.mBitFields.mPartial -  track->mObservedUnderruns.mBitFields.mPartial) & UNDERRUN_MASK;
            uint32_t recentEmpty = (underruns.mBitFields.mEmpty -  track->mObservedUnderruns.mBitFields.mEmpty) & UNDERRUN_MASK;
            uint32_t recentUnderruns = recentPartial + recentEmpty;
            track->mObservedUnderruns = underruns;
            // don't count underruns that occur while stopping or pausing
            // or stopped which can occur when flush() is called while active
            if (!(track->isStopping() || track->isPausing() || track->isStopped()) &&recentUnderruns > 0) {
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
                    ALOGE_IF(result != OK, "Error when retrieving output stream latency: %d", result);
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
                const float vh = track->getVolumeHandler()->getVolume( proxy->framesReleased()).first;
                float volume = masterVolume * mStreamTypes[track->streamType()].volume  * vh;
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
                            j, track->mState, state->mTrackMask, recentUnderruns,track->sharedBuffer() != 0);
                    // Since the FastMixer state already has the track inactive, do nothing here.
                }
                tracksToRemove->add(track);
                // Avoids a misleading display in dumpsys
                track->mObservedUnderruns.mBitFields.mMostRecent = UNDERRUN_FULL;
            }
            continue;
        }// track->isFastTrack()

        {   // local variable scope to avoid goto warning

        audio_track_cblk_t* cblk = track->cblk();

        // The first time a track is added we wait for all its buffers to be filled before processing it
        int name = track->name();

        // if an active track doesn't exist in the AudioMixer, create it.
        if (!mAudioMixer->exists(name)) {
            status_t status = mAudioMixer->create(name, track->mChannelMask,track->mFormat, track->mSessionId);
            if (status != OK) {
                ALOGW("%s: cannot create track name" " %d, mask %#x, format %#x, sessionId %d in AudioMixer",__func__, name, track->mChannelMask, track->mFormat, track->mSessionId);
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

        desiredFrames = sourceFramesNeededWithTimestretch(sampleRate, mNormalFrameCount, mSampleRate, playbackRate.mSpeed);
        // TODO: ONLY USED FOR LEGACY RESAMPLERS, remove when they are removed.
        // add frames already consumed but not yet released by the resampler
        // because mAudioTrackServerProxy->framesReady() will include these frames
        desiredFrames += mAudioMixer->getUnreleasedFrames(track->name());

        uint32_t minFrames = 1;
        if ((track->sharedBuffer() == 0) && !track->isStopped() && !track->isPausing() && (mMixerStatusIgnoringFastTracks == MIXER_TRACKS_READY)) {
            minFrames = desiredFrames;
        }

        size_t framesReady = track->framesReady();
        if (ATRACE_ENABLED()) {
            // I wish we had formatted trace names
            std::string traceName("nRdy");
            traceName += std::to_string(track->name());
            ATRACE_INT(traceName.c_str(), framesReady);
        }

        if (    (framesReady >= minFrames)      && track->isReady()     &&      !track->isPaused() && !track->isTerminated()    )
        {
            ALOGVV("track %d s=%08x [OK] on thread %p", name, cblk->mServer, this);

            mixedTracks++;

            // track->mainBuffer() != mSinkBuffer or mMixerBuffer means
            // there is an effect chain connected to the track
            chain.clear();
            /**
             * track->mainBuffer  函数在  effect_buffer_t *mainBuffer() const { return mMainBuffer; }     //      @ frameworks/av/services/audioflinger/PlaybackTracks.h
             * 而 track 中的 mMainBuffer 来源 thread->sinkBuffer() 获取
             * PlaybackThread::sinkBuffer() 得到 mSinkBuffer
             * 
             * 当然 track可以通过setMainBuffer更改mMainBuffer 
            */
            if (track->mainBuffer() != mSinkBuffer && track->mainBuffer() != mMixerBuffer) {
                if (mEffectBufferEnabled) {
                    mEffectBufferValid = true; // Later can set directly.
                }
                chain = getEffectChain_l(track->sessionId());
                // Delegate volume control to effect in track effect chain if needed
                if (chain != 0) {
                    tracksWithEffect++;
                } else {
                    ALOGW("prepareTracks_l(): track %d attached to effect but no chain found on "  "session %d",  name, track->sessionId());
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
                
                 // 正常播放执行这里   
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
                const float vh = track->getVolumeHandler()->getVolume( track->mAudioTrackServerProxy->framesReleased()).first;
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

            // For dedicated VoIP outputs, let the HAL apply the stream volume. Track volume is still applied by the mixer.
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
            /**
             * 这里非常重要 Track 继承 TrackBase（继承 AudioBufferProvider）
            */
            // XXX: these things DON'T need to be done each time
            mAudioMixer->setBufferProvider(name, track);
            mAudioMixer->enable(name);

            mAudioMixer->setParameter(name, param, AudioMixer::VOLUME0, &vlf);
            mAudioMixer->setParameter(name, param, AudioMixer::VOLUME1, &vrf);
            mAudioMixer->setParameter(name, param, AudioMixer::AUXLEVEL, &vaf);
            mAudioMixer->setParameter(name,AudioMixer::TRACK, AudioMixer::FORMAT, (void *)track->format());
            mAudioMixer->setParameter(name,AudioMixer::TRACK,AudioMixer::CHANNEL_MASK, (void *)(uintptr_t)track->channelMask());
            mAudioMixer->setParameter(name,AudioMixer::TRACK,AudioMixer::MIXER_CHANNEL_MASK, (void *)(uintptr_t)mChannelMask);
           
           /**
            * 这里统计采样率是否和目标采样率一样
           */
            // limit track sample rate to 2 x output sample rate, which changes at re-configuration
            /**
             * frameworks/av/media/libaudioprocessing/include/media/AudioResamplerPublic.h:32:#define AUDIO_RESAMPLER_DOWN_RATIO_MAX 256
             * 这里 mSampleRate 为 44100 HZ
            */
            uint32_t maxSampleRate = mSampleRate * AUDIO_RESAMPLER_DOWN_RATIO_MAX;
            uint32_t reqSampleRate = track->mAudioTrackServerProxy->getSampleRate();// 41800 HZ
            if (reqSampleRate == 0) {
                reqSampleRate = mSampleRate;            
            } else if (reqSampleRate > maxSampleRate) {
                reqSampleRate = maxSampleRate;
            }
            /**
             * 设置采样率
            */
            mAudioMixer->setParameter(name,AudioMixer::RESAMPLE,AudioMixer::SAMPLE_RATE,(void *)(uintptr_t)reqSampleRate);

            AudioPlaybackRate playbackRate = track->mAudioTrackServerProxy->getPlaybackRate();
            mAudioMixer->setParameter(name,AudioMixer::TIMESTRETCH,AudioMixer::PLAYBACK_RATE, &playbackRate);

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
             * 
             * 
             *@ frameworks/av/services/audioflinger/AudioFlinger.h:386:    static const bool kEnableExtendedPrecision = true;
             * mMixerBufferEnabled = kEnableExtendedPrecision
             * 
             *  
             * track->mainBuffer  函数在  effect_buffer_t *mainBuffer() const { return mMainBuffer; }     //      @ frameworks/av/services/audioflinger/PlaybackTracks.h
             * 而 track 中的 mMainBuffer 来源 thread->sinkBuffer() 获取
             * PlaybackThread::sinkBuffer() 得到 mSinkBuffer
             * 
             * 当然 track可以通过setMainBuffer更改mMainBuffer 
             *
             * 这里 track->mainBuffer() == mSinkBuffer  满足
             */
            if (mMixerBufferEnabled  && (track->mainBuffer() == mSinkBuffer || track->mainBuffer() == mMixerBuffer)) {
                mAudioMixer->setParameter(name, AudioMixer::TRACK,AudioMixer::MIXER_FORMAT, (void *)mMixerBufferFormat);

                /**
                 * 
                */
                mAudioMixer->setParameter(name,AudioMixer::TRACK,AudioMixer::MAIN_BUFFER, (void *)mMixerBuffer);

                // TODO: override track->mainBuffer()?
                mMixerBufferValid = true;
            } else {
                mAudioMixer->setParameter(name,AudioMixer::TRACK,AudioMixer::MIXER_FORMAT, (void *)EFFECT_BUFFER_FORMAT);
                mAudioMixer->setParameter(name,AudioMixer::TRACK,AudioMixer::MAIN_BUFFER, (void *)track->mainBuffer());
            }
            mAudioMixer->setParameter(name,AudioMixer::TRACK,AudioMixer::AUX_BUFFER, (void *)track->auxBuffer());

           /**
            * @ frameworks/av/services/audioflinger/Threads.cpp:111:static const int8_t kMaxTrackRetries = 50;
           */
            // reset retry count
            track->mRetryCount = kMaxTrackRetries;

            // If one track is ready, set the mixer ready if:
            //  - the mixer was not ready during previous round OR
            //  - no other track is not ready
            if (mMixerStatusIgnoringFastTracks != MIXER_TRACKS_READY || mixerStatus != MIXER_TRACKS_ENABLED) {
                mixerStatus = MIXER_TRACKS_READY;
            }

        } else {
            if (framesReady < desiredFrames && !track->isStopped() && !track->isPaused()) {
                ALOGV("track(%p) underrun,  framesReady(%zu) < framesDesired(%zd)",  track, framesReady, desiredFrames);
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
            if ((track->sharedBuffer() != 0) || track->isTerminated() || track->isStopped() || track->isPaused()) {
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
                } else if (mMixerStatusIgnoringFastTracks == MIXER_TRACKS_READY || mixerStatus != MIXER_TRACKS_READY) {
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
        if (kUseFastMixer == FastMixer_Dynamic && state->mCommand == FastMixerState::MIX_WRITE && state->mTrackMask <= 1) {
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
    if ((mBytesRemaining == 0) && ((mixedTracks != 0 && mixedTracks == tracksWithEffect) ||  (mixedTracks == 0 && fastTracks > 0))) {
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AudioFlinger::MixerThread::threadLoop_mix()
{
    // mix buffers...
    mAudioMixer->process();
    mCurrentWriteLength = mSinkBufferSize;
    // increase sleep time progressively when application underrun condition clears.
    // Only increase sleep time if the mixer is ready for two consecutive times to avoid
    // that a steady state of alternating ready/not ready conditions keeps the sleep time
    // such that we would underrun the audio HAL.
    if ((mSleepTimeUs == 0) && (sleepTimeShift > 0)) {
        sleepTimeShift--;
    }
    mSleepTimeUs = 0;
    mStandbyTimeNs = systemTime() + mStandbyDelayNs;
    //TODO: delay standby when effects have a tail

}
