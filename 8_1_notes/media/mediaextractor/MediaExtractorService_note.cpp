//	@frameworks/av/services/mediaextractor/MediaExtractorService.cpp
sp<IMediaExtractor> MediaExtractorService::makeExtractor(const sp<IDataSource> &remoteSource, const char *mime) {
    ALOGV("@@@ MediaExtractorService::makeExtractor for %s", mime);

    sp<DataSource> localSource = DataSource::CreateFromIDataSource(remoteSource);

    sp<IMediaExtractor> ret = MediaExtractor::CreateFromService(localSource, mime);

    ALOGV("extractor service created %p (%s)", ret.get(), ret == NULL ? "" : ret->name());

    if (ret != NULL) {
        registerMediaExtractor(ret, localSource, mime);
    }

    return ret;
}






//  @frameworks/av/media/libmedia/IMediaExtractor.cpp
void registerMediaExtractor(const sp<IMediaExtractor> &extractor,const sp<DataSource> &source,const char *mime) {
    ExtractorInstance ex;
    ex.mime = mime == NULL ? "NULL" : mime;
    ex.name = extractor->name();
    ex.sourceDescription = source->toString();
    ex.owner = IPCThreadState::self()->getCallingPid();
    ex.extractor = extractor;

    {
        Mutex::Autolock lock(sExtractorsLock);
        if (sExtractors.size() > 10) {
            sExtractors.resize(10);
        }
        sExtractors.push_front(ex);//   static Vector<ExtractorInstance> sExtractors;
    }
}















sp<IDataSource> MediaExtractorService::makeIDataSource(int fd, int64_t offset, int64_t length)
{
    sp<DataSource> source = DataSource::CreateFromFd(fd, offset, length);
    return source.get() != nullptr ? source->asIDataSource() : nullptr;
}

