//	@frameworks/av/media/libstagefright/MediaExtractor.cpp
// static
sp<IMediaExtractor> MediaExtractor::Create(const sp<DataSource> &source , const char *mime) {
    ALOGV("MediaExtractor::Create %s", mime);

    if (!property_get_bool("media.stagefright.extractremote", true)) {//没有设置该属性，返回默认值true
        // local extractor
        ALOGW("creating media extractor in calling process");
        return CreateFromService(source, mime);
    } else {//执行这里
        // remote extractor
        ALOGV("get service manager");
        sp<IBinder> binder = defaultServiceManager()->getService(String16("media.extractor"));

        if (binder != 0) {
            sp<IMediaExtractorService> mediaExService(interface_cast<IMediaExtractorService>(binder));
            // source 为 CallbackDataSource，没有实现asIDataSource方法, 转到 父类 DataSource 
            sp<IMediaExtractor> ex = mediaExService->makeExtractor(source->asIDataSource(), mime);
            return ex;
        } else {
            ALOGE("extractor service not running");
            return NULL;
        }
    }
    return NULL;
}


//  @frameworks/av/media/libstagefright/DataSource.cpp
sp<IDataSource> DataSource::asIDataSource() {
    return RemoteDataSource::wrap(sp<DataSource>(this));
}

//  @frameworks/av/include/media/stagefright/RemoteDataSource.h
// Originally in MediaExtractor.cpp
class RemoteDataSource : public BnDataSource {
public:
    static sp<IDataSource> wrap(const sp<DataSource> &source) {
        if (source.get() == nullptr) {
            return nullptr;
        }
        if (source->getIDataSource().get() != nullptr) {//由于CallbackDataSource的getIDataSource不为空
            return source->getIDataSource();//这里注意是之前通过MediaExtractorService::makeIDataSource方法创建得到的IDataSource
        }
        return new RemoteDataSource(source);
    }

}

sp<IDataSource> CallbackDataSource::getIDataSource() const {
    return mIDataSource;
}





