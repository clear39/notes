


sp<IMediaExtractor> MediaExtractorService::makeExtractor(const sp<IDataSource> &remoteSource, const char *mime) {
    ALOGV("@@@ MediaExtractorService::makeExtractor for %s", mime);

/**
 * @    frameworks/av/media/libstagefright/InterfaceUtils.cpp
*/
    sp<DataSource> localSource = CreateDataSourceFromIDataSource(remoteSource);

    sp<IMediaExtractor> extractor = MediaExtractorFactory::CreateFromService(localSource, mime);

    ALOGV("extractor service created %p (%s)", extractor.get(), extractor == nullptr ? "" : extractor->name());

    if (extractor != nullptr) {
        registerMediaExtractor(extractor, localSource, mime);
        return extractor;
    }
    return nullptr;
}


sp<DataSource> CreateDataSourceFromIDataSource(const sp<IDataSource> &source) {
    if (source == nullptr) {                                                                                                                                                                                   
             return nullptr;
     }
     return new TinyCacheSource(new CallbackDataSource(source));
}



sp<IDataSource> MediaExtractorService::makeIDataSource(int fd, int64_t offset, int64_t length)
{
    sp<DataSource> source = DataSourceFactory::CreateFromFd(fd, offset, length);
    /**
     *  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/InterfaceUtils.cpp
    */
    return CreateIDataSourceFromDataSource(source);
}

/**
     *  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/InterfaceUtils.cpp
    */
sp<IDataSource> CreateIDataSourceFromDataSource(const sp<DataSource> &source) {
    if (source == nullptr) {
        return nullptr;
    }
    return RemoteDataSource::wrap(source);
}