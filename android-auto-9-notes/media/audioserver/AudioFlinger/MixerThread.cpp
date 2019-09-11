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
    ALOGW_IF(initFastMixer == false && mFrameCount < mNormalFrameCount,
            "FastMixer is preferred for this sink as frameCount %zu is less than threshold %zu",
            mFrameCount, mNormalFrameCount);
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
        monoPipe->setAvgFrames((mScreenState & 1) ?
                (monoPipe->maxFrames() * 7) / 8 : mNormalFrameCount * 2);
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

    }

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
        mNormalSink = initFastMixer ? mPipeSink : mOutputSink;
        break;
    }
}
