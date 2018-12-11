

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

    if (mDataSource->flags() & DataSource::kIsCachingDataSource) {
        mCachedSource = static_cast<NuCachedSource2 *>(mDataSource.get());
    }

    // For cached streaming cases, we need to wait for enough
    // buffering before reporting prepared.
    mIsStreaming = (mCachedSource != NULL || isRTPUDP);

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

    ALOGV("flags %x", flags);

    notifyFlagsChanged(flags);

    finishPrepareAsync();

    ALOGV("onPrepareAsync: Done");
    if(isRTPUDP)
        notifyPrepared();
}



status_t NuPlayer::GenericSource::initFromDataSource() {
    sp<IMediaExtractor> extractor;
    String8 mimeType;
    CHECK(mDataSource != NULL);

    const char* uri = mUri.c_str();
    if (!strncasecmp("rtp://", uri, 6)
       || !strncasecmp("udp://", uri, 6))
    {
        mimeType = MEDIA_MIMETYPE_CONTAINER_MPEG2TS;
        int value;
        value = property_get_int32("media.rtp_streaming.low_latency", 0);
        if(value & 0x01)
            mLowLatencyRTPStreaming = true;
    }

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
        sp<IMediaSource> track = extractor->getTrack(i);
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
                mAudioTrack.mSource = track;
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
                mVideoTrack.mSource = track;
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