//  @frameworks/av/media/libstagefright/include/media/stagefright/MediaSource.h:33:
struct MediaSource : public BnMediaSource {}

//  @frameworks/av/media/libstagefright/FslExtractor.cpp
struct FslMediaSource : public MediaSource {
    explicit FslMediaSource(const sp<FslExtractor> &extractor, size_t index, sp<MetaData>& metadata);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(MediaBuffer **buffer, const ReadOptions *options);

    bool started();
    void addMediaBuffer(MediaBuffer *buffer);
    bool full();
protected:
    virtual ~FslMediaSource();
private:
    sp<FslExtractor> mExtractor;
    size_t mSourceIndex;
    Mutex mLock;
    sp<MetaData> mFormat;
    List<MediaBuffer *> mPendingFrames;
    bool mStarted;
    bool mIsAVC;
    bool mIsHEVC;
    size_t mNALLengthSize;
    size_t mBufferSize;
    uint32 mFrameSent;

    void clearPendingFrames();
    FslMediaSource(const FslMediaSource &);
    FslMediaSource &operator=(const FslMediaSource &);
};



status_t FslMediaSource::start(MetaData * /* params */)
{
    mStarted = true;
    mExtractor->ActiveTrack(mSourceIndex);//这里绕回FslExtractor->ActiveTrack中
    ALOGD("source start track %zu",mSourceIndex);
    return OK;
}