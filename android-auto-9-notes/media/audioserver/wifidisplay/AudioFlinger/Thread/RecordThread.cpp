class ThreadBase : public Thread {

}



//  @ frameworks/av/services/audioflinger/Threads.h
class RecordThread : public ThreadBase{

}



//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audioflinger/Threads.cpp
/***
 * 
 * AudioFlinger::openInput ---> AudioFlinger::openInput_l ---> DeviceHalInterface::openInputStream
 * 
*/
AudioFlinger::RecordThread::RecordThread(const sp<AudioFlinger>& audioFlinger,
                                         AudioStreamIn *input,
                                         audio_io_handle_t id,
                                         audio_devices_t outDevice,   // primaryOutputDevice_l()
                                         audio_devices_t inDevice,
                                         bool systemReady
#ifdef TEE_SINK
                                         , const sp<NBAIO_Sink>& teeSink
#endif
                                         ) :
    ThreadBase(audioFlinger, id, outDevice, inDevice, RECORD, systemReady),
    mInput(input),
    mActiveTracks(&this->mLocalLog),
    mRsmpInBuffer(NULL),
    // mRsmpInFrames, mRsmpInFramesP2, and mRsmpInFramesOA are set by readInputParameters_l()
    mRsmpInRear(0)
#ifdef TEE_SINK
    , mTeeSink(teeSink)
#endif
    , mReadOnlyHeap(new MemoryDealer(kRecordThreadReadOnlyHeapSize,"RecordThreadRO", MemoryHeapBase::READ_ONLY))
    // mFastCapture below
    , mFastCaptureFutex(0)
    // mInputSource
    // mPipeSink
    // mPipeSource
    , mPipeFramesP2(0)
    // mPipeMemory
    // mFastCaptureNBLogWriter
    , mFastTrackAvail(false)
    , mBtNrecSuspended(false)
{
    snprintf(mThreadName, kThreadNameLength, "AudioIn_%X", id);
    mNBLogWriter = audioFlinger->newWriter_l(kLogSize, mThreadName);

    readInputParameters_l();

    // create an NBAIO source for the HAL input stream, and negotiate
    mInputSource = new AudioStreamInSource(input->stream);
    size_t numCounterOffers = 0;
    const NBAIO_Format offers[1] = {Format_from_SR_C(mSampleRate, mChannelCount, mFormat)};
#if !LOG_NDEBUG
    ssize_t index =
#else
    (void)
#endif
            mInputSource->negotiate(offers, 1, NULL, numCounterOffers);
    ALOG_ASSERT(index == 0);

    // initialize fast capture depending on configuration
    bool initFastCapture;
    switch (kUseFastCapture) { // FastCapture_Static
    case FastCapture_Never:
        initFastCapture = false;
        ALOGV("%p kUseFastCapture = Never, initFastCapture = false", this);
        break;
    case FastCapture_Always:
        initFastCapture = true;
        ALOGV("%p kUseFastCapture = Always, initFastCapture = true", this);
        break;
    case FastCapture_Static: // FastCapture_Static
        //  frameworks/av/services/audioflinger/Threads.cpp:141:static const uint32_t kMinNormalCaptureBufferSizeMs = 12;
        initFastCapture = (mFrameCount * 1000) / mSampleRate < kMinNormalCaptureBufferSizeMs;
        //  10-10 12:38:11.057  1771  1801 V AudioFlinger: 0xeed83200 kUseFastCapture = Static, (1024 * 1000) / 48000 vs 12, initFastCapture = 0
        ALOGV("%p kUseFastCapture = Static, (%lld * 1000) / %u vs %u, initFastCapture = %d",
                this, (long long)mFrameCount, mSampleRate, kMinNormalCaptureBufferSizeMs,
                initFastCapture);
        break;
    // case FastCapture_Dynamic:
    }

    if (initFastCapture) {
        // create a Pipe for FastCapture to write to, and for us and fast tracks to read from
        NBAIO_Format format = mInputSource->format();
        // quadruple-buffering of 20 ms each; this ensures we can sleep for 20ms in RecordThread
        size_t pipeFramesP2 = roundup(4 * FMS_20 * mSampleRate / 1000);
        size_t pipeSize = pipeFramesP2 * Format_frameSize(format);
        void *pipeBuffer = nullptr;
        const sp<MemoryDealer> roHeap(readOnlyHeap());
        sp<IMemory> pipeMemory;
        if ((roHeap == 0) ||
                (pipeMemory = roHeap->allocate(pipeSize)) == 0 ||
                (pipeBuffer = pipeMemory->pointer()) == nullptr) {
            ALOGE("not enough memory for pipe buffer size=%zu; "
                    "roHeap=%p, pipeMemory=%p, pipeBuffer=%p; roHeapSize: %lld",
                    pipeSize, roHeap.get(), pipeMemory.get(), pipeBuffer,
                    (long long)kRecordThreadReadOnlyHeapSize);
            goto failed;
        }
        // pipe will be shared directly with fast clients, so clear to avoid leaking old information
        memset(pipeBuffer, 0, pipeSize);
        Pipe *pipe = new Pipe(pipeFramesP2, format, pipeBuffer);
        const NBAIO_Format offers[1] = {format};
        size_t numCounterOffers = 0;
        ssize_t index = pipe->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        mPipeSink = pipe;
        PipeReader *pipeReader = new PipeReader(*pipe);
        numCounterOffers = 0;
        index = pipeReader->negotiate(offers, 1, NULL, numCounterOffers);
        ALOG_ASSERT(index == 0);
        mPipeSource = pipeReader;
        mPipeFramesP2 = pipeFramesP2;
        mPipeMemory = pipeMemory;

        // create fast capture
        mFastCapture = new FastCapture();
        FastCaptureStateQueue *sq = mFastCapture->sq();
#ifdef STATE_QUEUE_DUMP
        // FIXME
#endif
        FastCaptureState *state = sq->begin();
        state->mCblk = NULL;
        state->mInputSource = mInputSource.get();
        state->mInputSourceGen++;
        state->mPipeSink = pipe;
        state->mPipeSinkGen++;
        state->mFrameCount = mFrameCount;
        state->mCommand = FastCaptureState::COLD_IDLE;
        // already done in constructor initialization list
        //mFastCaptureFutex = 0;
        state->mColdFutexAddr = &mFastCaptureFutex;
        state->mColdGen++;
        state->mDumpState = &mFastCaptureDumpState;
#ifdef TEE_SINK
        // FIXME
#endif
        mFastCaptureNBLogWriter = audioFlinger->newWriter_l(kFastCaptureLogSize, "FastCapture");
        state->mNBLogWriter = mFastCaptureNBLogWriter.get();
        sq->end();
        sq->push(FastCaptureStateQueue::BLOCK_UNTIL_PUSHED);

        // start the fast capture
        mFastCapture->run("FastCapture", ANDROID_PRIORITY_URGENT_AUDIO);
        pid_t tid = mFastCapture->getTid();
        sendPrioConfigEvent(getpid_cached, tid, kPriorityFastCapture, false /*forApp*/);
        stream()->setHalThreadPriority(kPriorityFastCapture);
#ifdef AUDIO_WATCHDOG
        // FIXME
#endif

        mFastTrackAvail = true;
    }
failed: ;

    // FIXME mNormalSource
}

void AudioFlinger::RecordThread::readInputParameters_l()
{
    status_t result = mInput->stream->getAudioProperties(&mSampleRate, &mChannelMask, &mHALFormat);
    LOG_ALWAYS_FATAL_IF(result != OK, "Error retrieving audio properties from HAL: %d", result);
    mChannelCount = audio_channel_count_from_in_mask(mChannelMask);
    LOG_ALWAYS_FATAL_IF(mChannelCount > FCC_8, "HAL channel count %d > %d", mChannelCount, FCC_8);
    mFormat = mHALFormat;
    LOG_ALWAYS_FATAL_IF(!audio_is_linear_pcm(mFormat), "HAL format %#x is not linear pcm", mFormat);
    result = mInput->stream->getFrameSize(&mFrameSize);
    LOG_ALWAYS_FATAL_IF(result != OK, "Error retrieving frame size from HAL: %d", result);
    result = mInput->stream->getBufferSize(&mBufferSize);
    LOG_ALWAYS_FATAL_IF(result != OK, "Error retrieving buffer size from HAL: %d", result);
    mFrameCount = mBufferSize / mFrameSize;
    ALOGV("%p RecordThread params: mChannelCount=%u, mFormat=%#x, mFrameSize=%lld, "
            "mBufferSize=%lld, mFrameCount=%lld",
            this, mChannelCount, mFormat, (long long)mFrameSize, (long long)mBufferSize,
            (long long)mFrameCount);
    // This is the formula for calculating the temporary buffer size.
    // With 7 HAL buffers, we can guarantee ability to down-sample the input by ratio of 6:1 to
    // 1 full output buffer, regardless of the alignment of the available input.
    // The value is somewhat arbitrary, and could probably be even larger.
    // A larger value should allow more old data to be read after a track calls start(),
    // without increasing latency.
    //
    // Note this is independent of the maximum downsampling ratio permitted for capture.
    mRsmpInFrames = mFrameCount * 7;
    mRsmpInFramesP2 = roundup(mRsmpInFrames);
    free(mRsmpInBuffer);
    mRsmpInBuffer = NULL;

    // TODO optimize audio capture buffer sizes ...
    // Here we calculate the size of the sliding buffer used as a source
    // for resampling.  mRsmpInFramesP2 is currently roundup(mFrameCount * 7).
    // For current HAL frame counts, this is usually 2048 = 40 ms.  It would
    // be better to have it derived from the pipe depth in the long term.
    // The current value is higher than necessary.  However it should not add to latency.

    // Over-allocate beyond mRsmpInFramesP2 to permit a HAL read past end of buffer
    mRsmpInFramesOA = mRsmpInFramesP2 + mFrameCount - 1;
    (void)posix_memalign(&mRsmpInBuffer, 32, mRsmpInFramesOA * mFrameSize);
    // if posix_memalign fails, will segv here.
    memset(mRsmpInBuffer, 0, mRsmpInFramesOA * mFrameSize);

    // AudioRecord mSampleRate and mChannelCount are constant due to AudioRecord API constraints.
    // But if thread's mSampleRate or mChannelCount changes, how will that affect active tracks?
}


/**
 * 启动线程
*/
void AudioFlinger::RecordThread::onFirstRef()
{
    run(mThreadName, PRIORITY_URGENT_AUDIO);
}



/**
 * AudioRecord::set
 * -> AudioRecord::createRecord_l
 * --> AudioFlinger::createRecord
 * ---> 
 * 
*/
// RecordThread::createRecordTrack_l() must be called with AudioFlinger::mLock held
sp<AudioFlinger::RecordThread::RecordTrack> AudioFlinger::RecordThread::createRecordTrack_l(
        const sp<AudioFlinger::Client>& client,
        const audio_attributes_t& attr,
        uint32_t *pSampleRate,
        audio_format_t format,
        audio_channel_mask_t channelMask,
        size_t *pFrameCount,
        audio_session_t sessionId,  //  
        size_t *pNotificationFrameCount,
        uid_t uid,
        audio_input_flags_t *flags,
        pid_t tid,
        status_t *status,
        audio_port_handle_t portId)
{
    size_t frameCount = *pFrameCount;
    size_t notificationFrameCount = *pNotificationFrameCount;
    sp<RecordTrack> track;
    status_t lStatus;
    audio_input_flags_t inputFlags = mInput->flags;
    audio_input_flags_t requestedFlags = *flags;
    uint32_t sampleRate;

    lStatus = initCheck();
    if (lStatus != NO_ERROR) {
        ALOGE("createRecordTrack_l() audio driver not initialized");
        goto Exit;
    }

    if (*pSampleRate == 0) {
        *pSampleRate = mSampleRate;
    }
    sampleRate = *pSampleRate;

    // special case for FAST flag considered OK if fast capture is present
    if (hasFastCapture()) {
        inputFlags = (audio_input_flags_t)(inputFlags | AUDIO_INPUT_FLAG_FAST);
    }

    // Check if requested flags are compatible with input stream flags
    if ((*flags & inputFlags) != *flags) {
        ALOGW("createRecordTrack_l(): mismatch between requested flags (%08x) and"  " input flags (%08x)", *flags, inputFlags);
        *flags = (audio_input_flags_t)(*flags & inputFlags);
    }

    // client expresses a preference for FAST, but we get the final say
    if (*flags & AUDIO_INPUT_FLAG_FAST) {
      if (
            // we formerly checked for a callback handler (non-0 tid),
            // but that is no longer required for TRANSFER_OBTAIN mode
            //
            // frame count is not specified, or is exactly the pipe depth
            ((frameCount == 0) || (frameCount == mPipeFramesP2)) &&
            // PCM data
            audio_is_linear_pcm(format) &&
            // hardware format
            (format == mFormat) &&
            // hardware channel mask
            (channelMask == mChannelMask) &&
            // hardware sample rate
            (sampleRate == mSampleRate) &&
            // record thread has an associated fast capture
            hasFastCapture() &&
            // there are sufficient fast track slots available
            mFastTrackAvail
        ) {
          // check compatibility with audio effects.
          Mutex::Autolock _l(mLock);
          // Do not accept FAST flag if the session has software effects
          sp<EffectChain> chain = getEffectChain_l(sessionId);
          if (chain != 0) {
              audio_input_flags_t old = *flags;
              chain->checkInputFlagCompatibility(flags);
              if (old != *flags) {
                  ALOGV("%p AUDIO_INPUT_FLAGS denied by effect old=%#x new=%#x",
                          this, (int)old, (int)*flags);
              }
          }
          ALOGV_IF((*flags & AUDIO_INPUT_FLAG_FAST) != 0,
                   "%p AUDIO_INPUT_FLAG_FAST accepted: frameCount=%zu mFrameCount=%zu",
                   this, frameCount, mFrameCount);
      } else {
        ALOGV("%p AUDIO_INPUT_FLAG_FAST denied: frameCount=%zu mFrameCount=%zu mPipeFramesP2=%zu "
                "format=%#x isLinear=%d mFormat=%#x channelMask=%#x sampleRate=%u mSampleRate=%u "
                "hasFastCapture=%d tid=%d mFastTrackAvail=%d",
                this, frameCount, mFrameCount, mPipeFramesP2,
                format, audio_is_linear_pcm(format), mFormat, channelMask, sampleRate, mSampleRate,
                hasFastCapture(), tid, mFastTrackAvail);
        *flags = (audio_input_flags_t)(*flags & ~AUDIO_INPUT_FLAG_FAST);
      }
    }

    // If FAST or RAW flags were corrected, ask caller to request new input from audio policy
    if ((*flags & AUDIO_INPUT_FLAG_FAST) !=
            (requestedFlags & AUDIO_INPUT_FLAG_FAST)) {
        *flags = (audio_input_flags_t) (*flags & ~(AUDIO_INPUT_FLAG_FAST | AUDIO_INPUT_FLAG_RAW));
        lStatus = BAD_TYPE;
        goto Exit;
    }

    // compute track buffer size in frames, and suggest the notification frame count
    if (*flags & AUDIO_INPUT_FLAG_FAST) {
        // fast track: frame count is exactly the pipe depth
        frameCount = mPipeFramesP2;
        // ignore requested notificationFrames, and always notify exactly once every HAL buffer
        notificationFrameCount = mFrameCount;
    } else {
        // not fast track: max notification period is resampled equivalent of one HAL buffer time
        //                 or 20 ms if there is a fast capture
        // TODO This could be a roundupRatio inline, and const
        size_t maxNotificationFrames = ((int64_t) (hasFastCapture() ? mSampleRate/50 : mFrameCount) * sampleRate + mSampleRate - 1) / mSampleRate;
        // minimum number of notification periods is at least kMinNotifications,
        // and at least kMinMs rounded up to a whole notification period (minNotificationsByMs)
        static const size_t kMinNotifications = 3;
        static const uint32_t kMinMs = 30;
        // TODO This could be a roundupRatio inline
        const size_t minFramesByMs = (sampleRate * kMinMs + 1000 - 1) / 1000;
        // TODO This could be a roundupRatio inline
        const size_t minNotificationsByMs = (minFramesByMs + maxNotificationFrames - 1) / maxNotificationFrames;
        const size_t minFrameCount = maxNotificationFrames * max(kMinNotifications, minNotificationsByMs);
        frameCount = max(frameCount, minFrameCount);
        if (notificationFrameCount == 0 || notificationFrameCount > maxNotificationFrames) {
            notificationFrameCount = maxNotificationFrames;
        }
    }
    *pFrameCount = frameCount;
    *pNotificationFrameCount = notificationFrameCount;

    { // scope for mLock
        Mutex::Autolock _l(mLock);
        /**
         * @    frameworks/av/services/audioflinger/Tracks.cpp
        */
        track = new RecordTrack(this, client, attr, sampleRate,
                      format, channelMask, frameCount,
                      nullptr /* buffer */, (size_t)0 /* bufferSize */, sessionId, uid,*flags, TrackBase::TYPE_DEFAULT, portId);

        lStatus = track->initCheck();
        if (lStatus != NO_ERROR) {
            ALOGE("createRecordTrack_l() initCheck failed %d; no control block?", lStatus);
            // track must be cleared from the caller as the caller has the AF lock
            goto Exit;
        }
        mTracks.add(track);

        if ((*flags & AUDIO_INPUT_FLAG_FAST) && (tid != -1)) {
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



/**
 * IAudioRecord::start 
 * ---> RecordHandle::start 
 * ----> RecordTrack::start 
 * -----> RecordThread::start
*/
status_t AudioFlinger::RecordThread::start(RecordThread::RecordTrack* recordTrack,
                                           AudioSystem::sync_event_t event /*= AudioSystem::SYNC_EVENT_NONE*/,
                                           audio_session_t triggerSession /*= AUDIO_SESSION_NONE*/)
{
    // 10-11 17:03:20.467  1769  1801 V AudioFlinger_Thread: RecordThread::start event 0, triggerSession 0
    ALOGV("RecordThread::start event %d, triggerSession %d", event, triggerSession);
    sp<ThreadBase> strongMe = this;
    status_t status = NO_ERROR;

    if (event == AudioSystem::SYNC_EVENT_NONE) {
        /**
         * 执行这里
        */
        recordTrack->clearSyncStartEvent(); 

    } else if (event != AudioSystem::SYNC_EVENT_SAME) {
        recordTrack->mSyncStartEvent = mAudioFlinger->createSyncEvent(event,
                                       triggerSession,
                                       recordTrack->sessionId(),
                                       syncStartEventCallback,
                                       recordTrack);
        // Sync event can be cancelled by the trigger session if the track is not in a
        // compatible state in which case we start record immediately
        if (recordTrack->mSyncStartEvent->isCancelled()) {
            recordTrack->clearSyncStartEvent();
        } else {
            // do not wait for the event for more than AudioSystem::kSyncRecordStartTimeOutMs
            recordTrack->mFramesToDrop = -(ssize_t) ((AudioSystem::kSyncRecordStartTimeOutMs * recordTrack->mSampleRate) / 1000);
        }
    }

    {
        // This section is a rendezvous between binder thread executing start() and RecordThread
        AutoMutex lock(mLock);
        if (mActiveTracks.indexOf(recordTrack) >= 0) {
            if (recordTrack->mState == TrackBase::PAUSING) {
                ALOGV("active record track PAUSING -> ACTIVE");
                recordTrack->mState = TrackBase::ACTIVE;
            } else {
                ALOGV("active record track state %d", recordTrack->mState);
            }
            return status;
        }

        // TODO consider other ways of handling this, such as changing the state to :STARTING and
        //      adding the track to mActiveTracks after returning from AudioSystem::startInput(),
        //      or using a separate command thread
        recordTrack->mState = TrackBase::STARTING_1;
        /**
         * 
         * 
        */
        mActiveTracks.add(recordTrack);
        status_t status = NO_ERROR;
        /**
         * 
         * 由于 mType 值为 TrackBase::TYPE_DEFAULT，所以这里 返回 true
        */
        if (recordTrack->isExternalTrack()) {
            mLock.unlock();
            bool silenced;
            /***
             * 这里非常重要
             * 
             * AudioSystem::startInput
             * --> AudioPolicyService::startInput
             * ---> AudioPolicyManager::startInput
             * 
             * 
            */
            status = AudioSystem::startInput(recordTrack->portId(), &silenced);
            mLock.lock();
            // FIXME should verify that recordTrack is still in mActiveTracks
            if (status != NO_ERROR) {
                mActiveTracks.remove(recordTrack);
                recordTrack->clearSyncStartEvent();
                ALOGV("RecordThread::start error %d", status);
                return status;
            }
            /**
             * frameworks/av/services/audioflinger/RecordTracks.h:67:            
             * void        setSilenced(bool silenced) { if (!isPatchTrack()) mSilenced = silenced; }

            */
            recordTrack->setSilenced(silenced);
        }
        // Catch up with current buffer indices if thread is already running.
        // This is what makes a new client discard all buffered data.  If the track's mRsmpInFront
        // was initialized to some value closer to the thread's mRsmpInFront, then the track could
        // see previously buffered data before it called start(), but with greater risk of overrun.

        recordTrack->mResamplerBufferProvider->reset();
        // clear any converter state as new data will be discontinuous
        recordTrack->mRecordBufferConverter->reset();
        recordTrack->mState = TrackBase::STARTING_2;
        // signal thread to start
        mWaitWorkCV.broadcast();
        if (mActiveTracks.indexOf(recordTrack) < 0) {
            ALOGV("Record failed to start");
            status = BAD_VALUE;
            goto startError;
        }
        return status;
    }

startError:
    if (recordTrack->isExternalTrack()) {
        AudioSystem::stopInput(recordTrack->portId());
    }
    recordTrack->clearSyncStartEvent();
    // FIXME I wonder why we do not reset the state here?
    return status;
}


bool AudioFlinger::RecordThread::threadLoop()
{
    nsecs_t lastWarning = 0;

    inputStandBy();

reacquire_wakelock:
    sp<RecordTrack> activeTrack;
    {
        Mutex::Autolock _l(mLock);
        acquireWakeLock_l();
    }

    // used to request a deferred sleep, to be executed later while mutex is unlocked
    uint32_t sleepUs = 0;

    // loop while there is work to do
    for (;;) {
        Vector< sp<EffectChain> > effectChains;

        // activeTracks accumulates a copy of a subset of mActiveTracks
        Vector< sp<RecordTrack> > activeTracks;

        // reference to the (first and only) active fast track
        sp<RecordTrack> fastTrack;

        // reference to a fast track which is about to be removed
        sp<RecordTrack> fastTrackToRemove;

        { // scope for mLock
            Mutex::Autolock _l(mLock);

            processConfigEvents_l();

            // check exitPending here because checkForNewParameters_l() and
            // checkForNewParameters_l() can temporarily release mLock
            if (exitPending()) {
                break;
            }

            // sleep with mutex unlocked
            if (sleepUs > 0) {
                ATRACE_BEGIN("sleepC");
                mWaitWorkCV.waitRelative(mLock, microseconds((nsecs_t)sleepUs));
                ATRACE_END();
                sleepUs = 0;
                continue;
            }

            // if no active track(s), then standby and release wakelock
            size_t size = mActiveTracks.size();
            if (size == 0) {
                standbyIfNotAlreadyInStandby();
                // exitPending() can't become true here
                releaseWakeLock_l();
                ALOGV("RecordThread: loop stopping");
                // go to sleep
                mWaitWorkCV.wait(mLock);
                ALOGV("RecordThread: loop starting");
                goto reacquire_wakelock;
            }

            bool doBroadcast = false;
            bool allStopped = true;
            for (size_t i = 0; i < size; ) {

                activeTrack = mActiveTracks[i];
                if (activeTrack->isTerminated()) {
                    if (activeTrack->isFastTrack()) {
                        ALOG_ASSERT(fastTrackToRemove == 0);
                        fastTrackToRemove = activeTrack;
                    }
                    removeTrack_l(activeTrack);
                    mActiveTracks.remove(activeTrack);
                    size--;
                    continue;
                }

                TrackBase::track_state activeTrackState = activeTrack->mState;
                switch (activeTrackState) {

                case TrackBase::PAUSING:
                    mActiveTracks.remove(activeTrack);
                    doBroadcast = true;
                    size--;
                    continue;

                case TrackBase::STARTING_1:
                    sleepUs = 10000;
                    i++;
                    allStopped = false;
                    continue;

                case TrackBase::STARTING_2:
                    doBroadcast = true;
                    mStandby = false;
                    activeTrack->mState = TrackBase::ACTIVE;
                    allStopped = false;
                    break;

                case TrackBase::ACTIVE:
                    allStopped = false;
                    break;

                case TrackBase::IDLE:
                    i++;
                    continue;

                default:
                    LOG_ALWAYS_FATAL("Unexpected activeTrackState %d", activeTrackState);
                }

                activeTracks.add(activeTrack);
                i++;

                if (activeTrack->isFastTrack()) {
                    ALOG_ASSERT(!mFastTrackAvail);
                    ALOG_ASSERT(fastTrack == 0);
                    fastTrack = activeTrack;
                }
            }

            mActiveTracks.updatePowerState(this);

            updateMetadata_l();

            if (allStopped) {
                standbyIfNotAlreadyInStandby();
            }
            if (doBroadcast) {
                mStartStopCond.broadcast();
            }

            // sleep if there are no active tracks to process
            if (activeTracks.size() == 0) {
                if (sleepUs == 0) {
                    sleepUs = kRecordThreadSleepUs;
                }
                continue;
            }
            sleepUs = 0;

            lockEffectChains_l(effectChains);
        }

        // thread mutex is now unlocked, mActiveTracks unknown, activeTracks.size() > 0

        size_t size = effectChains.size();
        for (size_t i = 0; i < size; i++) {
            // thread mutex is not locked, but effect chain is locked
            effectChains[i]->process_l();
        }

        // Push a new fast capture state if fast capture is not already running, or cblk change
        if (mFastCapture != 0) {
            FastCaptureStateQueue *sq = mFastCapture->sq();
            FastCaptureState *state = sq->begin();
            bool didModify = false;
            FastCaptureStateQueue::block_t block = FastCaptureStateQueue::BLOCK_UNTIL_PUSHED;
            if (state->mCommand != FastCaptureState::READ_WRITE /* FIXME &&
                    (kUseFastMixer != FastMixer_Dynamic || state->mTrackMask > 1)*/) {
                if (state->mCommand == FastCaptureState::COLD_IDLE) {
                    int32_t old = android_atomic_inc(&mFastCaptureFutex);
                    if (old == -1) {
                        (void) syscall(__NR_futex, &mFastCaptureFutex, FUTEX_WAKE_PRIVATE, 1);
                    }
                }
                state->mCommand = FastCaptureState::READ_WRITE;
#if 0   // FIXME
                mFastCaptureDumpState.increaseSamplingN(mAudioFlinger->isLowRamDevice() ?
                        FastThreadDumpState::kSamplingNforLowRamDevice :
                        FastThreadDumpState::kSamplingN);
#endif
                didModify = true;
            }
            audio_track_cblk_t *cblkOld = state->mCblk;
            audio_track_cblk_t *cblkNew = fastTrack != 0 ? fastTrack->cblk() : NULL;
            if (cblkNew != cblkOld) {
                state->mCblk = cblkNew;
                // block until acked if removing a fast track
                if (cblkOld != NULL) {
                    block = FastCaptureStateQueue::BLOCK_UNTIL_ACKED;
                }
                didModify = true;
            }
            sq->end(didModify);
            if (didModify) {
                sq->push(block);
#if 0
                if (kUseFastCapture == FastCapture_Dynamic) {
                    mNormalSource = mPipeSource;
                }
#endif
            }
        }

        // now run the fast track destructor with thread mutex unlocked
        fastTrackToRemove.clear();

        // Read from HAL to keep up with fastest client if multiple active tracks, not slowest one.
        // Only the client(s) that are too slow will overrun. But if even the fastest client is too
        // slow, then this RecordThread will overrun by not calling HAL read often enough.
        // If destination is non-contiguous, first read past the nominal end of buffer, then
        // copy to the right place.  Permitted because mRsmpInBuffer was over-allocated.

        int32_t rear = mRsmpInRear & (mRsmpInFramesP2 - 1);
        ssize_t framesRead;

        // If an NBAIO source is present, use it to read the normal capture's data
        if (mPipeSource != 0) {
            size_t framesToRead = mBufferSize / mFrameSize;
            framesToRead = min(mRsmpInFramesOA - rear, mRsmpInFramesP2 / 2);

            // The audio fifo read() returns OVERRUN on overflow, and advances the read pointer
            // to the full buffer point (clearing the overflow condition).  Upon OVERRUN error,
            // we immediately retry the read() to get data and prevent another overflow.
            for (int retries = 0; retries <= 2; ++retries) {
                ALOGW_IF(retries > 0, "overrun on read from pipe, retry #%d", retries);
                framesRead = mPipeSource->read((uint8_t*)mRsmpInBuffer + rear * mFrameSize,framesToRead);
                if (framesRead != OVERRUN) break;
            }

            const ssize_t availableToRead = mPipeSource->availableToRead();
            if (availableToRead >= 0) {
                // PipeSource is the master clock.  It is up to the AudioRecord client to keep up.
                LOG_ALWAYS_FATAL_IF((size_t)availableToRead > mPipeFramesP2,
                        "more frames to read than fifo size, %zd > %zu",
                        availableToRead, mPipeFramesP2);
                const size_t pipeFramesFree = mPipeFramesP2 - availableToRead;
                const size_t sleepFrames = min(pipeFramesFree, mRsmpInFramesP2) / 2;
                ALOGVV("mPipeFramesP2:%zu mRsmpInFramesP2:%zu sleepFrames:%zu availableToRead:%zd",
                        mPipeFramesP2, mRsmpInFramesP2, sleepFrames, availableToRead);
                sleepUs = (sleepFrames * 1000000LL) / mSampleRate;
            }
            if (framesRead < 0) {
                status_t status = (status_t) framesRead;
                switch (status) {
                case OVERRUN:
                    ALOGW("overrun on read from pipe");
                    framesRead = 0;
                    break;
                case NEGOTIATE:
                    ALOGE("re-negotiation is needed");
                    framesRead = -1;  // Will cause an attempt to recover.
                    break;
                default:
                    ALOGE("unknown error %d on read from pipe", status);
                    break;
                }
            }
        // otherwise use the HAL / AudioStreamIn directly
        } else {
            ATRACE_BEGIN("read");
            size_t bytesRead;
            status_t result = mInput->stream->read(
                    (uint8_t*)mRsmpInBuffer + rear * mFrameSize, mBufferSize, &bytesRead);
            ATRACE_END();
            if (result < 0) {
                framesRead = result;
            } else {
                framesRead = bytesRead / mFrameSize;
            }
        }

        // Update server timestamp with server stats
        // systemTime() is optional if the hardware supports timestamps.
        mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER] += framesRead;
        mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_SERVER] = systemTime();

        // Update server timestamp with kernel stats
        if (mPipeSource.get() == nullptr /* don't obtain for FastCapture, could block */) {
            int64_t position, time;
            int ret = mInput->stream->getCapturePosition(&position, &time);
            if (ret == NO_ERROR) {
                mTimestamp.mPosition[ExtendedTimestamp::LOCATION_KERNEL] = position;
                mTimestamp.mTimeNs[ExtendedTimestamp::LOCATION_KERNEL] = time;
                // Note: In general record buffers should tend to be empty in
                // a properly running pipeline.
                //
                // Also, it is not advantageous to call get_presentation_position during the read
                // as the read obtains a lock, preventing the timestamp call from executing.
            }
        }
        // Use this to track timestamp information
        // ALOGD("%s", mTimestamp.toString().c_str());

        if (framesRead < 0 || (framesRead == 0 && mPipeSource == 0)) {
            ALOGE("read failed: framesRead=%zd", framesRead);
            // Force input into standby so that it tries to recover at next read attempt
            inputStandBy();
            sleepUs = kRecordThreadSleepUs;
        }
        if (framesRead <= 0) {
            goto unlock;
        }
        ALOG_ASSERT(framesRead > 0);

        if (mTeeSink != 0) {
            (void) mTeeSink->write((uint8_t*)mRsmpInBuffer + rear * mFrameSize, framesRead);
        }
        // If destination is non-contiguous, we now correct for reading past end of buffer.
        {
            size_t part1 = mRsmpInFramesP2 - rear;
            if ((size_t) framesRead > part1) {
                memcpy(mRsmpInBuffer, (uint8_t*)mRsmpInBuffer + mRsmpInFramesP2 * mFrameSize,
                        (framesRead - part1) * mFrameSize);
            }
        }
        rear = mRsmpInRear += framesRead;

        size = activeTracks.size();

        // loop over each active track
        for (size_t i = 0; i < size; i++) {
            activeTrack = activeTracks[i];

            // skip fast tracks, as those are handled directly by FastCapture
            if (activeTrack->isFastTrack()) {
                continue;
            }

            // TODO: This code probably should be moved to RecordTrack.
            // TODO: Update the activeTrack buffer converter in case of reconfigure.

            enum {
                OVERRUN_UNKNOWN,
                OVERRUN_TRUE,
                OVERRUN_FALSE
            } overrun = OVERRUN_UNKNOWN;

            // loop over getNextBuffer to handle circular sink
            for (;;) {

                activeTrack->mSink.frameCount = ~0;
                status_t status = activeTrack->getNextBuffer(&activeTrack->mSink);
                size_t framesOut = activeTrack->mSink.frameCount;
                LOG_ALWAYS_FATAL_IF((status == OK) != (framesOut > 0));

                // check available frames and handle overrun conditions
                // if the record track isn't draining fast enough.
                bool hasOverrun;
                size_t framesIn;
                activeTrack->mResamplerBufferProvider->sync(&framesIn, &hasOverrun);
                if (hasOverrun) {
                    overrun = OVERRUN_TRUE;
                }
                if (framesOut == 0 || framesIn == 0) {
                    break;
                }

                // Don't allow framesOut to be larger than what is possible with resampling
                // from framesIn.
                // This isn't strictly necessary but helps limit buffer resizing in
                // RecordBufferConverter.  TODO: remove when no longer needed.
                framesOut = min(framesOut,
                        destinationFramesPossible(
                                framesIn, mSampleRate, activeTrack->mSampleRate));
                // process frames from the RecordThread buffer provider to the RecordTrack buffer
                framesOut = activeTrack->mRecordBufferConverter->convert(
                        activeTrack->mSink.raw, activeTrack->mResamplerBufferProvider, framesOut);

                if (framesOut > 0 && (overrun == OVERRUN_UNKNOWN)) {
                    overrun = OVERRUN_FALSE;
                }

                if (activeTrack->mFramesToDrop == 0) {
                    if (framesOut > 0) {
                        activeTrack->mSink.frameCount = framesOut;
                        // Sanitize before releasing if the track has no access to the source data
                        // An idle UID receives silence from non virtual devices until active
                        if (activeTrack->isSilenced()) {
                            memset(activeTrack->mSink.raw, 0, framesOut * mFrameSize);
                        }
                        activeTrack->releaseBuffer(&activeTrack->mSink);
                    }
                } else {
                    // FIXME could do a partial drop of framesOut
                    if (activeTrack->mFramesToDrop > 0) {
                        activeTrack->mFramesToDrop -= framesOut;
                        if (activeTrack->mFramesToDrop <= 0) {
                            activeTrack->clearSyncStartEvent();
                        }
                    } else {
                        activeTrack->mFramesToDrop += framesOut;
                        if (activeTrack->mFramesToDrop >= 0 || activeTrack->mSyncStartEvent == 0 ||
                                activeTrack->mSyncStartEvent->isCancelled()) {
                            ALOGW("Synced record %s, session %d, trigger session %d",
                                  (activeTrack->mFramesToDrop >= 0) ? "timed out" : "cancelled",
                                  activeTrack->sessionId(),
                                  (activeTrack->mSyncStartEvent != 0) ?
                                          activeTrack->mSyncStartEvent->triggerSession() :
                                          AUDIO_SESSION_NONE);
                            activeTrack->clearSyncStartEvent();
                        }
                    }
                }

                if (framesOut == 0) {
                    break;
                }
            }

            switch (overrun) {
            case OVERRUN_TRUE:
                // client isn't retrieving buffers fast enough
                if (!activeTrack->setOverflow()) {
                    nsecs_t now = systemTime();
                    // FIXME should lastWarning per track?
                    if ((now - lastWarning) > kWarningThrottleNs) {
                        ALOGW("RecordThread: buffer overflow");
                        lastWarning = now;
                    }
                }
                break;
            case OVERRUN_FALSE:
                activeTrack->clearOverflow();
                break;
            case OVERRUN_UNKNOWN:
                break;
            }

            // update frame information and push timestamp out
            activeTrack->updateTrackFrameInfo(
                    activeTrack->mServerProxy->framesReleased(),
                    mTimestamp.mPosition[ExtendedTimestamp::LOCATION_SERVER],
                    mSampleRate, mTimestamp);
        }

unlock:
        // enable changes in effect chain
        unlockEffectChains(effectChains);
        // effectChains doesn't need to be cleared, since it is cleared by destructor at scope end
    }

    standbyIfNotAlreadyInStandby();

    {
        Mutex::Autolock _l(mLock);
        for (size_t i = 0; i < mTracks.size(); i++) {
            sp<RecordTrack> track = mTracks[i];
            track->invalidate();
        }
        mActiveTracks.clear();
        mStartStopCond.broadcast();
    }

    releaseWakeLock();

    ALOGV("RecordThread %p exiting", this);
    return false;
}















