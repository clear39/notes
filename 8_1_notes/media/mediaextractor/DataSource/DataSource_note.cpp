//  @frameworks/av/media/libstagefright/include/media/stagefright/DataSource.h
class DataSource : public RefBase {}




//  @frameworks/av/media/libstagefright/DataSource.cpp
sp<DataSource> DataSource::CreateFromFd(int fd, int64_t offset, int64_t length) {
    sp<FileSource> source = new FileSource(fd, offset, length);
    return source->initCheck() != OK ? nullptr : source;
}









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

    enum {
        kBufferSize = 64 * 1024,
    };


    explicit RemoteDataSource(const sp<DataSource> &source) {
        mSource = source;
        sp<MemoryDealer> memoryDealer = new MemoryDealer(kBufferSize, "RemoteDataSource");//64k
        mMemory = memoryDealer->allocate(kBufferSize);
        if (mMemory.get() == nullptr) {
            ALOGE("Failed to allocate memory!");
        }
        mName = String8::format("RemoteDataSource(%s)", mSource->toString().string());
    }



    virtual uint32_t getFlags() {
        return mSource->flags();
    }

}






sp<DataSource> DataSource::CreateFromIDataSource(const sp<IDataSource> &source) {
    return new TinyCacheSource(new CallbackDataSource(source));
}

//  @frameworks/av/media/libstagefright/include/CallbackDataSource.h
class CallbackDataSource : public DataSource {

}

class TinyCacheSource : public DataSource {

}


//  @frameworks/av/media/libstagefright/CallbackDataSource.cpp
CallbackDataSource::CallbackDataSource(const sp<IDataSource>& binderDataSource)
    : mIDataSource(binderDataSource),mIsClosed(false) { //      sp<IDataSource> mIDataSource;
    // Set up the buffer to read into.
    mMemory = mIDataSource->getIMemory();
    mName = String8::format("CallbackDataSource(%s)", mIDataSource->toString().string());

}



TinyCacheSource::TinyCacheSource(const sp<DataSource>& source)
    : mSource(source), mCachedOffset(0), mCachedSize(0) {
    mName = String8::format("TinyCacheSource(%s)", mSource->toString().string());
}


