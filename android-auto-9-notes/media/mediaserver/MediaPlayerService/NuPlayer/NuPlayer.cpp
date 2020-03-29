//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayer.cpp
NuPlayer::NuPlayer(pid_t pid, const sp<MediaClock> &mediaClock)
    : mUIDValid(false),
      mPID(pid),
      mMediaClock(mediaClock),
      mSourceFlags(0),
      mOffloadAudio(false),
      mAudioDecoderGeneration(0),
      mVideoDecoderGeneration(0),
      mRendererGeneration(0),
      mLastStartedPlayingTimeNs(0),
      mLastStartedRebufferingTimeNs(0),
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
    CHECK(mediaClock != NULL);
    clearFlushComplete();
    mRendering = false;
    bEnablePassThrough = false;
    mNoAudioDecoder = false;
}



void NuPlayer::init(const wp<NuPlayerDriver> &driver) {
    mDriver = driver;

    sp<AMessage> notify = new AMessage(kWhatMediaClockNotify, this);
    mMediaClock->setNotificationMessage(notify);
}




void NuPlayer::setDataSourceAsync(const sp<IStreamSource> &source) {
    sp<AMessage> msg = new AMessage(kWhatSetDataSource, this);

    sp<AMessage> notify = new AMessage(kWhatSourceNotify, this);

    msg->setObject("source", new StreamingSource(notify, source));
    msg->post();
    mDataSourceType = DATA_SOURCE_TYPE_STREAM;
}




void NuPlayer::prepareAsync() {
    ALOGV("prepareAsync");
    (new AMessage(kWhatPrepare, this))->post();
}

