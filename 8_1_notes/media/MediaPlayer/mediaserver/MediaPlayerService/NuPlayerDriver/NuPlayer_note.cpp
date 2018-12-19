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
            mSource->prepareAsync();//  GenericSource->prepareAsync();
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

    if (mVideoDecoder != NULL) {    // @frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDecoder.cpp
        mVideoDecoder->setRenderer(mRenderer);
    }
    if (mAudioDecoder != NULL) {       //   @frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDecoder.cpp
        mAudioDecoder->setRenderer(mRenderer);
    }

    mLastStartedPlayingTimeNs = systemTime();

    if(mVideoDecoder != NULL){
        scheduleSetVideoDecoderTime();
    }

    postScanSources();
}


void NuPlayer::scheduleSetVideoDecoderTime() {
    sp<AMessage> msg = new AMessage(kWhatSetTime, this);
    msg->setInt32("generation", mSetVideoTimeGeneration);
    msg->setInt32("pause", mPaused);
    msg->post();



void NuPlayer::postScanSources() {
    if (mScanSourcesPending) {
        return;
    }

    sp<AMessage> msg = new AMessage(kWhatScanSources, this);
    msg->setInt32("generation", mScanSourcesGeneration);
    msg->post();

    mScanSourcesPending = true;
}


void NuPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatScanSources:
        {
            int32_t generation;
            CHECK(msg->findInt32("generation", &generation));
            if (generation != mScanSourcesGeneration) {
                // Drop obsolete msg.
                break;
            }

            mScanSourcesPending = false;

            ALOGV("scanning sources haveAudio=%d, haveVideo=%d", mAudioDecoder != NULL, mVideoDecoder != NULL);

            bool mHadAnySourcesBefore = (mAudioDecoder != NULL) || (mVideoDecoder != NULL);
            bool rescan = false;

            // initialize video before audio because successful initialization of
            // video may change deep buffer mode of audio.
            if (mSurface != NULL) {
                if (instantiateDecoder(false, &mVideoDecoder) == -EWOULDBLOCK) {
                    rescan = true;
                }
            }

            // Don't try to re-open audio sink if there's an existing decoder.
            if (mAudioSink != NULL && mAudioDecoder == NULL) {
                if (instantiateDecoder(true, &mAudioDecoder) == -EWOULDBLOCK) {
                    rescan = true;
                }
            }

            if (!mHadAnySourcesBefore && (mAudioDecoder != NULL || mVideoDecoder != NULL)) {
                // This is the first time we've found anything playable.

                if (mSourceFlags & Source::FLAG_DYNAMIC_DURATION) {
                    schedulePollDuration();
                }
            }


#if 0
            if(mAudioDecoder != NULL && mVideoDecoder != NULL)
                mRenderer->enableSyncQueue(true);
#endif

            status_t err;
            if ((err = mSource->feedMoreTSData()) != OK) {
                if (mAudioDecoder == NULL && mVideoDecoder == NULL) {
                    // We're not currently decoding anything (no audio or
                    // video tracks found) and we just ran out of input data.

                    if (err == ERROR_END_OF_STREAM) {
                        notifyListener(MEDIA_PLAYBACK_COMPLETE, 0, 0);
                    } else {
                        notifyListener(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);
                    }
                }
                break;
            }

            if (rescan) {
                msg->post(100000ll);
                mScanSourcesPending = true;
            }
            break;
        }
    }
}



status_t NuPlayer::instantiateDecoder(bool audio, sp<DecoderBase> *decoder, bool checkAudioModeChange) {
    // The audio decoder could be cleared by tear down. If still in shut down
    // process, no need to create a new audio decoder.
    if (*decoder != NULL || (audio && mFlushingAudio == SHUT_DOWN)) {
        return OK;
    }

    ALOGV("instantiateDecoder audio:%d",audio);

    sp<AMessage> format = mSource->getFormat(audio);

    if (format == NULL) {
        return UNKNOWN_ERROR;
    } else {
        status_t err;
        if (format->findInt32("err", &err) && err) {
            return err;
        }
    }

    format->setInt32("priority", 0 /* realtime */);

    if (!audio) {
        AString mime;
        CHECK(format->findString("mime", &mime));

        bool bVideoIsAVC = !strcasecmp(MEDIA_MIMETYPE_VIDEO_AVC, mime.c_str());
        if (bVideoIsAVC && mSource->isAVCReorderDisabled())
            format->setString("disreorder", "1");
        else
            format->setString("disreorder", "0");

        sp<AMessage> ccNotify = new AMessage(kWhatClosedCaptionNotify, this);
        if (mCCDecoder == NULL) {
            mCCDecoder = new CCDecoder(ccNotify);
        }

        if (mSourceFlags & Source::FLAG_SECURE) {
            format->setInt32("secure", true);
        }

        if (mSourceFlags & Source::FLAG_PROTECTED) {
            format->setInt32("protected", true);
        }

        float rate = getFrameRate();
        if (rate > 0) {
            format->setFloat("operating-rate", rate * mPlaybackSettings.mSpeed);
        }
    }

    if (audio) {
        sp<AMessage> notify = new AMessage(kWhatAudioNotify, this);
        ++mAudioDecoderGeneration;
        notify->setInt32("generation", mAudioDecoderGeneration);

        if (checkAudioModeChange) {
            determineAudioModeChange(format);
        }

        if(bEnablePassThrough){
            AString mime;
            CHECK(format->findString("mime", &mime));
            mSource->setOffloadAudio(false /* offload */);

            //use pass through decoder for test now.
            const bool hasVideo = (mSource->getFormat(false /*audio */) != NULL);
            format->setInt32("has-video", hasVideo);
            if(0 == strcasecmp(mime.c_str(), MEDIA_MIMETYPE_AUDIO_AC3))
                *decoder = new DecoderPassThroughAC3(notify, mSource, mRenderer);
            else if( 0 == strcmp(mime.c_str(), MEDIA_MIMETYPE_AUDIO_EAC3))
                *decoder = new DecoderPassThroughDDP(notify, mSource, mRenderer);
        }else if (mOffloadAudio) {
            mSource->setOffloadAudio(true /* offload */);

            const bool hasVideo = (mSource->getFormat(false /*audio */) != NULL);
            format->setInt32("has-video", hasVideo);
            *decoder = new DecoderPassThrough(notify, mSource, mRenderer);
            ALOGV("instantiateDecoder audio DecoderPassThrough  hasVideo: %d", hasVideo);
        } else {
            mSource->setOffloadAudio(false /* offload */);
            //  创建音频 Decoder解码类
            *decoder = new Decoder(notify, mSource, mPID, mUID, mRenderer);
            ALOGV("instantiateDecoder audio Decoder");
        }
        mAudioDecoderError = false;
    } else {
        sp<AMessage> notify = new AMessage(kWhatVideoNotify, this);
        ++mVideoDecoderGeneration;
        notify->setInt32("generation", mVideoDecoderGeneration);
        
        //创建视频Decoder解码类
        *decoder = new Decoder(notify, mSource, mPID, mUID, mRenderer, mSurface, mCCDecoder);
        mVideoDecoderError = false;

        // enable FRC if high-quality AV sync is requested, even if not
        // directly queuing to display, as this will even improve textureview
        // playback.
        {
            if (property_get_bool("persist.sys.media.avsync", false)) {
                format->setInt32("auto-frc", 1);
            }
        }
    }
    format->setInt32("streaming", mStreaming?1:0);

    (*decoder)->init();//这里只是注册了handler

    // Modular DRM
    if (mIsDrmProtected) {
        format->setPointer("crypto", mCrypto.get());
        ALOGV("instantiateDecoder: mCrypto: %p (%d) isSecure: %d", mCrypto.get(),
                (mCrypto != NULL ? mCrypto->getStrongCount() : 0),
                (mSourceFlags & Source::FLAG_SECURE) != 0);
    }

    (*decoder)->configure(format);

    if (!audio) {//如果为视频，执行
        sp<AMessage> params = new AMessage();
        float rate = getFrameRate();
        if (rate > 0) {
            params->setFloat("frame-rate-total", rate);
        }

        sp<MetaData> fileMeta = getFileMeta();
        if (fileMeta != NULL) {
            int32_t videoTemporalLayerCount;
            if (fileMeta->findInt32(kKeyTemporalLayerCount, &videoTemporalLayerCount) & videoTemporalLayerCount > 0) {
                params->setInt32("temporal-layer-count", videoTemporalLayerCount);
            }
        }

        // paused before start, send the state to video decoder
        if (mPaused) {
            params->setInt64("media-time", mPreviousSeekTimeUs);
            params->setInt32("pause", mPaused);
        }

        if (params->countEntries() > 0) {
            (*decoder)->setParameters(params);
        }
    }
    return OK;
}



