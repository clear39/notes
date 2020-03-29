
//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDriver.cpp

NuPlayerDriver::NuPlayerDriver(pid_t pid)
    : mState(STATE_IDLE),
      mIsAsyncPrepare(false),
      mAsyncResult(UNKNOWN_ERROR),
      mSetSurfaceInProgress(false),
      mDurationUs(-1),
      mPositionUs(-1),
      mSeekInProgress(false),
      mPlayingTimeUs(0),
      mRebufferingTimeUs(0),
      mRebufferingEvents(0),
      mRebufferingAtExit(false),
      mLooper(new ALooper),
      mMediaClock(new MediaClock),
      mPlayer(new NuPlayer(pid, mMediaClock)),
      mPlayerFlags(0),
      mAnalyticsItem(NULL),
      mClientUid(-1),
      mAtEOS(false),
      mLooping(false),
      mAutoLoop(false) {
    ALOGD("NuPlayerDriver(%p) created, clientPid(%d)", this, pid);
    mLooper->setName("NuPlayerDriver Looper");

    mMediaClock->init();

    // set up an analytics record
    mAnalyticsItem = new MediaAnalyticsItem(kKeyPlayer);

    mLooper->start(false, /* runOnCallingThread */ true,  /* canCallJava */ PRIORITY_AUDIO);

    mLooper->registerHandler(mPlayer);

    mPlayer->init(this);
}




status_t NuPlayerDriver::prepareAsync() {
    ALOGV("prepareAsync(%p)", this);
    Mutex::Autolock autoLock(mLock);

    switch (mState) {
        case STATE_UNPREPARED:
            mState = STATE_PREPARING;
            mIsAsyncPrepare = true;
            mPlayer->prepareAsync();
            return OK;
        case STATE_STOPPED:
            // this is really just paused. handle as seek to start
            mAtEOS = false;
            mState = STATE_STOPPED_AND_PREPARING;
            mIsAsyncPrepare = true;
            mPlayer->seekToAsync(0, MediaPlayerSeekMode::SEEK_PREVIOUS_SYNC /* mode */,true /* needNotify */);
            return OK;
        default:
            return INVALID_OPERATION;
    };
}



void NuPlayer::prepareAsync() {
    ALOGV("prepareAsync");

    (new AMessage(kWhatPrepare, this))->post();
}


