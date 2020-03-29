
sp<IMediaExtractor> MediaExtractorFactory::CreateFromService(const sp<DataSource> &source, const char *mime) {

    ALOGV("MediaExtractorFactory::CreateFromService %s", mime);

    UpdateExtractors(nullptr);

    // initialize source decryption if needed
    source->DrmInitialization(nullptr /* mime */);

    void *meta = nullptr;
    MediaExtractor::CreatorFunc creator = NULL;
    MediaExtractor::FreeMetaFunc freeMeta = nullptr;
    float confidence;
    sp<ExtractorPlugin> plugin;
    /***
     * 
    */
    creator = sniff(source.get(), &confidence, &meta, &freeMeta, plugin);
    if (!creator) {
        ALOGV("FAILED to autodetect media content.");
        return NULL;
    }

/***
 * 
*/
    MediaExtractor *ret = creator(source.get(), meta);
    if (meta != nullptr && freeMeta != nullptr) {
        freeMeta(meta);
    }

    ALOGV("Created an extractor '%s' with confidence %.2f",ret != nullptr ? ret->name() : "<null>", confidence);
    /**
     * @    /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/InterfaceUtils.cpp
    */
    return CreateIMediaExtractorFromMediaExtractor(ret, source, plugin);
}


sp<IMediaExtractor> CreateIMediaExtractorFromMediaExtractor(
        MediaExtractor *extractor,
        const sp<DataSource> &source,
        const sp<RefBase> &plugin) {
    if (extractor == nullptr) {
        return nullptr;
    }
    return RemoteMediaExtractor::wrap(extractor, source, plugin);
}













