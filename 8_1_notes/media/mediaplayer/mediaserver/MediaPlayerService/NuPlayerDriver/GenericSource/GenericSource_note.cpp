

struct NuPlayer::Source : public AHandler {}



struct NuPlayer::GenericSource : public NuPlayer::Source, public MediaBufferObserver // Modular DRM
{}



//	@frameworks/av/media/libmediaplayerservice/nuplayer/GenericSource.cpp
NuPlayer::GenericSource::GenericSource(
        const sp<AMessage> &notify,
        bool uidValid,
        uid_t uid)
    : Source(notify),
      mAudioTimeUs(0),
      mAudioLastDequeueTimeUs(0),
      mVideoTimeUs(0),
      mVideoLastDequeueTimeUs(0),
      mFetchSubtitleDataGeneration(0),
      mFetchTimedTextDataGeneration(0),
      mDurationUs(-1ll),
      mAudioIsVorbis(false),
      mIsSecure(false),
      mIsStreaming(false),
      mUIDValid(uidValid),
      mUID(uid),
      mFd(-1),
      mBitrate(-1ll),
      mPendingReadBufferTypes(0) {
    ALOGV("GenericSource");

    mBufferingMonitor = new BufferingMonitor(notify);
    resetDataSource();
    mTextTrackType = TextTrackType_3GPP;
}


void NuPlayer::GenericSource::resetDataSource() {
    ALOGV("resetDataSource");

    mHTTPService.clear();
    mHttpSource.clear();
    mUri.clear();
    mUriHeaders.clear();
    if (mFd >= 0) {
        close(mFd);
        mFd = -1;
    }
    mOffset = 0;
    mLength = 0;
    mStarted = false;
    mStopRead = true;
    mPositionUs = -1;
    mStartAnchor = -1;
    mDropEndTimeUs = -1;
    mLowestlatency = -1;
    mLowLatencyRTPStreaming = false;

    if (mBufferingMonitorLooper != NULL) {
        mBufferingMonitorLooper->unregisterHandler(mBufferingMonitor->id());
        mBufferingMonitorLooper->stop();
        mBufferingMonitorLooper = NULL;
    }
    mBufferingMonitor->stop();

    mIsDrmProtected = false;
    mIsDrmReleased = false;
    mIsSecure = false;
    mMimes.clear();
}


status_t NuPlayer::GenericSource::setDataSource(int fd, int64_t offset, int64_t length) {
    ALOGV("setDataSource %d/%lld/%lld", fd, (long long)offset, (long long)length);

    resetDataSource();

    mFd = dup(fd);
    mOffset = offset;
    mLength = length;

    // delay data source creation to prepareAsync() to avoid blocking
    // the calling thread in setDataSource for any significant time.
    return OK;
}


void NuPlayer::GenericSource::prepareAsync() {
    ALOGV("prepareAsync: (looper: %d)", (mLooper != NULL));

    if (mLooper == NULL) {
        mLooper = new ALooper;
        mLooper->setName("generic");
        mLooper->start();

        mLooper->registerHandler(this);
    }

    sp<AMessage> msg = new AMessage(kWhatPrepareAsync, this);
    msg->post();
}


void NuPlayer::GenericSource::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
      case kWhatPrepareAsync:
      {
          onPrepareAsync();
          break;
      }
}

void NuPlayer::GenericSource::onPrepareAsync() {
    bool isRTPUDP = false;
    ALOGV("onPrepareAsync: mDataSource: %d", (mDataSource != NULL));

    // delayed data source creation
    if (mDataSource == NULL) {
        // set to false first, if the extractor
        // comes back as secure, set it to true then.
        mIsSecure = false;

        if (!mUri.empty()) {
            const char* uri = mUri.c_str();
            String8 contentType;

            if (!strncasecmp("http://", uri, 7) || !strncasecmp("https://", uri, 8)) {
                mHttpSource = DataSource::CreateMediaHTTP(mHTTPService);
                if (mHttpSource == NULL) {
                    ALOGE("Failed to create http source!");
                    notifyPreparedAndCleanup(UNKNOWN_ERROR);
                    return;
                }
            }
            if (!strncasecmp("rtp://", uri, 6) || !strncasecmp("udp://", uri, 6)){
                mDataSource = new StreamingDataSource(uri);
                isRTPUDP = true;
            }else{
                mDataSource = DataSource::CreateFromURI(mHTTPService, uri, &mUriHeaders, &contentType,static_cast<HTTPBase *>(mHttpSource.get()));
            }
        } else {
            if (property_get_bool("media.stagefright.extractremote", true) && !FileSource::requiresDrm(mFd, mOffset, mLength, nullptr /* mime */)) {
                sp<IBinder> binder = defaultServiceManager()->getService(String16("media.extractor"));
                if (binder != nullptr) {
                    ALOGD("FileSource remote");
                    sp<IMediaExtractorService> mediaExService(interface_cast<IMediaExtractorService>(binder));
                    sp<IDataSource> source = mediaExService->makeIDataSource(mFd, mOffset, mLength);
                    ALOGV("IDataSource(FileSource): %p %d %lld %lld", source.get(), mFd, (long long)mOffset, (long long)mLength);
                    if (source.get() != nullptr) {
                        //将IDataSource 转换成 TinyCacheSource 封装
                        mDataSource = DataSource::CreateFromIDataSource(source);
                        if (mDataSource != nullptr) {
                            // Close the local file descriptor as it is not needed anymore.
                            close(mFd);
                            mFd = -1;
                        }
                    } else {
                        ALOGW("extractor service cannot make data source");
                    }
                } else {
                    ALOGW("extractor service not running");
                }
            }
            if (mDataSource == nullptr) {
                ALOGD("FileSource local");
                mDataSource = new FileSource(mFd, mOffset, mLength);
            }
            // TODO: close should always be done on mFd, see the lines following
            // DataSource::CreateFromIDataSource above,
            // and the FileSource constructor should dup the mFd argument as needed.
            mFd = -1;
        }

        if (mDataSource == NULL) {
            ALOGE("Failed to create data source!");
            notifyPreparedAndCleanup(UNKNOWN_ERROR);
            return;
        }
    }

    if (mDataSource->flags() & DataSource::kIsCachingDataSource) {// kIsLocalFileSource
        mCachedSource = static_cast<NuCachedSource2 *>(mDataSource.get());//不执行
    }

    // For cached streaming cases, we need to wait for enough
    // buffering before reporting prepared.
    mIsStreaming = (mCachedSource != NULL || isRTPUDP);//false

    // init extractor from data source
    status_t err = initFromDataSource();

    if (err != OK) {
        ALOGE("Failed to init from data source!");
        notifyPreparedAndCleanup(err);
        return;
    }

    if (mVideoTrack.mSource != NULL) {
        sp<MetaData> meta = doGetFormatMeta(false /* audio */);
        sp<AMessage> msg = new AMessage;
        err = convertMetaDataToMessage(meta, &msg);
        if(err != OK) {
            notifyPreparedAndCleanup(err);
            return;
        }
        notifyVideoSizeChanged(msg);
    }
    //FLAG_SECURE && FLAG_PROTECTED will be known if/when prepareDrm is called by the app
    uint32_t flags = FLAG_CAN_PAUSE;
    uint32_t extractor_flags = mExtractor->flags();
    if(extractor_flags & MediaExtractor::CAN_SEEK)
        flags |= FLAG_CAN_SEEK;
    if(extractor_flags & MediaExtractor::CAN_SEEK_FORWARD)
        flags |= FLAG_CAN_SEEK_FORWARD;
    if(extractor_flags & MediaExtractor::CAN_SEEK_BACKWARD)
        flags |= FLAG_CAN_SEEK_BACKWARD;

    ALOGV("flags %x", flags);   // f

    notifyFlagsChanged(flags);

    finishPrepareAsync();

    ALOGV("onPrepareAsync: Done");
    if(isRTPUDP)
        notifyPrepared();
}


sp<MetaData> NuPlayer::GenericSource::doGetFormatMeta(bool audio) const {
    sp<IMediaSource> source = audio ? mAudioTrack.mSource : mVideoTrack.mSource;

    if (source == NULL) {
        return NULL;
    }

    return source->getFormat();
}
//  @frameworks/av/media/libmediaplayerservice/nuplayer/NuPlayer.cpp
void NuPlayer::Source::notifyVideoSizeChanged(const sp<AMessage> &format) {
    sp<AMessage> notify = dupNotify();//    sp<AMessage> dupNotify() const { return mNotify->dup(); }
    notify->setInt32("what", kWhatVideoSizeChanged);
    notify->setMessage("format", format);
    notify->post();
}


//  
void NuPlayer::Source::notifyFlagsChanged(uint32_t flags) {
    sp<AMessage> notify = dupNotify();
    notify->setInt32("what", kWhatFlagsChanged);
    notify->setInt32("flags", flags);
    notify->post();
}



status_t NuPlayer::GenericSource::initFromDataSource() {
    sp<IMediaExtractor> extractor;
    String8 mimeType;
    CHECK(mDataSource != NULL);

    const char* uri = mUri.c_str();
    if (!strncasecmp("rtp://", uri, 6) || !strncasecmp("udp://", uri, 6)) {
        mimeType = MEDIA_MIMETYPE_CONTAINER_MPEG2TS;
        int value;
        value = property_get_int32("media.rtp_streaming.low_latency", 0);
        if(value & 0x01)
            mLowLatencyRTPStreaming = true;
    }

    //这里会调用MediaExtractorService的makeExtractor方法
    extractor = MediaExtractor::Create(mDataSource, mimeType.isEmpty() ? NULL : mimeType.string());

    if (extractor == NULL) {
        ALOGE("initFromDataSource, cannot create extractor!");
        return UNKNOWN_ERROR;
    }
    mExtractor = extractor;
    mFileMeta = extractor->getMetaData();
    if (mFileMeta != NULL) {
        int64_t duration;
        if (mFileMeta->findInt64(kKeyDuration, &duration)) {
            mDurationUs = duration;
        }
    }

    int32_t totalBitrate = 0;

    size_t numtracks = extractor->countTracks();
    if (numtracks == 0) {
        ALOGE("initFromDataSource, source has no track!");
        return UNKNOWN_ERROR;
    }

    mMimes.clear();

    for (size_t i = 0; i < numtracks; ++i) {
        sp<IMediaSource> track = extractor->getTrack(i);//
        if (track == NULL) {
            continue;
        }

        sp<MetaData> meta = extractor->getTrackMetaData(i);
        if (meta == NULL) {
            ALOGE("no metadata for track %zu", i);
            return UNKNOWN_ERROR;
        }

        const char *mime;
        CHECK(meta->findCString(kKeyMIMEType, &mime));

        ALOGV("initFromDataSource track[%zu]: %s", i, mime);

        // Do the string compare immediately with "mime",
        // we can't assume "mime" would stay valid after another
        // extractor operation, some extractors might modify meta
        // during getTrack() and make it invalid.
        if (!strncasecmp(mime, "audio/", 6)) {
            if (mAudioTrack.mSource == NULL) {
                mAudioTrack.mIndex = i;
                mAudioTrack.mSource = track;//  IMediaSource
                mAudioTrack.mPackets = new AnotherPacketSource(mAudioTrack.mSource->getFormat());

                if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_VORBIS)) {
                    mAudioIsVorbis = true;
                } else {
                    mAudioIsVorbis = false;
                }

                mMimes.add(String8(mime));
            }
        } else if (!strncasecmp(mime, "video/", 6)) {
            if (mVideoTrack.mSource == NULL) {
                mVideoTrack.mIndex = i;
                mVideoTrack.mSource = track;//  IMediaSource
                mVideoTrack.mPackets = new AnotherPacketSource(mVideoTrack.mSource->getFormat());

                // video always at the beginning
                mMimes.insertAt(String8(mime), 0);
            }
        }

        mSources.push(track);
        int64_t durationUs;
        if (meta->findInt64(kKeyDuration, &durationUs)) {
            if (durationUs > mDurationUs) {
                mDurationUs = durationUs;
            }
        }

        int32_t bitrate;
        if (totalBitrate >= 0 && meta->findInt32(kKeyBitRate, &bitrate)) {
            totalBitrate += bitrate;
        } else {
            totalBitrate = -1;
        }
    }

    ALOGV("initFromDataSource mSources.size(): %zu  mIsSecure: %d  mime[0]: %s", mSources.size(), mIsSecure, (mMimes.isEmpty() ? "NONE" : mMimes[0].string()));

    if (mSources.size() == 0) {
        ALOGE("b/23705695");
        return UNKNOWN_ERROR;
    }

    // Modular DRM: The return value doesn't affect source initialization.
    (void)checkDrmInfo();

    mBitrate = totalBitrate;

    return OK;
}




void NuPlayer::GenericSource::finishPrepareAsync() {
    ALOGV("finishPrepareAsync");

    status_t err = startSources();
    if (err != OK) {
        ALOGE("Failed to init start data source!");
        notifyPreparedAndCleanup(err);
        return;
    }

    if (mIsStreaming) {
        if (mBufferingMonitorLooper == NULL) {
            mBufferingMonitor->prepare(mCachedSource, mDurationUs, mBitrate,mIsStreaming);

            mBufferingMonitorLooper = new ALooper;
            mBufferingMonitorLooper->setName("GSBMonitor");
            mBufferingMonitorLooper->start();
            mBufferingMonitorLooper->registerHandler(mBufferingMonitor);
        }

        mBufferingMonitor->ensureCacheIsFetching();
        mBufferingMonitor->restartPollBuffering();
    } else {
        notifyPrepared();
    }
}


status_t NuPlayer::GenericSource::startSources() {
    // Start the selected A/V tracks now before we start buffering.
    // Widevine sources might re-initialize crypto when starting, if we delay
    // this to start(), all data buffered during prepare would be wasted.
    // (We don't actually start reading until start().)
    //
    // TODO: this logic may no longer be relevant after the removal of widevine
    // support
    if (mAudioTrack.mSource != NULL && mAudioTrack.mSource->start() != OK) {//  IMediaSource
        ALOGE("failed to start audio track!");
        return UNKNOWN_ERROR;
    }

    if (mVideoTrack.mSource != NULL && mVideoTrack.mSource->start() != OK) {//  IMediaSource
        ALOGE("failed to start video track!");
        return UNKNOWN_ERROR;
    }

    return OK;
}

void NuPlayer::Source::notifyPrepared(status_t err) {
    ALOGV("Source::notifyPrepared %d", err);
    sp<AMessage> notify = dupNotify();
    notify->setInt32("what", kWhatPrepared);
    notify->setInt32("err", err);
    notify->post();
}

void NuPlayer::GenericSource::start() {
    ALOGI("start");

    mStopRead = false;
    if (mAudioTrack.mSource != NULL) {
        postReadBuffer(MEDIA_TRACK_TYPE_AUDIO);
    }

    if (mVideoTrack.mSource != NULL) {
        postReadBuffer(MEDIA_TRACK_TYPE_VIDEO);
    }

    mStarted = true;

    (new AMessage(kWhatStart, this))->post();
}



void NuPlayer::GenericSource::postReadBuffer(media_track_type trackType) {
    Mutex::Autolock _l(mReadBufferLock);

    if ((mPendingReadBufferTypes & (1 << trackType)) == 0) {
        mPendingReadBufferTypes |= (1 << trackType);
        sp<AMessage> msg = new AMessage(kWhatReadBuffer, this);
        msg->setInt32("trackType", trackType);
        msg->post();
    }
}


void NuPlayer::GenericSource::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
      。。。。。。
      case kWhatReadBuffer:
      {
          onReadBuffer(msg);
          break;
      }
      。。。。。。
    }
}

void NuPlayer::GenericSource::onReadBuffer(const sp<AMessage>& msg) {
    int32_t tmpType;
    CHECK(msg->findInt32("trackType", &tmpType));
    media_track_type trackType = (media_track_type)tmpType;
    readBuffer(trackType);
    {
        // only protect the variable change, as readBuffer may
        // take considerable time.
        Mutex::Autolock _l(mReadBufferLock);
        mPendingReadBufferTypes &= ~(1 << trackType);
    }
}

void NuPlayer::GenericSource::readBuffer(media_track_type trackType, int64_t seekTimeUs, MediaPlayerSeekMode mode,int64_t *actualTimeUs, bool formatChange) {
    // Do not read data if Widevine source is stopped
    //
    // TODO: revisit after widevine is removed.  May be able to
    // combine mStopRead with mStarted.
    if (mStopRead) {
        return;
    }
    Track *track;
    size_t maxBuffers = 1;
    switch (trackType) {
        case MEDIA_TRACK_TYPE_VIDEO:
            track = &mVideoTrack;
            maxBuffers = 8;  // too large of a number may influence seeks
            break;
        case MEDIA_TRACK_TYPE_AUDIO:
            track = &mAudioTrack;
            if (mVideoTrack.mSource == NULL) {
                maxBuffers = 64;
            } else {
                maxBuffers = 16;
            }
            break;
        case MEDIA_TRACK_TYPE_SUBTITLE:
            track = &mSubtitleTrack;
            break;
        case MEDIA_TRACK_TYPE_TIMEDTEXT:
            track = &mTimedTextTrack;
            break;
        default:
            TRESPASS();
    }

    if (track->mSource == NULL) {
        return;
    }

    if (actualTimeUs) {
        *actualTimeUs = seekTimeUs;
    }

    MediaSource::ReadOptions options;

    bool seeking = false;
    if (seekTimeUs >= 0) {
        options.setSeekTo(seekTimeUs, mode);
        seeking = true;
    }

    const bool couldReadMultiple = (track->mSource->supportReadMultiple());

    if (couldReadMultiple) {
        options.setNonBlocking();
    }

    int64_t videoSeekTimeResultUs = -1;
    int64_t startUs = ALooper::GetNowUs();
    int64_t nowUs = startUs;

    if(mLowLatencyRTPStreaming)
        maxBuffers = 1;

    for (size_t numBuffers = 0; numBuffers < maxBuffers; ) {
        Vector<MediaBuffer *> mediaBuffers;
        status_t err = NO_ERROR;

        if (couldReadMultiple) {
            err = track->mSource->readMultiple(&mediaBuffers, maxBuffers - numBuffers, &options);
        } else {
            MediaBuffer *mbuf = NULL;
            err = track->mSource->read(&mbuf, &options);
            if (err == OK && mbuf != NULL) {
                mediaBuffers.push_back(mbuf);
            }
        }

        options.clearNonPersistent();

        size_t id = 0;
        size_t count = mediaBuffers.size();
        for (; id < count; ++id) {
            int64_t timeUs;
            MediaBuffer *mbuf = mediaBuffers[id];
            if (!mbuf->meta_data()->findInt64(kKeyTime, &timeUs)) {
                mbuf->meta_data()->dumpToLog();
                track->mPackets->signalEOS(ERROR_MALFORMED);
                break;
            }

            if(mLowLatencyRTPStreaming && doDropPacket(trackType,timeUs)){
                continue;
            }

            if (trackType == MEDIA_TRACK_TYPE_AUDIO) {
                mAudioTimeUs = timeUs;
                mBufferingMonitor->updateQueuedTime(true /* isAudio */, timeUs);
            } else if (trackType == MEDIA_TRACK_TYPE_VIDEO) {
                mVideoTimeUs = timeUs;
                if(seeking == true && numBuffers == 0)
                    videoSeekTimeResultUs = timeUs; //save the first frame timestamp after seek in order to seek audio
                mBufferingMonitor->updateQueuedTime(false /* isAudio */, timeUs);
            }

            queueDiscontinuityIfNeeded(seeking, formatChange, trackType, track);

            sp<ABuffer> buffer = mediaBufferToABuffer(mbuf, trackType);
            if (numBuffers == 0 && actualTimeUs != nullptr) {
                *actualTimeUs = timeUs;
            }
            if (seeking && buffer != nullptr) {
                sp<AMessage> meta = buffer->meta();
                if (meta != nullptr && mode == MediaPlayerSeekMode::SEEK_CLOSEST
                        && seekTimeUs > timeUs) {
                    sp<AMessage> extra = new AMessage;
                    extra->setInt64("resume-at-mediaTimeUs", seekTimeUs);
                    meta->setMessage("extra", extra);
                }
            }

            track->mPackets->queueAccessUnit(buffer);
            formatChange = false;
            seeking = false;
            ++numBuffers;
        }
        if (id < count) {
            // Error, some mediaBuffer doesn't have kKeyTime.
            for (; id < count; ++id) {
                mediaBuffers[id]->release();
            }
            break;
        }

        if (err == WOULD_BLOCK) {
            break;
        } else if (err == INFO_FORMAT_CHANGED) {
#if 0
            track->mPackets->queueDiscontinuity(
                    ATSParser::DISCONTINUITY_FORMATCHANGE,
                    NULL,
                    false /* discard */);
#endif
        } else if (err != OK) {
            queueDiscontinuityIfNeeded(seeking, formatChange, trackType, track);
            track->mPackets->signalEOS(err);
            break;
        }
        //quit from loop when reading too many audio buffer
        nowUs = ALooper::GetNowUs();
        if(nowUs - startUs > 250000LL)
            break;
    }
    if(videoSeekTimeResultUs > 0)
        *actualTimeUs = videoSeekTimeResultUs;

    if(mLowLatencyRTPStreaming)
        notifyNeedCurrentPosition();
}



void NuPlayer::GenericSource::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
      。。。。。。
      case kWhatStart:
      case kWhatResume:
      {
          mBufferingMonitor->restartPollBuffering();
          break;
      }
      。。。。。。
    }
}










