//	@	frameworks/av/media/libstagefright/include/media/stagefright/MPEG4Writer.h
class MPEG4Writer : public MediaWriter {}



//	@	frameworks/av/media/libstagefright/MPEG4Writer.cpp
MPEG4Writer::MPEG4Writer(int fd) {
    initInternal(fd, true /*isFirstSession*/);
}



void MPEG4Writer::initInternal(int fd, bool isFirstSession) {
    ALOGV("initInternal");
    mFd = dup(fd);
    mNextFd = -1;
    mInitCheck = mFd < 0? NO_INIT: OK;

    mInterleaveDurationUs = 1000000;

    mStartTimestampUs = -1ll;
    mStartTimeOffsetMs = -1;
    mPaused = false;
    mStarted = false;
    mWriterThreadStarted = false;
    mSendNotify = false;

    // Reset following variables for all the sessions and they will be
    // initialized in start(MetaData *param).
    mIsRealTimeRecording = true;
    mUse4ByteNalLength = true;
    mUse32BitOffset = true;
    mOffset = 0;
    mMdatOffset = 0;
    mMoovBoxBuffer = NULL;
    mMoovBoxBufferOffset = 0;
    mWriteMoovBoxToMemory = false;
    mFreeBoxOffset = 0;
    mStreamableFile = false;
    mEstimatedMoovBoxSize = 0;
    mTimeScale = -1;

    // Following variables only need to be set for the first recording session.
    // And they will stay the same for all the recording sessions.
    if (isFirstSession) {
        mMoovExtraSize = 0;
        mMetaKeys = new AMessage();
        addDeviceMeta();//添加一些系统相关信息
        mLatitudex10000 = 0;
        mLongitudex10000 = 0;
        mAreGeoTagsAvailable = false;
        mSwitchPending = false;
        mIsFileSizeLimitExplicitlyRequested = false;
    }

    // Verify mFd is seekable
    off64_t off = lseek64(mFd, 0, SEEK_SET);
    if (off < 0) {
        ALOGE("cannot seek mFd: %s (%d) %lld", strerror(errno), errno, (long long)mFd);
        release();
    }

    for (List<Track *>::iterator it = mTracks.begin(); it != mTracks.end(); ++it) {
        (*it)->resetInternal();
    }
}


void MPEG4Writer::addDeviceMeta() {
    // add device info and estimate space in 'moov'
    char val[PROPERTY_VALUE_MAX];
    size_t n;
    // meta size is estimated by adding up the following:
    // - meta header structures, which occur only once (total 66 bytes)
    // - size for each key, which consists of a fixed header (32 bytes),
    //   plus key length and data length.
    mMoovExtraSize += 66;
    if (property_get("ro.build.version.release", val, NULL) && (n = strlen(val)) > 0) {
        mMetaKeys->setString(kMetaKey_Version, val, n + 1);
        mMoovExtraSize += sizeof(kMetaKey_Version) + n + 32;
    }

    if (property_get_bool("media.recorder.show_manufacturer_and_model", false)) {
        if (property_get("ro.product.manufacturer", val, NULL) && (n = strlen(val)) > 0) {
            mMetaKeys->setString(kMetaKey_Manufacturer, val, n + 1);
            mMoovExtraSize += sizeof(kMetaKey_Manufacturer) + n + 32;
        }
        if (property_get("ro.product.model", val, NULL) && (n = strlen(val)) > 0) {
            mMetaKeys->setString(kMetaKey_Model, val, n + 1);
            mMoovExtraSize += sizeof(kMetaKey_Model) + n + 32;
        }
    }
#ifdef SHOW_MODEL_BUILD
    if (property_get("ro.build.display.id", val, NULL) && (n = strlen(val)) > 0) {
        mMetaKeys->setString(kMetaKey_Build, val, n + 1);
        mMoovExtraSize += sizeof(kMetaKey_Build) + n + 32;
    }
#endif
}




status_t MPEG4Writer::addSource(const sp<IMediaSource> &source) {

    Mutex::Autolock l(mLock);
    if (mStarted) {
        ALOGE("Attempt to add source AFTER recording is started");
        return UNKNOWN_ERROR;
    }

    CHECK(source.get() != NULL);

    const char *mime;
    source->getFormat()->findCString(kKeyMIMEType, &mime);

    //判断是否为支持格式
    if (Track::getFourCCForMime(mime) == NULL) {
        ALOGE("Unsupported mime '%s'", mime);
        return ERROR_UNSUPPORTED;
    }

    // This is a metadata track or the first track of either audio or video
    // Go ahead to add the track.
    Track *track = new Track(this, source, 1 + mTracks.size());//	List<Track *> mTracks;
    mTracks.push_back(track);

    return OK;
}

// static
const char *MPEG4Writer::Track::getFourCCForMime(const char *mime) {
    if (mime == NULL) {
        return NULL;
    }
    if (!strncasecmp(mime, "audio/", 6)) {
    	//	@frameworks/av/media/libmedia/MediaDefs.cpp:36:const char *MEDIA_MIMETYPE_AUDIO_AMR_NB = "audio/3gpp";
        if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_AMR_NB, mime)) {
            return "samr";
        //	@frameworks/av/media/libmedia/MediaDefs.cpp:37:const char *MEDIA_MIMETYPE_AUDIO_AMR_WB = "audio/amr-wb";
        } else if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_AMR_WB, mime)) {
            return "sawb";
        //	@frameworks/av/media/libmedia/MediaDefs.cpp:42:const char *MEDIA_MIMETYPE_AUDIO_AAC = "audio/mp4a-latm";
        //	@frameworks/av/media/libmedia/MediaDefs.cpp:97:const char *MEDIA_MIMETYPE_AUDIO_AAC_FSL = "audio/aac-fsl";
        } else if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_AAC, mime)
                || !strcasecmp(MEDIA_MIMETYPE_AUDIO_AAC_FSL, mime)) {
            return "mp4a";
        }
    } else if (!strncasecmp(mime, "video/", 6)) {
    	//	@frameworks/av/media/libmedia/MediaDefs.cpp:29:const char *MEDIA_MIMETYPE_VIDEO_MPEG4 = "video/mp4v-es";
        if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_MPEG4, mime)) {
            return "mp4v";
        //	@frameworks/av/media/libmedia/MediaDefs.cpp:30:const char *MEDIA_MIMETYPE_VIDEO_H263 = "video/3gpp";
        } else if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_H263, mime)) {
            return "s263";
        //	@frameworks/av/media/libmedia/MediaDefs.cpp:27:const char *MEDIA_MIMETYPE_VIDEO_AVC = "video/avc";
        } else if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_AVC, mime)) {
            return "avc1";
        //	@frameworks/av/media/libmedia/MediaDefs.cpp:28:const char *MEDIA_MIMETYPE_VIDEO_HEVC = "video/hevc";
        } else if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_HEVC, mime)) {
            return "hvc1";
        }
    } else if (!strncasecmp(mime, "application/", 12)) {
        return "mett";
    } else {
        ALOGE("Track (%s) other than video/audio/metadata is not supported", mime);
    }
    return NULL;
}






// 在相应的track创建ok之后，在进行写入
status_t MPEG4Writer::start(MetaData *param) {
    if (mInitCheck != OK) {
        return UNKNOWN_ERROR;
    }
    mStartMeta = param;

    /*
     * Check mMaxFileSizeLimitBytes at the beginning
     * since mMaxFileSizeLimitBytes may be implicitly
     * changed later for 32-bit file offset even if
     * user does not ask to set it explicitly.
     */
    if (mMaxFileSizeLimitBytes != 0) {
        mIsFileSizeLimitExplicitlyRequested = true;
    }

    int32_t use64BitOffset;
    if (param && param->findInt32(kKey64BitFileOffset, &use64BitOffset) && use64BitOffset) {
        mUse32BitOffset = false;
    }

    if (mUse32BitOffset) {
        // Implicit 32 bit file size limit
        if (mMaxFileSizeLimitBytes == 0) {
            mMaxFileSizeLimitBytes = kMax32BitFileSize;
        }

        // If file size is set to be larger than the 32 bit file
        // size limit, treat it as an error.
        if (mMaxFileSizeLimitBytes > kMax32BitFileSize) {
            ALOGW("32-bit file size limit (%" PRId64 " bytes) too big. "
                 "It is changed to %" PRId64 " bytes",
                mMaxFileSizeLimitBytes, kMax32BitFileSize);
            mMaxFileSizeLimitBytes = kMax32BitFileSize;
        }
    }

    int32_t use2ByteNalLength;
    if (param &&
        param->findInt32(kKey2ByteNalLength, &use2ByteNalLength) &&
        use2ByteNalLength) {
        mUse4ByteNalLength = false;
    }

    int32_t isRealTimeRecording;
    if (param && param->findInt32(kKeyRealTimeRecording, &isRealTimeRecording)) {
        mIsRealTimeRecording = isRealTimeRecording;
    }

    mStartTimestampUs = -1;

    if (mStarted) {
        if (mPaused) {
            mPaused = false;
            return startTracks(param);
        }
        return OK;
    }

    if (!param ||
        !param->findInt32(kKeyTimeScale, &mTimeScale)) {
        mTimeScale = 1000;
    }
    CHECK_GT(mTimeScale, 0);
    ALOGV("movie time scale: %d", mTimeScale);

    /*
     * When the requested file size limit is small, the priority
     * is to meet the file size limit requirement, rather than
     * to make the file streamable. mStreamableFile does not tell
     * whether the actual recorded file is streamable or not.
     */
    mStreamableFile =
        (mMaxFileSizeLimitBytes != 0 &&
         mMaxFileSizeLimitBytes >= kMinStreamableFileSizeInBytes);

    /*
     * mWriteMoovBoxToMemory is true if the amount of data in moov box is
     * smaller than the reserved free space at the beginning of a file, AND
     * when the content of moov box is constructed. Note that video/audio
     * frame data is always written to the file but not in the memory.
     *
     * Before stop()/reset() is called, mWriteMoovBoxToMemory is always
     * false. When reset() is called at the end of a recording session,
     * Moov box needs to be constructed.
     *
     * 1) Right before a moov box is constructed, mWriteMoovBoxToMemory
     * to set to mStreamableFile so that if
     * the file is intended to be streamable, it is set to true;
     * otherwise, it is set to false. When the value is set to false,
     * all the content of the moov box is written immediately to
     * the end of the file. When the value is set to true, all the
     * content of the moov box is written to an in-memory cache,
     * mMoovBoxBuffer, util the following condition happens. Note
     * that the size of the in-memory cache is the same as the
     * reserved free space at the beginning of the file.
     *
     * 2) While the data of the moov box is written to an in-memory
     * cache, the data size is checked against the reserved space.
     * If the data size surpasses the reserved space, subsequent moov
     * data could no longer be hold in the in-memory cache. This also
     * indicates that the reserved space was too small. At this point,
     * _all_ moov data must be written to the end of the file.
     * mWriteMoovBoxToMemory must be set to false to direct the write
     * to the file.
     *
     * 3) If the data size in moov box is smaller than the reserved
     * space after moov box is completely constructed, the in-memory
     * cache copy of the moov box is written to the reserved free
     * space. Thus, immediately after the moov is completedly
     * constructed, mWriteMoovBoxToMemory is always set to false.
     */
    mWriteMoovBoxToMemory = false;
    mMoovBoxBuffer = NULL;
    mMoovBoxBufferOffset = 0;

    writeFtypBox(param);

    mFreeBoxOffset = mOffset;

    if (mEstimatedMoovBoxSize == 0) {
        int32_t bitRate = -1;
        if (param) {
            param->findInt32(kKeyBitRate, &bitRate);
        }
        mEstimatedMoovBoxSize = estimateMoovBoxSize(bitRate);
    }
    CHECK_GE(mEstimatedMoovBoxSize, 8);
    if (mStreamableFile) {
        // Reserve a 'free' box only for streamable file
        lseek64(mFd, mFreeBoxOffset, SEEK_SET);
        writeInt32(mEstimatedMoovBoxSize);
        write("free", 4);
        mMdatOffset = mFreeBoxOffset + mEstimatedMoovBoxSize;
    } else {
        mMdatOffset = mOffset;
    }

    mOffset = mMdatOffset;
    lseek64(mFd, mMdatOffset, SEEK_SET);
    if (mUse32BitOffset) {
        write("????mdat", 8);
    } else {
        write("\x00\x00\x00\x01mdat????????", 16);
    }

    status_t err = startWriterThread();
    if (err != OK) {
        return err;
    }

    err = startTracks(param);
    if (err != OK) {
        return err;
    }

    mStarted = true;
    return OK;
}


void MPEG4Writer::writeFtypBox(MetaData *param) {
    beginBox("ftyp");

    int32_t fileType;
    if (param && param->findInt32(kKeyFileType, &fileType) && fileType != OUTPUT_FORMAT_MPEG_4) {
        writeFourcc("3gp4");
        writeInt32(0);
        writeFourcc("isom");
        writeFourcc("3gp4");
    } else {
        writeFourcc("mp42");
        writeInt32(0);
        writeFourcc("isom");
        writeFourcc("mp42");
    }

    endBox();
}

void MPEG4Writer::beginBox(uint32_t id) {
    mBoxes.push_back(mWriteMoovBoxToMemory? mMoovBoxBufferOffset: mOffset);
    writeInt32(0);
    writeInt32(id);
}

void MPEG4Writer::endBox() {
    CHECK(!mBoxes.empty());

    off64_t offset = *--mBoxes.end();
    mBoxes.erase(--mBoxes.end());

    if (mWriteMoovBoxToMemory) {
       int32_t x = htonl(mMoovBoxBufferOffset - offset);
       memcpy(mMoovBoxBuffer + offset, &x, 4);
    } else {
        lseek64(mFd, offset, SEEK_SET);
        writeInt32(mOffset - offset);
        mOffset -= 4;
        lseek64(mFd, mOffset, SEEK_SET);
    }
}



status_t MPEG4Writer::startWriterThread() {
    ALOGV("startWriterThread");

    mDone = false;
    mIsFirstChunk = true;
    mDriftTimeUs = 0;
    for (List<Track *>::iterator it = mTracks.begin(); it != mTracks.end(); ++it) {
        ChunkInfo info;
        info.mTrack = *it;
        info.mPrevChunkTimestampUs = 0;
        info.mMaxInterChunkDurUs = 0;
        mChunkInfos.push_back(info);
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mThread, &attr, ThreadWrapper, this);
    pthread_attr_destroy(&attr);
    mWriterThreadStarted = true;
    return OK;
}

// static
void *MPEG4Writer::ThreadWrapper(void *me) {
    ALOGV("ThreadWrapper: %p", me);
    MPEG4Writer *writer = static_cast<MPEG4Writer *>(me);
    writer->threadFunc();
    return NULL;
}

void MPEG4Writer::threadFunc() {
    ALOGV("threadFunc");

    prctl(PR_SET_NAME, (unsigned long)"MPEG4Writer", 0, 0, 0);

    Mutex::Autolock autoLock(mLock);
    while (!mDone) {
        Chunk chunk;
        bool chunkFound = false;

        while (!mDone && !(chunkFound = findChunkToWrite(&chunk))) {
            mChunkReadyCondition.wait(mLock);
        }

        // In real time recording mode, write without holding the lock in order
        // to reduce the blocking time for media track threads.
        // Otherwise, hold the lock until the existing chunks get written to the
        // file.
        if (chunkFound) {
            if (mIsRealTimeRecording) {
                mLock.unlock();
            }
            writeChunkToFile(&chunk);
            if (mIsRealTimeRecording) {
                mLock.lock();
            }
        }
    }

    writeAllChunks();
}







status_t MPEG4Writer::startTracks(MetaData *params) {
    if (mTracks.empty()) {
        ALOGE("No source added");
        return INVALID_OPERATION;
    }

    for (List<Track *>::iterator it = mTracks.begin();
         it != mTracks.end(); ++it) {
        status_t err = (*it)->start(params);

        if (err != OK) {
            for (List<Track *>::iterator it2 = mTracks.begin();
                 it2 != it; ++it2) {
                (*it2)->stop();
            }

            return err;
        }
    }
    return OK;
}


status_t MPEG4Writer::Track::start(MetaData *params) {

    if (!mDone && mPaused) {
        mPaused = false;
        mResumed = true;
        return OK;
    }

    int64_t startTimeUs;
    if (params == NULL || !params->findInt64(kKeyTime, &startTimeUs)) {
        startTimeUs = 0;
    }
    mStartTimeRealUs = startTimeUs;

    int32_t rotationDegrees;
    if (mIsVideo && params && params->findInt32(kKeyRotation, &rotationDegrees)) {
        mRotation = rotationDegrees;
    }

    initTrackingProgressStatus(params);

    sp<MetaData> meta = new MetaData;
    if (mOwner->isRealTimeRecording() && mOwner->numTracks() > 1) {
        /*
         * This extra delay of accepting incoming audio/video signals
         * helps to align a/v start time at the beginning of a recording
         * session, and it also helps eliminate the "recording" sound for
         * camcorder applications.
         *
         * If client does not set the start time offset, we fall back to
         * use the default initial delay value.
         */
        int64_t startTimeOffsetUs = mOwner->getStartTimeOffsetMs() * 1000LL;
        if (startTimeOffsetUs < 0) {  // Start time offset was not set
            startTimeOffsetUs = kInitialDelayTimeUs;
        }
        startTimeUs += startTimeOffsetUs;
        ALOGI("Start time offset: %" PRId64 " us", startTimeOffsetUs);
    }

    meta->setInt64(kKeyTime, startTimeUs);

    status_t err = mSource->start(meta.get());
    if (err != OK) {
        mDone = mReachedEOS = true;
        return err;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    mDone = false;
    mStarted = true;
    mTrackDurationUs = 0;
    mReachedEOS = false;
    mEstimatedTrackSizeBytes = 0;
    mMdatSizeBytes = 0;
    mMaxChunkDurationUs = 0;
    mLastDecodingTimeUs = -1;

    pthread_create(&mThread, &attr, ThreadWrapper, this);
    pthread_attr_destroy(&attr);

    return OK;
}


// static
void *MPEG4Writer::Track::ThreadWrapper(void *me) {
    Track *track = static_cast<Track *>(me);

    status_t err = track->threadEntry();
    return (void *)(uintptr_t)err;
}



 status_t MPEG4Writer::Track::threadEntry() {
    int32_t count = 0;
    const int64_t interleaveDurationUs = mOwner->interleaveDuration();
    const bool hasMultipleTracks = (mOwner->numTracks() > 1);
    int64_t chunkTimestampUs = 0;
    int32_t nChunks = 0;
    int32_t nActualFrames = 0;        // frames containing non-CSD data (non-0 length)
    int32_t nZeroLengthFrames = 0;
    int64_t lastTimestampUs = 0;      // Previous sample time stamp
    int64_t lastDurationUs = 0;       // Between the previous two samples
    int64_t currDurationTicks = 0;    // Timescale based ticks
    int64_t lastDurationTicks = 0;    // Timescale based ticks
    int32_t sampleCount = 1;          // Sample count in the current stts table entry
    uint32_t previousSampleSize = 0;  // Size of the previous sample
    int64_t previousPausedDurationUs = 0;
    int64_t timestampUs = 0;
    int64_t cttsOffsetTimeUs = 0;
    int64_t currCttsOffsetTimeTicks = 0;   // Timescale based ticks
    int64_t lastCttsOffsetTimeTicks = -1;  // Timescale based ticks
    int32_t cttsSampleCount = 0;           // Sample count in the current ctts table entry
    uint32_t lastSamplesPerChunk = 0;

    if (mIsAudio) {
        prctl(PR_SET_NAME, (unsigned long)"AudioTrackEncoding", 0, 0, 0);
    } else if (mIsVideo) {
        prctl(PR_SET_NAME, (unsigned long)"VideoTrackEncoding", 0, 0, 0);
    } else {
        prctl(PR_SET_NAME, (unsigned long)"MetadataTrackEncoding", 0, 0, 0);
    }

    if (mOwner->isRealTimeRecording()) {
        androidSetThreadPriority(0, ANDROID_PRIORITY_AUDIO);
    }

    sp<MetaData> meta_data;

    status_t err = OK;
    MediaBuffer *buffer;
    const char *trackName = getTrackType();
    while (!mDone && (err = mSource->read(&buffer)) == OK) {
        if (buffer->range_length() == 0) {
            buffer->release();
            buffer = NULL;
            ++nZeroLengthFrames;
            continue;
        }

        // If the codec specific data has not been received yet, delay pause.
        // After the codec specific data is received, discard what we received
        // when the track is to be paused.
        if (mPaused && !mResumed) {
            buffer->release();
            buffer = NULL;
            continue;
        }

        ++count;

        int32_t isCodecConfig;
        if (buffer->meta_data()->findInt32(kKeyIsCodecConfig, &isCodecConfig)
                && isCodecConfig) {
            // if config format (at track addition) already had CSD, keep that
            // UNLESS we have not received any frames yet.
            // TODO: for now the entire CSD has to come in one frame for encoders, even though
            // they need to be spread out for decoders.
            if (mGotAllCodecSpecificData && nActualFrames > 0) {
                ALOGI("ignoring additional CSD for video track after first frame");
            } else {
                mMeta = mSource->getFormat(); // get output format after format change
                status_t err;
                if (mIsAvc) {
                    err = makeAVCCodecSpecificData(
                            (const uint8_t *)buffer->data()
                                + buffer->range_offset(),
                            buffer->range_length());
                } else if (mIsHevc) {
                    err = makeHEVCCodecSpecificData(
                            (const uint8_t *)buffer->data()
                                + buffer->range_offset(),
                            buffer->range_length());
                } else if (mIsMPEG4) {
                    copyCodecSpecificData((const uint8_t *)buffer->data() + buffer->range_offset(),
                            buffer->range_length());
                }
            }

            buffer->release();
            buffer = NULL;
            if (OK != err) {
                mSource->stop();
                mOwner->notify(MEDIA_RECORDER_TRACK_EVENT_ERROR,
                       mTrackId | MEDIA_RECORDER_TRACK_ERROR_GENERAL, err);
                break;
            }

            mGotAllCodecSpecificData = true;
            continue;
        }

        // Per-frame metadata sample's size must be smaller than max allowed.
        if (!mIsVideo && !mIsAudio && buffer->range_length() >= kMaxMetadataSize) {
            ALOGW("Buffer size is %zu. Maximum metadata buffer size is %lld for %s track",
                    buffer->range_length(), (long long)kMaxMetadataSize, trackName);
            buffer->release();
            mSource->stop();
            mIsMalformed = true;
            break;
        }

        ++nActualFrames;

        // Make a deep copy of the MediaBuffer and Metadata and release
        // the original as soon as we can
        MediaBuffer *copy = new MediaBuffer(buffer->range_length());
        memcpy(copy->data(), (uint8_t *)buffer->data() + buffer->range_offset(),
                buffer->range_length());
        copy->set_range(0, buffer->range_length());
        meta_data = new MetaData(*buffer->meta_data().get());
        buffer->release();
        buffer = NULL;

        if (mIsAvc || mIsHevc) StripStartcode(copy);

        size_t sampleSize = copy->range_length();
        if (mIsAvc || mIsHevc) {
            if (mOwner->useNalLengthFour()) {
                sampleSize += 4;
            } else {
                sampleSize += 2;
            }
        }

        // Max file size or duration handling
        mMdatSizeBytes += sampleSize;
        updateTrackSizeEstimate();

        if (mOwner->exceedsFileSizeLimit()) {
            if (mOwner->switchFd() != OK) {
                ALOGW("Recorded file size exceeds limit %" PRId64 "bytes",
                        mOwner->mMaxFileSizeLimitBytes);
                mSource->stop();
                mOwner->notify(
                        MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED, 0);
            } else {
                ALOGV("%s Current recorded file size exceeds limit %" PRId64 "bytes. Switching output",
                        getTrackType(), mOwner->mMaxFileSizeLimitBytes);
            }
            copy->release();
            break;
        }

        if (mOwner->exceedsFileDurationLimit()) {
            ALOGW("Recorded file duration exceeds limit %" PRId64 "microseconds",
                    mOwner->mMaxFileDurationLimitUs);
            mOwner->notify(MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_MAX_DURATION_REACHED, 0);
            copy->release();
            mSource->stop();
            break;
        }

        if (mOwner->approachingFileSizeLimit()) {
            mOwner->notifyApproachingLimit();
        }

        int32_t isSync = false;
        meta_data->findInt32(kKeyIsSyncFrame, &isSync);
        CHECK(meta_data->findInt64(kKeyTime, &timestampUs));

        // For video, skip the first several non-key frames until getting the first key frame.
        if (mIsVideo && !mGotStartKeyFrame && !isSync) {
            ALOGD("Video skip non-key frame");
            copy->release();
            continue;
        }
        if (mIsVideo && isSync) {
            mGotStartKeyFrame = true;
        }
////////////////////////////////////////////////////////////////////////////////
        if (mStszTableEntries->count() == 0) {
            mFirstSampleTimeRealUs = systemTime() / 1000;
            mStartTimestampUs = timestampUs;
            mOwner->setStartTimestampUs(mStartTimestampUs);
            previousPausedDurationUs = mStartTimestampUs;
        }

        if (mResumed) {
            int64_t durExcludingEarlierPausesUs = timestampUs - previousPausedDurationUs;
            if (WARN_UNLESS(durExcludingEarlierPausesUs >= 0ll, "for %s track", trackName)) {
                copy->release();
                mSource->stop();
                mIsMalformed = true;
                break;
            }

            int64_t pausedDurationUs = durExcludingEarlierPausesUs - mTrackDurationUs;
            if (WARN_UNLESS(pausedDurationUs >= lastDurationUs, "for %s track", trackName)) {
                copy->release();
                mSource->stop();
                mIsMalformed = true;
                break;
            }

            previousPausedDurationUs += pausedDurationUs - lastDurationUs;
            mResumed = false;
        }
        TimestampDebugHelperEntry timestampDebugEntry;
        timestampUs -= previousPausedDurationUs;
        timestampDebugEntry.pts = timestampUs;
        if (WARN_UNLESS(timestampUs >= 0ll, "for %s track", trackName)) {
            copy->release();
            mSource->stop();
            mIsMalformed = true;
            break;
        }

        if (mIsVideo) {
            /*
             * Composition time: timestampUs
             * Decoding time: decodingTimeUs
             * Composition time offset = composition time - decoding time
             */
            int64_t decodingTimeUs;
            CHECK(meta_data->findInt64(kKeyDecodingTime, &decodingTimeUs));
            decodingTimeUs -= previousPausedDurationUs;

            // ensure non-negative, monotonic decoding time
            if (mLastDecodingTimeUs < 0) {
                decodingTimeUs = std::max((int64_t)0, decodingTimeUs);
            } else {
                // increase decoding time by at least the larger vaule of 1 tick and
                // 0.1 milliseconds. This needs to take into account the possible
                // delta adjustment in DurationTicks in below.
                decodingTimeUs = std::max(mLastDecodingTimeUs +
                        std::max(100, divUp(1000000, mTimeScale)), decodingTimeUs);
            }

            mLastDecodingTimeUs = decodingTimeUs;
            timestampDebugEntry.dts = decodingTimeUs;
            timestampDebugEntry.frameType = isSync ? "Key frame" : "Non-Key frame";
            // Insert the timestamp into the mTimestampDebugHelper
            if (mTimestampDebugHelper.size() >= kTimestampDebugCount) {
                mTimestampDebugHelper.pop_front();
            }
            mTimestampDebugHelper.push_back(timestampDebugEntry);

            cttsOffsetTimeUs =
                    timestampUs + kMaxCttsOffsetTimeUs - decodingTimeUs;
            if (WARN_UNLESS(cttsOffsetTimeUs >= 0ll, "for %s track", trackName)) {
                copy->release();
                mSource->stop();
                mIsMalformed = true;
                break;
            }

            timestampUs = decodingTimeUs;
            ALOGV("decoding time: %" PRId64 " and ctts offset time: %" PRId64,
                timestampUs, cttsOffsetTimeUs);

            // Update ctts box table if necessary
            currCttsOffsetTimeTicks =
                    (cttsOffsetTimeUs * mTimeScale + 500000LL) / 1000000LL;
            if (WARN_UNLESS(currCttsOffsetTimeTicks <= 0x0FFFFFFFFLL, "for %s track", trackName)) {
                copy->release();
                mSource->stop();
                mIsMalformed = true;
                break;
            }

            if (mStszTableEntries->count() == 0) {
                // Force the first ctts table entry to have one single entry
                // so that we can do adjustment for the initial track start
                // time offset easily in writeCttsBox().
                lastCttsOffsetTimeTicks = currCttsOffsetTimeTicks;
                addOneCttsTableEntry(1, currCttsOffsetTimeTicks);
                cttsSampleCount = 0;      // No sample in ctts box is pending
            } else {
                if (currCttsOffsetTimeTicks != lastCttsOffsetTimeTicks) {
                    addOneCttsTableEntry(cttsSampleCount, lastCttsOffsetTimeTicks);
                    lastCttsOffsetTimeTicks = currCttsOffsetTimeTicks;
                    cttsSampleCount = 1;  // One sample in ctts box is pending
                } else {
                    ++cttsSampleCount;
                }
            }

            // Update ctts time offset range
            if (mStszTableEntries->count() == 0) {
                mMinCttsOffsetTicks = currCttsOffsetTimeTicks;
                mMaxCttsOffsetTicks = currCttsOffsetTimeTicks;
            } else {
                if (currCttsOffsetTimeTicks > mMaxCttsOffsetTicks) {
                    mMaxCttsOffsetTicks = currCttsOffsetTimeTicks;
                } else if (currCttsOffsetTimeTicks < mMinCttsOffsetTicks) {
                    mMinCttsOffsetTicks = currCttsOffsetTimeTicks;
                    mMinCttsOffsetTimeUs = cttsOffsetTimeUs;
                }
            }
        }

        if (mOwner->isRealTimeRecording()) {
            if (mIsAudio) {
                updateDriftTime(meta_data);
            }
        }

        if (WARN_UNLESS(timestampUs >= 0ll, "for %s track", trackName)) {
            copy->release();
            mSource->stop();
            mIsMalformed = true;
            break;
        }

        ALOGV("%s media time stamp: %" PRId64 " and previous paused duration %" PRId64,
                trackName, timestampUs, previousPausedDurationUs);
        if (timestampUs > mTrackDurationUs) {
            mTrackDurationUs = timestampUs;
        }

        // We need to use the time scale based ticks, rather than the
        // timestamp itself to determine whether we have to use a new
        // stts entry, since we may have rounding errors.
        // The calculation is intended to reduce the accumulated
        // rounding errors.
        currDurationTicks =
            ((timestampUs * mTimeScale + 500000LL) / 1000000LL -
                (lastTimestampUs * mTimeScale + 500000LL) / 1000000LL);
        if (currDurationTicks < 0ll) {
            ALOGE("do not support out of order frames (timestamp: %lld < last: %lld for %s track",
                    (long long)timestampUs, (long long)lastTimestampUs, trackName);
            copy->release();
            mSource->stop();
            mIsMalformed = true;
            break;
        }

        // if the duration is different for this sample, see if it is close enough to the previous
        // duration that we can fudge it and use the same value, to avoid filling the stts table
        // with lots of near-identical entries.
        // "close enough" here means that the current duration needs to be adjusted by less
        // than 0.1 milliseconds
        if (lastDurationTicks && (currDurationTicks != lastDurationTicks)) {
            int64_t deltaUs = ((lastDurationTicks - currDurationTicks) * 1000000LL
                    + (mTimeScale / 2)) / mTimeScale;
            if (deltaUs > -100 && deltaUs < 100) {
                // use previous ticks, and adjust timestamp as if it was actually that number
                // of ticks
                currDurationTicks = lastDurationTicks;
                timestampUs += deltaUs;
            }
        }
        mStszTableEntries->add(htonl(sampleSize));
        if (mStszTableEntries->count() > 2) {

            // Force the first sample to have its own stts entry so that
            // we can adjust its value later to maintain the A/V sync.
            if (mStszTableEntries->count() == 3 || currDurationTicks != lastDurationTicks) {
                addOneSttsTableEntry(sampleCount, lastDurationTicks);
                sampleCount = 1;
            } else {
                ++sampleCount;
            }

        }
        if (mSamplesHaveSameSize) {
            if (mStszTableEntries->count() >= 2 && previousSampleSize != sampleSize) {
                mSamplesHaveSameSize = false;
            }
            previousSampleSize = sampleSize;
        }
        ALOGV("%s timestampUs/lastTimestampUs: %" PRId64 "/%" PRId64,
                trackName, timestampUs, lastTimestampUs);
        lastDurationUs = timestampUs - lastTimestampUs;
        lastDurationTicks = currDurationTicks;
        lastTimestampUs = timestampUs;

        if (isSync != 0) {
            addOneStssTableEntry(mStszTableEntries->count());
        }

        if (mTrackingProgressStatus) {
            if (mPreviousTrackTimeUs <= 0) {
                mPreviousTrackTimeUs = mStartTimestampUs;
            }
            trackProgressStatus(timestampUs);
        }
        if (!hasMultipleTracks) {
            off64_t offset = (mIsAvc || mIsHevc) ? mOwner->addMultipleLengthPrefixedSamples_l(copy)
                                 : mOwner->addSample_l(copy);

            uint32_t count = (mOwner->use32BitFileOffset()
                        ? mStcoTableEntries->count()
                        : mCo64TableEntries->count());

            if (count == 0) {
                addChunkOffset(offset);
            }
            copy->release();
            copy = NULL;
            continue;
        }

        mChunkSamples.push_back(copy);
        if (interleaveDurationUs == 0) {
            addOneStscTableEntry(++nChunks, 1);
            bufferChunk(timestampUs);
        } else {
            if (chunkTimestampUs == 0) {
                chunkTimestampUs = timestampUs;
            } else {
                int64_t chunkDurationUs = timestampUs - chunkTimestampUs;
                if (chunkDurationUs > interleaveDurationUs) {
                    if (chunkDurationUs > mMaxChunkDurationUs) {
                        mMaxChunkDurationUs = chunkDurationUs;
                    }
                    ++nChunks;
                    if (nChunks == 1 ||  // First chunk
                        lastSamplesPerChunk != mChunkSamples.size()) {
                        lastSamplesPerChunk = mChunkSamples.size();
                        addOneStscTableEntry(nChunks, lastSamplesPerChunk);
                    }
                    bufferChunk(timestampUs);
                    chunkTimestampUs = timestampUs;
                }
            }
        }

    }

    if (isTrackMalFormed()) {
        dumpTimeStamps();
        err = ERROR_MALFORMED;
    }

    mOwner->trackProgressStatus(mTrackId, -1, err);

    // Last chunk
    if (!hasMultipleTracks) {
        addOneStscTableEntry(1, mStszTableEntries->count());
    } else if (!mChunkSamples.empty()) {
        addOneStscTableEntry(++nChunks, mChunkSamples.size());
        bufferChunk(timestampUs);
    }

    // We don't really know how long the last frame lasts, since
    // there is no frame time after it, just repeat the previous
    // frame's duration.
    if (mStszTableEntries->count() == 1) {
        lastDurationUs = 0;  // A single sample's duration
        lastDurationTicks = 0;
    } else {
        ++sampleCount;  // Count for the last sample
    }

    if (mStszTableEntries->count() <= 2) {
        addOneSttsTableEntry(1, lastDurationTicks);
        if (sampleCount - 1 > 0) {
            addOneSttsTableEntry(sampleCount - 1, lastDurationTicks);
        }
    } else {
        addOneSttsTableEntry(sampleCount, lastDurationTicks);
    }

    // The last ctts box may not have been written yet, and this
    // is to make sure that we write out the last ctts box.
    if (currCttsOffsetTimeTicks == lastCttsOffsetTimeTicks) {
        if (cttsSampleCount > 0) {
            addOneCttsTableEntry(cttsSampleCount, lastCttsOffsetTimeTicks);
        }
    }

    mTrackDurationUs += lastDurationUs;
    mReachedEOS = true;

    sendTrackSummary(hasMultipleTracks);

    ALOGI("Received total/0-length (%d/%d) buffers and encoded %d frames. - %s",
            count, nZeroLengthFrames, mStszTableEntries->count(), trackName);
    if (mIsAudio) {
        ALOGI("Audio track drift time: %" PRId64 " us", mOwner->getDriftTimeUs());
    }

    if (err == ERROR_END_OF_STREAM) {
        return OK;
    }
    return err;
}







