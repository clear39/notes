//	@frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerRenderer.cpp
NuPlayer::Renderer::Renderer(
        const sp<MediaPlayerBase::AudioSink> &sink,
        const sp<AMessage> &notify,
        uint32_t flags)
    : mAudioSink(sink),
      mUseVirtualAudioSink(false),
      mNotify(notify),
      mFlags(flags),
      mNumFramesWritten(0),
      mDrainAudioQueuePending(false),
      mDrainVideoQueuePending(false),
      mAudioQueueGeneration(0),
      mVideoQueueGeneration(0),
      mAudioDrainGeneration(0),
      mVideoDrainGeneration(0),
      mAudioEOSGeneration(0),
      mPlaybackSettings(AUDIO_PLAYBACK_RATE_DEFAULT),
      mAudioFirstAnchorTimeMediaUs(-1),
      mAnchorTimeMediaUs(-1),
      mAnchorNumFramesWritten(-1),
      mVideoLateByUs(0ll),
      mHasAudio(false),
      mHasVideo(false),
      mNotifyCompleteAudio(false),
      mNotifyCompleteVideo(false),
      mSyncQueues(false),
      mPaused(false),
      mPauseDrainAudioAllowedUs(0),
      mVideoSampleReceived(false),
      mVideoRenderingStarted(false),
      mVideoRenderingStartGeneration(0),
      mAudioRenderingStartGeneration(0),
      mRenderingDataDelivered(false),
      mNextAudioClockUpdateTimeUs(-1),
      mLastAudioMediaTimeUs(-1),
      mAudioOffloadPauseTimeoutGeneration(0),
      mAudioTornDown(false),
      mCurrentOffloadInfo(AUDIO_INFO_INITIALIZER),
      mCurrentPcmInfo(AUDIO_PCMINFO_INITIALIZER),
      mTotalBuffersQueued(0),
      mLastAudioBufferDrained(0),
      mUseAudioCallback(false),
      mWakeLock(new AWakeLock()),
      mContinusDrop(0){
    mMediaClock = new MediaClock;
    mPlaybackRate = mPlaybackSettings.mSpeed;
    mMediaClock->setPlaybackRate(mPlaybackRate);
}




void NuPlayer::Renderer::setVideoFrameRate(float fps) {
    sp<AMessage> msg = new AMessage(kWhatSetVideoFrameRate, this);
    msg->setFloat("frame-rate", fps);
    msg->post();
}


void NuPlayer::Renderer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        ......
        case kWhatSetVideoFrameRate:
        {
            float fps;
            CHECK(msg->findFloat("frame-rate", &fps));
            onSetVideoFrameRate(fps);
            break;
        }
        ......
    }
}


void NuPlayer::Renderer::onSetVideoFrameRate(float fps) {
    if (mVideoScheduler == NULL) {
        mVideoScheduler = new VideoFrameScheduler();
    }
    mVideoScheduler->init(fps);
}







