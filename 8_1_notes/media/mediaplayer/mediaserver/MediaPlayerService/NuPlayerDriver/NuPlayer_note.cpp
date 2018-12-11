//	@frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayer.cpp
struct NuPlayer : public AHandler {	}



NuPlayer::NuPlayer(pid_t pid)
    : mUIDValid(false),
      mPID(pid),
      mSourceFlags(0),
      mOffloadAudio(false),
      mAudioDecoderGeneration(0),
      mVideoDecoderGeneration(0),
      mRendererGeneration(0),
      mPreviousSeekTimeUs(0),
      mAudioEOS(false),
      mVideoEOS(false),
      mScanSourcesPending(false),
      mScanSourcesGeneration(0),
      mPollDurationGeneration(0),
      mTimedTextGeneration(0),
      mSetVideoTimeGeneration(0),
      mFlushingAudio(NONE),
      mFlushingVideo(NONE),
      mResumePending(false),
      mVideoScalingMode(NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW),
      mPlaybackSettings(AUDIO_PLAYBACK_RATE_DEFAULT),
      mVideoFpsHint(-1.f),
      mStarted(false),
      mPrepared(false),
      mResetting(false),
      mSourceStarted(false),
      mAudioDecoderError(false),
      mVideoDecoderError(false),
      mPaused(false),
      mPausedByClient(true),
      mPausedForBuffering(false),
      mStreaming(false),
      mIsDrmProtected(false),
      mDataSourceType(DATA_SOURCE_TYPE_NONE) {
    clearFlushComplete();
    mRendering = false;
    bNuPlayerStreamingSource = false;
    bEnablePassThrough = false;
}


//setDataSource
void NuPlayer::setDataSourceAsync(int fd, int64_t offset, int64_t length) {
    sp<AMessage> msg = new AMessage(kWhatSetDataSource, this);

    sp<AMessage> notify = new AMessage(kWhatSourceNotify, this);

    sp<GenericSource> source = new GenericSource(notify, mUIDValid, mUID);

    ALOGV("setDataSourceAsync fd %d/%lld/%lld source: %p",fd, (long long)offset, (long long)length, source.get());

    status_t err = source->setDataSource(fd, offset, length);

    if (err != OK) {
        ALOGE("Failed to set data source!");
        source = NULL;
    }

    msg->setObject("source", source);
    msg->post();
    mDataSourceType = DATA_SOURCE_TYPE_GENERIC_FD;
}


void NuPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatSetDataSource:
        {
            ALOGV("kWhatSetDataSource");

            CHECK(mSource == NULL);

            status_t err = OK;
            sp<RefBase> obj;
            CHECK(msg->findObject("source", &obj));
            if (obj != NULL) {
                Mutex::Autolock autoLock(mSourceLock);
                mSource = static_cast<Source *>(obj.get());//这里只是简单的复制操作
            } else {
                err = UNKNOWN_ERROR;
            }

            CHECK(mDriver != NULL);
            sp<NuPlayerDriver> driver = mDriver.promote();
            if (driver != NULL) {
                driver->notifySetDataSourceCompleted(err);//这里回到给
            }
            break;
        }
        。。。。。。
    }
}





//	prepare
void NuPlayer::prepareAsync() {
    ALOGV("prepareAsync");

    (new AMessage(kWhatPrepare, this))->post();
}

void NuPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
    	。。。。。。
        case kWhatPrepare:
        {
            ALOGV("onMessageReceived kWhatPrepare");
            mSource->prepareAsync();
            break;
        }
        。。。。。。
    }
}






void NuPlayer::start() {
    (new AMessage(kWhatStart, this))->post();
}


void NuPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
    	。。。。。。
        case kWhatStart:
        {
            ALOGV("kWhatStart");
            if (mStarted) {
                // do not resume yet if the source is still buffering
                if (!mPausedForBuffering) {
                    onResume();
                }
            } else {
                onStart();
            }
            mPausedByClient = false;
            break;
        }
        。。。。。。
    }
}


void NuPlayer::onStart(int64_t startPositionUs, MediaPlayerSeekMode mode) {
    ALOGV("onStart: mCrypto: %p (%d)", mCrypto.get(),(mCrypto != NULL ? mCrypto->getStrongCount() : 0));

    if (!mSourceStarted) {
        mSourceStarted = true;
        mSource->start();
    }
    if (startPositionUs > 0) {
        performSeek(startPositionUs, mode);
        if (mSource->getFormat(false /* audio */) == NULL) {
            return;
        }
    }

    mOffloadAudio = false;
    mAudioEOS = false;
    mVideoEOS = false;
    mStarted = true;
    mPaused = false;
    bEnablePassThrough = false;
    uint32_t flags = 0;

    if (mSource->isRealTime()) {
        flags |= Renderer::FLAG_REAL_TIME;
    }

    bool hasAudio = (mSource->getFormat(true /* audio */) != NULL);
    bool hasVideo = (mSource->getFormat(false /* audio */) != NULL);
    //do not know if there is cts to test this condition
    //add an variable so that calling start() will not return error when using StreamingSource
    if (!hasAudio && !hasVideo && !bNuPlayerStreamingSource) {
        ALOGE("no metadata for either audio or video source");
        mSource->stop();
        mSourceStarted = false;
        notifyListener(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, ERROR_MALFORMED);
        return;
    }
    ALOGV_IF(!hasAudio, "no metadata for audio source");  // video only stream

    sp<MetaData> audioMeta = mSource->getFormatMeta(true /* audio */);

    audio_stream_type_t streamType = AUDIO_STREAM_MUSIC;
    if (mAudioSink != NULL) {
        streamType = mAudioSink->getAudioStreamType();
    }

    mOffloadAudio = canOffloadStream(audioMeta, hasVideo, mSource->isStreaming(), streamType) && (mPlaybackSettings.mSpeed == 1.f && mPlaybackSettings.mPitch == 1.f);

    // Modular DRM: Disabling audio offload if the source is protected
    if (mOffloadAudio && mIsDrmProtected) {
        mOffloadAudio = false;
        ALOGV("onStart: Disabling mOffloadAudio now that the source is protected.");
    }

    if (mOffloadAudio) {
        flags |= Renderer::FLAG_OFFLOAD_AUDIO;
    }

    bEnablePassThrough = canPassThrough(audioMeta)  && (mPlaybackSettings.mSpeed == 1.f && mPlaybackSettings.mPitch == 1.f);

    sp<AMessage> notify = new AMessage(kWhatRendererNotify, this);
    ++mRendererGeneration;
    notify->setInt32("generation", mRendererGeneration);
    mRenderer = new Renderer(mAudioSink, notify, flags);	//	@frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerRenderer.h:34
    mRendererLooper = new ALooper;
    mRendererLooper->setName("NuPlayerRenderer");
    mRendererLooper->start(false, false, ANDROID_PRIORITY_AUDIO);
    mRendererLooper->registerHandler(mRenderer);

    status_t err = mRenderer->setPlaybackSettings(mPlaybackSettings);
    if (err != OK) {
        mSource->stop();
        mSourceStarted = false;
        notifyListener(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);
        return;
    }

    float rate = getFrameRate();
    if (rate > 0) {
        mRenderer->setVideoFrameRate(rate);
    }

    if (mVideoDecoder != NULL) {
        mVideoDecoder->setRenderer(mRenderer);
    }
    if (mAudioDecoder != NULL) {
        mAudioDecoder->setRenderer(mRenderer);
    }

    mLastStartedPlayingTimeNs = systemTime();

    if(mVideoDecoder != NULL){
        scheduleSetVideoDecoderTime();
    }

    postScanSources();
}