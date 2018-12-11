//		@frameworks/av/media/libmediaplayerservice/MediaPlayerService.cpp

// 创建一个和客户断对应的播放Client
sp<IMediaPlayer> MediaPlayerService::create(const sp<IMediaPlayerClient>& client,audio_session_t audioSessionId)
{
    pid_t pid = IPCThreadState::self()->getCallingPid();
    int32_t connId = android_atomic_inc(&mNextConnId);
    sp<Client> c = new Client(this, pid, connId, client, audioSessionId,IPCThreadState::self()->getCallingUid());
    ALOGV("Create new client(%d) from pid %d, uid %d, ", connId, pid, IPCThreadState::self()->getCallingUid());
    wp<Client> w = c;
    {
        Mutex::Autolock lock(mLock);
        mClients.add(w);
    }
    return c;
}


MediaPlayerService::Client::Client(
        const sp<MediaPlayerService>& service, pid_t pid,
        int32_t connId, const sp<IMediaPlayerClient>& client,
        audio_session_t audioSessionId, uid_t uid)
{
    ALOGV("Client(%d) constructor", connId);
    mPid = pid;
    mConnId = connId;
    mService = service;
    mClient = client;
    mLoop = false;
    mStatus = NO_INIT;
    mAudioSessionId = audioSessionId;
    mUid = uid;
    mRetransmitEndpointValid = false;
    mAudioAttributes = NULL;
    mListener = new Listener(this);

#if CALLBACK_ANTAGONIZER            //  frameworks/av/media/libmediaplayerservice/MediaPlayerService.h:50:#define CALLBACK_ANTAGONIZER 0
    ALOGD("create Antagonizer");
    mAntagonizer = new Antagonizer(mListener);
#endif
}



// setDataSource动作开始/////////////////////////////////////////////////////////////////////////////////////
status_t MediaPlayerService::Client::setDataSource(int fd, int64_t offset, int64_t length)
{
    ALOGV("setDataSource fd=%d (%s), offset=%lld, length=%lld",fd, nameForFd(fd).c_str(), (long long) offset, (long long) length);
    struct stat sb;
    int ret = fstat(fd, &sb);
    if (ret != 0) {
        ALOGE("fstat(%d) failed: %d, %s", fd, ret, strerror(errno));
        return UNKNOWN_ERROR;
    }

    ALOGV("st_dev  = %llu", static_cast<unsigned long long>(sb.st_dev));
    ALOGV("st_mode = %u", sb.st_mode);
    ALOGV("st_uid  = %lu", static_cast<unsigned long>(sb.st_uid));
    ALOGV("st_gid  = %lu", static_cast<unsigned long>(sb.st_gid));
    ALOGV("st_size = %llu", static_cast<unsigned long long>(sb.st_size));

    if (offset >= sb.st_size) {
        ALOGE("offset error");
        return UNKNOWN_ERROR;
    }
    if (offset + length > sb.st_size) {
        length = sb.st_size - offset;
        ALOGV("calculated length = %lld", (long long)length);
    }

    //查找支持改格式的播放器类型
    player_type playerType = MediaPlayerFactory::getPlayerType(this,fd,offset,length);
    //创建对应播放器
    sp<MediaPlayerBase> p = setDataSource_pre(playerType);//这里默认 播放器为NuPlayerDriver
    if (p == NULL) {
        return NO_INIT;
    }
    // now set data source
    return mStatus = setDataSource_post(p, p->setDataSource(fd, offset, length));
}


sp<MediaPlayerBase> MediaPlayerService::Client::setDataSource_pre(player_type playerType)
{
    ALOGV("player type = %d", playerType);

    // create the right type of player
    sp<MediaPlayerBase> p = createPlayer(playerType);//创建对应的播放器
    if (p == NULL) {
        return p;
    }

    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16("media.extractor"));
    if (binder == NULL) {
        ALOGE("extractor service not available");
        return NULL;
    }
    sp<ServiceDeathNotifier> extractorDeathListener = new ServiceDeathNotifier(binder, p, MEDIAEXTRACTOR_PROCESS_DEATH);
    binder->linkToDeath(extractorDeathListener);

    sp<ServiceDeathNotifier> codecDeathListener;
    if (property_get_bool("persist.media.treble_omx", true)) {//true
        // Treble IOmx
        sp<IOmx> omx = IOmx::getService();
        if (omx == nullptr) {
            ALOGE("Treble IOmx not available");
            return NULL;
        }
        codecDeathListener = new ServiceDeathNotifier(omx, p, MEDIACODEC_PROCESS_DEATH);
        omx->linkToDeath(codecDeathListener, 0);
    } else {
        // Legacy IOMX
        binder = sm->getService(String16("media.codec"));
        if (binder == NULL) {
            ALOGE("codec service not available");
            return NULL;
        }
        codecDeathListener = new ServiceDeathNotifier(binder, p, MEDIACODEC_PROCESS_DEATH);
        binder->linkToDeath(codecDeathListener);
    }

    Mutex::Autolock lock(mLock);

    clearDeathNotifiers_l();
    mExtractorDeathListener = extractorDeathListener;
    mCodecDeathListener = codecDeathListener;

    //设置音频输出
    if (!p->hardwareOutput()) {
        mAudioOutput = new AudioOutput(mAudioSessionId, IPCThreadState::self()->getCallingUid(),mPid, mAudioAttributes);
        static_cast<MediaPlayerInterface*>(p.get())->setAudioSink(mAudioOutput);
    }

    return p;
}



status_t MediaPlayerService::Client::setDataSource_post(const sp<MediaPlayerBase>& p,status_t status)
{
    ALOGV(" setDataSource");
    if (status != OK) {
        ALOGE("  error: %d", status);
        return status;
    }

    // Set the re-transmission endpoint if one was chosen.
    if (mRetransmitEndpointValid) {
        status = p->setRetransmitEndpoint(&mRetransmitEndpoint);
        if (status != NO_ERROR) {
            ALOGE("setRetransmitEndpoint error: %d", status);
        }
    }

    if (status == OK) {
        Mutex::Autolock lock(mLock);
        mPlayer = p;//将创建好的播放器 赋值给成员变量
    }
    return status;
}




//设置声音输出类型有俩中方式///////////////////////////////////////////////////////////////////////
status_t MediaPlayerService::Client::setAudioStreamType(audio_stream_type_t type)
{
    ALOGV("[%d] setAudioStreamType(%d)", mConnId, type);
    // TODO: for hardware output, call player instead
    Mutex::Autolock l(mLock);
    if (mAudioOutput != 0) mAudioOutput->setAudioStreamType(type);
    return NO_ERROR;
}


status_t MediaPlayerService::Client::setAudioAttributes_l(const Parcel &parcel)
{
    if (mAudioAttributes != NULL) { free(mAudioAttributes); }
    mAudioAttributes = (audio_attributes_t *) calloc(1, sizeof(audio_attributes_t));
    if (mAudioAttributes == NULL) {
        return NO_MEMORY;
    }
    unmarshallAudioAttributes(parcel, mAudioAttributes);

    ALOGV("setAudioAttributes_l() usage=%d content=%d flags=0x%x tags=%s",
            mAudioAttributes->usage, mAudioAttributes->content_type, mAudioAttributes->flags,
            mAudioAttributes->tags);

    if (mAudioOutput != 0) {
        mAudioOutput->setAudioAttributes(mAudioAttributes);
    }
    return NO_ERROR;
}

//prepare///////////////////////////////////////////////////////////////////////
status_t MediaPlayerService::Client::prepareAsync()
{
    ALOGV("[%d] prepareAsync", mConnId);
    sp<MediaPlayerBase> p = getPlayer();
    if (p == 0) return UNKNOWN_ERROR;
    status_t ret = p->prepareAsync();
#if CALLBACK_ANTAGONIZER
    ALOGD("start Antagonizer");
    if (ret == NO_ERROR) mAntagonizer->start();
#endif
    return ret;
}



//start///////////////////////////////////////////////////////////////////////
status_t MediaPlayerService::Client::start()
{
    ALOGV("[%d] start", mConnId);
    sp<MediaPlayerBase> p = getPlayer();
    if (p == 0) return UNKNOWN_ERROR;
    p->setLooping(mLoop);
    return p->start();
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

