//	@frameworks/av/media/libstagefright/include/media/stagefright/MediaMuxer.h
class MediaMuxer : public RefBase {}


//	@frameworks/av/media/libstagefright/MediaMuxer.cpp
MediaMuxer::MediaMuxer(int fd, OutputFormat format)
    : mFormat(format),
      mState(UNINITIALIZED) {
    if (format == OUTPUT_FORMAT_MPEG_4 || format == OUTPUT_FORMAT_THREE_GPP) {
        mWriter = new MPEG4Writer(fd);
    } else if (format == OUTPUT_FORMAT_WEBM) {
        mWriter = new WebmWriter(fd);
    }

    if (mWriter != NULL) {
        mFileMeta = new MetaData;
        mState = INITIALIZED;
    }
}



status_t MediaMuxer::start() {
    Mutex::Autolock autoLock(mMuxerLock);
    if (mState == INITIALIZED) {
        mState = STARTED;
        mFileMeta->setInt32(kKeyRealTimeRecording, false);
        return mWriter->start(mFileMeta.get());
    } else {
        ALOGE("start() is called in invalid state %d", mState);
        return INVALID_OPERATION;
    }
}

ssize_t MediaMuxer::addTrack(const sp<AMessage> &format) {
    Mutex::Autolock autoLock(mMuxerLock);

    if (format.get() == NULL) {
        ALOGE("addTrack() get a null format");
        return -EINVAL;
    }

    if (mState != INITIALIZED) {
        ALOGE("addTrack() must be called after constructor and before start().");
        return INVALID_OPERATION;
    }

    sp<MetaData> trackMeta = new MetaData;
    convertMessageToMetaData(format, trackMeta);//将format的信息恢复到MetaData结构中

    sp<MediaAdapter> newTrack = new MediaAdapter(trackMeta);
    status_t result = mWriter->addSource(newTrack);
    if (result == OK) {
    	//	Vector< sp<MediaAdapter> > mTrackList;  // Each track has its MediaAdapter.
        return mTrackList.add(newTrack);//注意返回给应用上层的为mTrackList索引
    }
    return -1;
}



status_t MediaMuxer::writeSampleData(const sp<ABuffer> &buffer, size_t trackIndex,int64_t timeUs, uint32_t flags) {
    Mutex::Autolock autoLock(mMuxerLock);

    if (buffer.get() == NULL) {
        ALOGE("WriteSampleData() get an NULL buffer.");
        return -EINVAL;
    }

    if (mState != STARTED) {
        ALOGE("WriteSampleData() is called in invalid state %d", mState);
        return INVALID_OPERATION;
    }

    if (trackIndex >= mTrackList.size()) {
        ALOGE("WriteSampleData() get an invalid index %zu", trackIndex);
        return -EINVAL;
    }

    MediaBuffer* mediaBuffer = new MediaBuffer(buffer);

    mediaBuffer->add_ref(); // Released in MediaAdapter::signalBufferReturned().
    mediaBuffer->set_range(buffer->offset(), buffer->size());

    sp<MetaData> sampleMetaData = mediaBuffer->meta_data();
    sampleMetaData->setInt64(kKeyTime, timeUs);
    // Just set the kKeyDecodingTime as the presentation time for now.
    sampleMetaData->setInt64(kKeyDecodingTime, timeUs);

    if (flags & MediaCodec::BUFFER_FLAG_SYNCFRAME) {
        sampleMetaData->setInt32(kKeyIsSyncFrame, true);
    }

    sp<MediaAdapter> currentTrack = mTrackList[trackIndex];//得到上一次通过 addTrack 创建的 MediaAdapter
    // This pushBuffer will wait until the mediaBuffer is consumed.
    return currentTrack->pushBuffer(mediaBuffer);
}
