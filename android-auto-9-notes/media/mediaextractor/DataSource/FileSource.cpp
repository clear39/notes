


FileSource::FileSource(const char *filename)
    : mFd(-1),
      mOffset(0),
      mLength(-1),
      mName("<null>"),
      mDecryptHandle(NULL),
      mDrmManagerClient(NULL),
      mDrmBufOffset(0),
      mDrmBufSize(0),
      mDrmBuf(NULL){

    if (filename) {
        mName = String8::format("FileSource(%s)", filename);
    }
    ALOGV("%s", filename);
    mFd = open(filename, O_LARGEFILE | O_RDONLY);

    if (mFd >= 0) {
        mLength = lseek64(mFd, 0, SEEK_END);
    } else {
        ALOGE("Failed to open file '%s'. (%s)", filename, strerror(errno));
    }
}

status_t FileSource::initCheck() const {
    return mFd >= 0 ? OK : NO_INIT;
}


    virtual uint32_t FileSource::flags() {
        return kIsLocalFileSource;
    }



ssize_t FileSource::readAtDRM(off64_t offset, void *data, size_t size) {
    size_t DRM_CACHE_SIZE = 1024;
    if (mDrmBuf == NULL) {
        mDrmBuf = new unsigned char[DRM_CACHE_SIZE];
    }

    if (mDrmBuf != NULL && mDrmBufSize > 0 && (offset + mOffset) >= mDrmBufOffset && (offset + mOffset + size) <= static_cast<size_t>(mDrmBufOffset + mDrmBufSize)) {
        /* Use buffered data */
        memcpy(data, (void*)(mDrmBuf+(offset+mOffset-mDrmBufOffset)), size);
        return size;
    } else if (size <= DRM_CACHE_SIZE) {
        /* Buffer new data */
        mDrmBufOffset =  offset + mOffset;
        mDrmBufSize = mDrmManagerClient->pread(mDecryptHandle, mDrmBuf,DRM_CACHE_SIZE, offset + mOffset);
        if (mDrmBufSize > 0) {
            int64_t dataRead = 0;
            dataRead = size > static_cast<size_t>(mDrmBufSize) ? mDrmBufSize : size;
            memcpy(data, (void*)mDrmBuf, dataRead);
            return dataRead;
        } else {
            return mDrmBufSize;
        }
    } else {
        /* Too big chunk to cache. Call DRM directly */
        return mDrmManagerClient->pread(mDecryptHandle, data, size, offset + mOffset);
    }
}