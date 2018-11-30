//	@frameworks/av/media/libstagefright/DataSource.cpp
sp<IDataSource> DataSource::asIDataSource() {
    return RemoteDataSource::wrap(sp<DataSource>(this));
}



//	@frameworks/av/include/media/stagefright/RemoteDataSource.h
// Originally in MediaExtractor.cpp
class RemoteDataSource : public BnDataSource {
public:
    static sp<IDataSource> wrap(const sp<DataSource> &source) {
        if (source.get() == nullptr) {
            return nullptr;
        }
        if (source->getIDataSource().get() != nullptr) {
            return source->getIDataSource();
        }
        return new RemoteDataSource(source);
    }



    explicit RemoteDataSource(const sp<DataSource> &source) {
        mSource = source;
        sp<MemoryDealer> memoryDealer = new MemoryDealer(kBufferSize, "RemoteDataSource");
        mMemory = memoryDealer->allocate(kBufferSize);
        if (mMemory.get() == nullptr) {
            ALOGE("Failed to allocate memory!");
        }
        mName = String8::format("RemoteDataSource(%s)", mSource->toString().string());
    }
}