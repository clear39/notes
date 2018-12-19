//	@frameworks/av/media/libstagefright/include/media/stagefright/MediaSource.h
struct MediaSource : public BnMediaSource {}


//	@frameworks/av/media/libstagefright/include/media/stagefright/MediaAdapter.h
struct MediaAdapter : public MediaSource, public MediaBufferObserver {}

//	@frameworks/av/media/libstagefright/MediaAdapter.cpp
MediaAdapter::MediaAdapter(const sp<MetaData> &meta)
    : mCurrentMediaBuffer(NULL),
      mStarted(false),
      mOutputFormat(meta) {
}

sp<MetaData> MediaAdapter::getFormat() {
    Mutex::Autolock autoLock(mAdapterLock);
    return mOutputFormat;
}



status_t MediaAdapter::pushBuffer(MediaBuffer *buffer) {
    if (buffer == NULL) {
        ALOGE("pushBuffer get an NULL buffer");
        return -EINVAL;
    }

    Mutex::Autolock autoLock(mAdapterLock);
    if (!mStarted) {
        ALOGE("pushBuffer called before start");
        return INVALID_OPERATION;
    }
    mCurrentMediaBuffer = buffer;
    mBufferReadCond.signal();

    ALOGV("wait for the buffer returned @ pushBuffer! %p", buffer);
    mBufferReturnedCond.wait(mAdapterLock);

    return OK;
}




status_t MediaAdapter::read(MediaBuffer **buffer, const ReadOptions * /* options */) {
    Mutex::Autolock autoLock(mAdapterLock);
    if (!mStarted) {
        ALOGV("Read before even started!");
        return ERROR_END_OF_STREAM;
    }

    while (mCurrentMediaBuffer == NULL && mStarted) {
        ALOGV("waiting @ read()");
        mBufferReadCond.wait(mAdapterLock);
    }

    if (!mStarted) {
        ALOGV("read interrupted after stop");
        CHECK(mCurrentMediaBuffer == NULL);
        return ERROR_END_OF_STREAM;
    }

    CHECK(mCurrentMediaBuffer != NULL);

    *buffer = mCurrentMediaBuffer;
    mCurrentMediaBuffer = NULL;
    (*buffer)->setObserver(this);

    return OK;
}