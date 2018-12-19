//  @frameworks/av/include/media/MediaPlayerInterface.h
class MediaPlayerBase : public RefBase{}

//  @frameworks/av/include/media/MediaPlayerInterface.h
// Implement this class for media players that use the AudioFlinger software mixer
class MediaPlayerInterface : public MediaPlayerBase{}


//	@frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayerDriver.cpp
struct NuPlayerDriver : public MediaPlayerInterface {}


NuPlayerDriver::NuPlayerDriver(pid_t pid)
    : mState(STATE_IDLE),
      mIsAsyncPrepare(false),
      mAsyncResult(UNKNOWN_ERROR),
      mSetSurfaceInProgress(false),
      mDurationUs(-1),
      mPositionUs(-1),
      mSeekInProgress(false),
      mPlayingTimeUs(0),
      mLooper(new ALooper),//注意这里
      mPlayer(new NuPlayer(pid)),//注意这里
      mPlayerFlags(0),
      mAnalyticsItem(NULL),
      mClientUid(-1),
      mAtEOS(false),
      mLooping(false),
      mAutoLoop(false) {
    ALOGD("NuPlayerDriver(%p) created, clientPid(%d)", this, pid);
    mLooper->setName("NuPlayerDriver Looper");

    // set up an analytics record
    mAnalyticsItem = new MediaAnalyticsItem(kKeyPlayer);
    mAnalyticsItem->generateSessionID();

    mLooper->start(false, /* runOnCallingThread */true,  /* canCallJava */PRIORITY_AUDIO);

    mLooper->registerHandler(mPlayer);

    mPlayer->setDriver(this);//这里将自己传给NuPlayer，以便信息回馈
}


//setDataSource
status_t NuPlayerDriver::setDataSource(int fd, int64_t offset, int64_t length) {
    ALOGV("setDataSource(%p) file(%d)", this, fd);
    Mutex::Autolock autoLock(mLock);

    if (mState != STATE_IDLE) {
        return INVALID_OPERATION;
    }

    mState = STATE_SET_DATASOURCE_PENDING;

    mPlayer->setDataSourceAsync(fd, offset, length);

    while (mState == STATE_SET_DATASOURCE_PENDING) {
        mCondition.wait(mLock);
    }

    return mAsyncResult;
}


//回调
void NuPlayerDriver::notifySetDataSourceCompleted(status_t err) {
    Mutex::Autolock autoLock(mLock);

    CHECK_EQ(mState, STATE_SET_DATASOURCE_PENDING);

    mAsyncResult = err;
    mState = (err == OK) ? STATE_UNPREPARED : STATE_IDLE;
    mCondition.broadcast();
}





//prepare
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


//这里分俩中情况









status_t NuPlayerDriver::setLooping(int loop /*= 0*/) {
    mLooping = loop != 0;
    return OK;
}




status_t NuPlayerDriver::start() {
    ALOGD("start(%p), state is %d, eos is %d", this, mState, mAtEOS);
    Mutex::Autolock autoLock(mLock);
    return start_l();
}

status_t NuPlayerDriver::start_l() {
    switch (mState) {
        case STATE_UNPREPARED:
        {
            status_t err = prepare_l();

            if (err != OK) {
                return err;
            }

            CHECK_EQ(mState, STATE_PREPARED);

            // fall through
        }

        case STATE_PAUSED:
        case STATE_STOPPED_AND_PREPARED:
        case STATE_PREPARED:
        {
            mPlayer->start();

            // fall through
        }

        case STATE_RUNNING:
        {
            if (mAtEOS) {
                mPlayer->seekToAsync(0);
                mAtEOS = false;
                mPositionUs = -1;
            }
            break;
        }

        default:
            return INVALID_OPERATION;
    }

    mState = STATE_RUNNING;

    return OK;
}

