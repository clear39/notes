//	@frameworks/av/media/libstagefright/include/media/stagefright/MediaCodec.h
struct MediaCodec : public AHandler {}


//	@frameworks/av/media/libstagefright/MediaCodec.cpp
// static
sp<MediaCodec> MediaCodec::CreateByType(const sp<ALooper> &looper, const AString &mime, bool encoder, status_t *err, pid_t pid,uid_t uid) {
    sp<MediaCodec> codec = new MediaCodec(looper, pid, uid);

    const status_t ret = codec->init(mime, true /* nameIsType */, encoder);
    if (err != NULL) {
        *err = ret;
    }
    return ret == OK ? codec : NULL; // NULL deallocates codec.
}


MediaCodec::MediaCodec(const sp<ALooper> &looper, pid_t pid, uid_t uid)
    : mState(UNINITIALIZED),
      mReleasedByResourceManager(false),
      mLooper(looper),
      mCodec(NULL),
      mReplyID(0),
      mFlags(0),
      mStickyError(OK),
      mSoftRenderer(NULL),
      mAnalyticsItem(NULL),
      mResourceManagerClient(new ResourceManagerClient(this)),	//	@frameworks/av/media/libstagefright/MediaCodec.cpp
      mResourceManagerService(new ResourceManagerServiceProxy(pid)),//	@frameworks/av/media/libstagefright/MediaCodec.cpp
      mBatteryStatNotified(false),
      mIsVideo(false),
      mVideoWidth(0),
      mVideoHeight(0),
      mRotationDegrees(0),
      mDequeueInputTimeoutGeneration(0),
      mDequeueInputReplyID(0),
      mDequeueOutputTimeoutGeneration(0),
      mDequeueOutputReplyID(0),
      mHaveInputSurface(false),
      mHavePendingInputBuffers(false) {
    if (uid == kNoUid) {
        mUid = IPCThreadState::self()->getCallingUid();
    } else {
        mUid = uid;
    }
    initAnalyticsItem();
}

void MediaCodec::initAnalyticsItem() {
    CHECK(mAnalyticsItem == NULL);
    // set up our new record, get a sessionID, put it into the in-progress list
    mAnalyticsItem = new MediaAnalyticsItem(kCodecKeyName);
    if (mAnalyticsItem != NULL) {
        (void) mAnalyticsItem->generateSessionID();
        // don't record it yet; only at the end, when we have decided that we have
        // data worth writing (e.g. .count() > 0)
    }
}


status_t MediaCodec::init(const AString &name, bool nameIsType, bool encoder) {
    mResourceManagerService->init();//	ResourceManagerServiceProxy

    // save init parameters for reset
    mInitName = name;
    mInitNameIsType = nameIsType;
    mInitIsEncoder = encoder;

    // Current video decoders do not return from OMX_FillThisBuffer
    // quickly, violating the OpenMAX specs, until that is remedied
    // we need to invest in an extra looper to free the main event
    // queue.

    mCodec = GetCodecBase(name, nameIsType);//这里得到对应的CodecBase
    if (mCodec == NULL) {
        return NAME_NOT_FOUND;
    }

    bool secureCodec = false;
    if (nameIsType && !strncasecmp(name.c_str(), "video/", 6)) {
        mIsVideo = true;
    } else {
        AString tmp = name;
        if (tmp.endsWith(".secure")) {
            secureCodec = true;
            tmp.erase(tmp.size() - 7, 7);
        }
        const sp<IMediaCodecList> mcl = MediaCodecList::getInstance();
        if (mcl == NULL) {
            mCodec = NULL;  // remove the codec.
            return NO_INIT; // if called from Java should raise IOException
        }
        ssize_t codecIdx = mcl->findCodecByName(tmp.c_str());
        if (codecIdx >= 0) {
            const sp<MediaCodecInfo> info = mcl->getCodecInfo(codecIdx);
            Vector<AString> mimes;
            info->getSupportedMimes(&mimes);
            for (size_t i = 0; i < mimes.size(); i++) {
                if (mimes[i].startsWith("video/")) {
                    mIsVideo = true;
                    break;
                }
            }
        }
    }

    if (mIsVideo) {
        // video codec needs dedicated looper
        if (mCodecLooper == NULL) {
            mCodecLooper = new ALooper;
            mCodecLooper->setName("CodecLooper");
            mCodecLooper->start(false, false, ANDROID_PRIORITY_AUDIO);
        }

        mCodecLooper->registerHandler(mCodec);
    } else {
        mLooper->registerHandler(mCodec);
    }

    mLooper->registerHandler(this);

    mCodec->setCallback(std::unique_ptr<CodecBase::CodecCallback>(new CodecCallback(new AMessage(kWhatCodecNotify, this))));
    mBufferChannel = mCodec->getBufferChannel();
    mBufferChannel->setCallback(std::unique_ptr<CodecBase::BufferCallback>(new BufferCallback(new AMessage(kWhatCodecNotify, this))));

    sp<AMessage> msg = new AMessage(kWhatInit, this);
    msg->setString("name", name);
    msg->setInt32("nameIsType", nameIsType);

    if (nameIsType) {
        msg->setInt32("encoder", encoder);
    }

    if (mAnalyticsItem != NULL) {
        if (nameIsType) {
            // name is the mime type
            mAnalyticsItem->setCString(kCodecMime, name.c_str());
        } else {
            mAnalyticsItem->setCString(kCodecCodec, name.c_str());
        }
        mAnalyticsItem->setCString(kCodecMode, mIsVideo ? "video" : "audio");
        if (nameIsType)
            mAnalyticsItem->setInt32(kCodecEncoder, encoder);
    }

    status_t err;
    Vector<MediaResource> resources;
    MediaResource::Type type = secureCodec ? MediaResource::kSecureCodec : MediaResource::kNonSecureCodec;
    MediaResource::SubType subtype = mIsVideo ? MediaResource::kVideoCodec : MediaResource::kAudioCodec;
    resources.push_back(MediaResource(type, subtype, 1));
    for (int i = 0; i <= kMaxRetry; ++i) {
        if (i > 0) {
            // Don't try to reclaim resource for the first time.
            if (!mResourceManagerService->reclaimResource(resources)) {
                break;
            }
        }

        sp<AMessage> response;
        err = PostAndAwaitResponse(msg, &response);
        if (!isResourceError(err)) {
            break;
        }
    }
    return err;
}


//static
sp<CodecBase> MediaCodec::GetCodecBase(const AString &name, bool nameIsType) {
    // at this time only ACodec specifies a mime type.
    if (nameIsType || name.startsWithIgnoreCase("omx.")) {
        return new ACodec;
    } else if (name.startsWithIgnoreCase("android.filter.")) {
        return new MediaFilter;
    } else {
        return NULL;
    }
}



