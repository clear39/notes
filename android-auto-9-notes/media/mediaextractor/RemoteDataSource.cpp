
class RemoteDataSource : public BnDtaSource {
a
}


//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/include/media/stagefright/RemoteDataSource.h
static sp<IDataSource> RemoteDataSource::wrap(const sp<DataSource> &source) {
    if (source.get() == nullptr) {
        return nullptr;
    }
    /**
     * DataSource中定义getIDataSource方法返回nullptr
    */
    if (source->getIDataSource().get() != nullptr) {
        return source->getIDataSource();
    }
    return new RemoteDataSource(source);
}


explicit RemoteDataSource::RemoteDataSource(const sp<DataSource> &source) {
    mSource = source;
    sp<MemoryDealer> memoryDealer = new MemoryDealer(kBufferSize, "RemoteDataSource");
    mMemory = memoryDealer->allocate(kBufferSize);
    if (mMemory.get() == nullptr) {
        ALOGE("Failed to allocate memory!");
    }
    mName = String8::format("RemoteDataSource(%s)", mSource->toString().string());
}



virtual sp<IMemory> RemoteDataSource::getIMemory() {
    return mMemory;
}



virtual uint32_t RemoteDataSource::getFlags() {
    return mSource->flags();
}