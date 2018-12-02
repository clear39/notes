// @frameworks/av/include/media/IMediaLogService.h
class IMediaLogService: public IInterface
{
public:
    DECLARE_META_INTERFACE(MediaLogService);

    virtual void    registerWriter(const sp<IMemory>& shared, size_t size, const char *name) = 0;
    virtual void    unregisterWriter(const sp<IMemory>& shared) = 0;

    virtual void    requestMergeWakeup() = 0;
};



//	@frameworks/av/services/medialog/MediaLogService.h
class MediaLogService : public BinderService<MediaLogService>, public BnMediaLogService{}



//	@frameworks/av/services/medialog/MediaLogService.cpp
MediaLogService::MediaLogService() :
    BnMediaLogService(),
    mMergerShared((NBLog::Shared*) malloc(NBLog::Timeline::sharedSize(kMergeBufferSize))),
    mMerger(mMergerShared, kMergeBufferSize),
    mMergeReader(mMergerShared, kMergeBufferSize, mMerger),
    mMergeThread(new NBLog::MergeThread(mMerger))
{
    mMergeThread->run("MergeThread");
}


void MediaLogService::registerWriter(const sp<IMemory>& shared, size_t size, const char *name)
{
    if (IPCThreadState::self()->getCallingUid() != AID_AUDIOSERVER || shared == 0 ||
            size < kMinSize || size > kMaxSize || name == NULL ||
            shared->size() < NBLog::Timeline::sharedSize(size)) {
        return;
    }
    sp<NBLog::Reader> reader(new NBLog::Reader(shared, size));
    NBLog::NamedReader namedReader(reader, name);
    Mutex::Autolock _l(mLock);
    mNamedReaders.add(namedReader);
    mMerger.addReader(namedReader);
}

void MediaLogService::unregisterWriter(const sp<IMemory>& shared)
{
    if (IPCThreadState::self()->getCallingUid() != AID_AUDIOSERVER || shared == 0) {
        return;
    }
    Mutex::Autolock _l(mLock);
    for (size_t i = 0; i < mNamedReaders.size(); ) {
        if (mNamedReaders[i].reader()->isIMemory(shared)) {
            mNamedReaders.removeAt(i);
        } else {
            i++;
        }
    }
}

void MediaLogService::requestMergeWakeup() {
    mMergeThread->wakeup();
}