//  @frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDecoderBase.h
struct NuPlayer::DecoderBase : public AHandler {}


//  @frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDecoder.h
struct NuPlayer::Decoder : public DecoderBase {}



// @frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDecoder.cpp
// 这个是在 NuPlayer 中创建
NuPlayer::Decoder::Decoder(
        const sp<AMessage> &notify,  //回调给 NuPlayer 的消息 kWhatAudioNotify或者kWhatVideoNotify
        const sp<Source> &source,
        pid_t pid,
        uid_t uid,
        const sp<Renderer> &renderer,
        const sp<Surface> &surface,
        const sp<CCDecoder> &ccDecoder)
    : DecoderBase(notify),
      mSurface(surface),
      mSource(source),
      mRenderer(renderer),
      mCCDecoder(ccDecoder),
      mPid(pid),
      mUid(uid),
      mSkipRenderingUntilMediaTimeUs(-1ll),
      mNumFramesTotal(0ll),
      mNumInputFramesDropped(0ll),
      mNumOutputFramesDropped(0ll),
      mVideoWidth(0),
      mVideoHeight(0),
      mIsAudio(true),
      mIsVideoAVC(false),
      mIsSecure(false),
      mIsEncrypted(false),
      mIsEncryptedObservedEarlier(false),
      mFormatChangePending(false),
      mTimeChangePending(false),
      mFrameRateTotal(kDefaultVideoFrameRateTotal),
      mPlaybackSpeed(1.0f),
      mNumVideoTemporalLayerTotal(1), // decode all layers
      mNumVideoTemporalLayerAllowed(1),
      mCurrentMaxVideoTemporalLayerId(0),
      mResumePending(false),
      mComponentName("decoder") {
    mCodecLooper = new ALooper;
    mCodecLooper->setName("NPDecoder-CL");
    mCodecLooper->start(false, false, ANDROID_PRIORITY_AUDIO);
    mVideoTemporalLayerAggregateFps[0] = mFrameRateTotal;
}



void NuPlayer::DecoderBase::init() {
    mDecoderLooper->registerHandler(this);//注册Handler,用于回调
}


void NuPlayer::DecoderBase::configure(const sp<AMessage> &format) {
    sp<AMessage> msg = new AMessage(kWhatConfigure, this);
    msg->setMessage("format", format);
    msg->post();
}

void NuPlayer::DecoderBase::onMessageReceived(const sp<AMessage> &msg) {

    switch (msg->what()) {
        case kWhatConfigure:
        {
            sp<AMessage> format;
            CHECK(msg->findMessage("format", &format));
            onConfigure(format);
            break;
        }
    }
}


void NuPlayer::Decoder::onConfigure(const sp<AMessage> &format) {
    CHECK(mCodec == NULL);

    mFormatChangePending = false;
    mTimeChangePending = false;

    ++mBufferGeneration;

    AString mime;
    CHECK(format->findString("mime", &mime));

    mIsAudio = !strncasecmp("audio/", mime.c_str(), 6);
    mIsVideoAVC = !strcasecmp(MEDIA_MIMETYPE_VIDEO_AVC, mime.c_str());

    mComponentName = mime;
    mComponentName.append(" decoder");
    ALOGV("[%s] onConfigure (surface=%p)", mComponentName.c_str(), mSurface.get());

    mCodec = MediaCodec::CreateByType(mCodecLooper, mime.c_str(), false /* encoder */, NULL /* err */, mPid, mUid);
    if (mCodec != NULL) {
        int32_t numChannels;

        mCodec->getName(&mComponentName);
        if (format->findInt32("channel-count", &numChannels) && numChannels > 2 && mComponentName.endsWith("aac.hw-based")) {
            ALOGV("AAC hw-based decoder doesn't support multi-channel, switch to sw-based decoder");
            mCodec->release();
            mComponentName.erase(mComponentName.find("hw-based"), 8);
            mComponentName.append("sw-based");
            mCodec = MediaCodec::CreateByComponentName(mCodecLooper, mComponentName.c_str(), NULL /* err */, mPid, mUid);
        }
    }
    int32_t secure = 0;
    if (format->findInt32("secure", &secure) && secure != 0) {
        if (mCodec != NULL) {
            mCodec->getName(&mComponentName);
            mComponentName.append(".secure");
            mCodec->release();
            ALOGI("[%s] creating", mComponentName.c_str());
            mCodec = MediaCodec::CreateByComponentName(
                    mCodecLooper, mComponentName.c_str(), NULL /* err */, mPid, mUid);
        }
    }
    if (mCodec == NULL) {
        ALOGE("Failed to create %s%s decoder",
                (secure ? "secure " : ""), mime.c_str());
        handleError(UNKNOWN_ERROR);
        return;
    }
    mIsSecure = secure;

    mCodec->getName(&mComponentName);

    if (mComponentName.startsWith("OMX.Freescale.std.video_decoder") && mComponentName.endsWith("hw-based")){
        if( property_get_int32("media.hantro_vpu.enable-tile", 0)){
            format->setInt32("color-format", 0x7F000003);//OMX_COLOR_FormatYUV420SemiPlanar8x4Tiled
        }else if( property_get_int32("media.amphion_vpu.enable-tile", 0)){
            format->setInt32("color-format", 0x7F000002);//OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled
        }else{
            format->setInt32("color-format", 21);//OMX_COLOR_FormatYUV420SemiPlanar
        }

        int tunneled = property_get_int32("media.omx.enable-tunnel", 0);
        format->setInt32("feature-tunneled-playback", tunneled);
    }

    status_t err;
    if (mSurface != NULL) {
        // disconnect from surface as MediaCodec will reconnect
        err = nativeWindowDisconnect(mSurface.get(), "onConfigure");
        // We treat this as a warning, as this is a preparatory step.
        // Codec will try to connect to the surface, which is where
        // any error signaling will occur.
        ALOGW_IF(err != OK, "failed to disconnect from surface: %d", err);
    }

    // Modular DRM
    void *pCrypto;
    if (!format->findPointer("crypto", &pCrypto)) {
        pCrypto = NULL;
    }
    sp<ICrypto> crypto = (ICrypto*)pCrypto;
    // non-encrypted source won't have a crypto
    mIsEncrypted = (crypto != NULL);
    // configure is called once; still using OR in case the behavior changes.
    mIsEncryptedObservedEarlier = mIsEncryptedObservedEarlier || mIsEncrypted;
    ALOGV("onConfigure mCrypto: %p (%d)  mIsSecure: %d",
            crypto.get(), (crypto != NULL ? crypto->getStrongCount() : 0), mIsSecure);

    err = mCodec->configure(
            format, mSurface, crypto, 0 /* flags */);

    if (err != OK) {
        ALOGE("Failed to configure [%s] decoder (err=%d)", mComponentName.c_str(), err);
        mCodec->release();
        mCodec.clear();
        handleError(err);
        return;
    }
    rememberCodecSpecificData(format);

    // the following should work in configured state
    CHECK_EQ((status_t)OK, mCodec->getOutputFormat(&mOutputFormat));
    CHECK_EQ((status_t)OK, mCodec->getInputFormat(&mInputFormat));

    mStats->setString("mime", mime.c_str());
    mStats->setString("component-name", mComponentName.c_str());

    if (!mIsAudio) {
        int32_t width, height;
        if (mOutputFormat->findInt32("width", &width)
                && mOutputFormat->findInt32("height", &height)) {
            mStats->setInt32("width", width);
            mStats->setInt32("height", height);
        }

        int32_t tunneled = 0;
        if (format->findInt32("feature-tunneled-playback", &tunneled) && tunneled) {
            // work round to make surface set the display width/height to sideband stream
            int32_t left = 0, top = 0, right = 0, bottom = 0;

            right = width > 0 ? width : 128;
            bottom = height > 0 ? height : 128;
            format->setRect("crop", left/2, top, right/2, bottom);
            sp<AMessage> notify = mNotify->dup();
            notify->setInt32("what", kWhatVideoSizeChanged);
            notify->setMessage("format", format);
            notify->post();
    }
    }

    sp<AMessage> reply = new AMessage(kWhatCodecNotify, this);
    mCodec->setCallback(reply);

    err = mCodec->start();
    if (err != OK) {
        ALOGE("Failed to start [%s] decoder (err=%d)", mComponentName.c_str(), err);
        mCodec->release();
        mCodec.clear();
        handleError(err);
        return;
    }

    releaseAndResetMediaBuffers();

    mPaused = false;
    mResumePending = false;
}