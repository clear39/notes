
// A caching DataSource that wraps a CallbackDataSource. For reads smaller
// than kCacheSize it will read up to kCacheSize ahead and cache it.
// This reduces the number of binder round trips to the IDataSource and has a significant
// impact on time taken for filetype sniffing and metadata extraction.
class TinyCacheSource : public DataSource {

}

//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/include/CallbackDataSource.h
TinyCacheSource::TinyCacheSource(const sp<DataSource>& source)
    : mSource(source), mCachedOffset(0), mCachedSize(0) {
    mName = String8::format("TinyCacheSource(%s)", mSource->toString().string());
}



status_t TinyCacheSource::initCheck() const {
    return mSource->initCheck();
}



uint32_t TinyCacheSource::flags() {
    return mSource->flags();
}