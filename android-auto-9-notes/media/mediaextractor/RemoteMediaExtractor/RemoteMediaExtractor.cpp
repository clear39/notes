

//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/RemoteMediaExtractor.cpp

// static
sp<IMediaExtractor> RemoteMediaExtractor::wrap( MediaExtractor *extractor, const sp<DataSource> &source, const sp<RefBase> &plugin) {
    if (extractor == nullptr) {
        return nullptr;
    }
    return new RemoteMediaExtractor(extractor, source, plugin);
}


RemoteMediaExtractor::RemoteMediaExtractor(MediaExtractor *extractor, const sp<DataSource> &source, const sp<RefBase> &plugin)
    :mExtractor(extractor),
     mSource(source),
     mExtractorPlugin(plugin) {

    mAnalyticsItem = nullptr;
    if (MEDIA_LOG) {
        mAnalyticsItem = new MediaAnalyticsItem(kKeyExtractor);

        // track the container format (mpeg, aac, wvm, etc)
        size_t ntracks = extractor->countTracks();
        mAnalyticsItem->setCString(kExtractorFormat, extractor->name());
        // tracks (size_t)
        mAnalyticsItem->setInt32(kExtractorTracks, ntracks);
        // metadata
        MetaDataBase pMetaData;
        if (extractor->getMetaData(pMetaData) == OK) {
            String8 xx = pMetaData.toString();
            // 'titl' -- but this verges into PII
            // 'mime'
            const char *mime = nullptr;
            if (pMetaData.findCString(kKeyMIMEType, &mime)) {
                mAnalyticsItem->setCString(kExtractorMime,  mime);
            }
            // what else is interesting and not already available?
        }
    }
}