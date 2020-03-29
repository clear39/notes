
// static
sp<IMediaExtractor> MediaExtractorFactory::Create(const sp<DataSource> &source, const char *mime) {
    ALOGV("MediaExtractorFactory::Create %s", mime);

    if (!property_get_bool("media.stagefright.extractremote", true)) {
        // local extractor
        ALOGW("creating media extractor in calling process");
        /**
         * 本地解析
        */
        return CreateFromService(source, mime);
    } else {
        // remote extractor
        ALOGV("get service manager");
        sp<IBinder> binder = defaultServiceManager()->getService(String16("media.extractor"));

        if (binder != 0) {
            sp<IMediaExtractorService> mediaExService(interface_cast<IMediaExtractorService>(binder));
            /**
             * @    /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/InterfaceUtils.cpp
            */
            sp<IMediaExtractor> ex = mediaExService->makeExtractor(CreateIDataSourceFromDataSource(source), mime);
            return ex;
        } else {
            ALOGE("extractor service not running");
            return NULL;
        }
    }
    return NULL;
}


//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libstagefright/InterfaceUtils.cpp
sp<IDataSource> CreateIDataSourceFromDataSource(const sp<DataSource> &source) {
    if (source == nullptr) {
        return nullptr;
    }
    return RemoteDataSource::wrap(source);
}


sp<IMediaExtractor> MediaExtractorFactory::CreateFromService(const sp<DataSource> &source, const char *mime) {

    ALOGV("MediaExtractorFactory::CreateFromService %s", mime);

/**
 * 
*/
    UpdateExtractors(nullptr);

    // initialize source decryption if needed
    source->DrmInitialization(nullptr /* mime */);

    void *meta = nullptr;
    MediaExtractor::CreatorFunc creator = NULL;
    MediaExtractor::FreeMetaFunc freeMeta = nullptr;
    float confidence;
    sp<ExtractorPlugin> plugin;
    creator = sniff(source.get(), &confidence, &meta, &freeMeta, plugin);
    if (!creator) {
        ALOGV("FAILED to autodetect media content.");
        return NULL;
    }

    MediaExtractor *ret = creator(source.get(), meta);
    if (meta != nullptr && freeMeta != nullptr) {
        freeMeta(meta);
    }

    ALOGV("Created an extractor '%s' with confidence %.2f",ret != nullptr ? ret->name() : "<null>", confidence);

    return CreateIMediaExtractorFromMediaExtractor(ret, source, plugin);
}


// static
void MediaExtractorFactory::UpdateExtractors(const char *newUpdateApkPath) {
    Mutex::Autolock autoLock(gPluginMutex);
    if (newUpdateApkPath != nullptr) {
        gPluginsRegistered = false;
    }
    if (gPluginsRegistered) {
        return;
    }

    std::shared_ptr<List<sp<ExtractorPlugin>>> newList(new List<sp<ExtractorPlugin>>());

    RegisterExtractorsInSystem("/system/lib"
#ifdef __LP64__
            "64"
#endif
            "/extractors", *newList);

    int value;
    value = property_get_int32("media.fsl_codec.flag", 0);
    if(value & 0x01)
        RegisterExtractorsInSystem("/vendor/lib"
#ifdef __LP64__
            "64"
#endif
            "/extractors", *newList);

    if (newUpdateApkPath != nullptr) {
        RegisterExtractorsInApk(newUpdateApkPath, *newList);
    }

    gPlugins = newList;
    gPluginsRegistered = true;
}



// static
MediaExtractor::CreatorFunc MediaExtractorFactory::sniff(DataSourceBase *source, float *confidence, void **meta,MediaExtractor::FreeMetaFunc *freeMeta, sp<ExtractorPlugin> &plugin) {
    *confidence = 0.0f;
    *meta = nullptr;

    std::shared_ptr<List<sp<ExtractorPlugin>>> plugins;
    {
        Mutex::Autolock autoLock(gPluginMutex);
        if (!gPluginsRegistered) {
            return NULL;
        }
        plugins = gPlugins;
    }

    MediaExtractor::CreatorFunc curCreator = NULL;
    MediaExtractor::CreatorFunc bestCreator = NULL;
    for (auto it = plugins->begin(); it != plugins->end(); ++it) {
        float newConfidence;
        void *newMeta = nullptr;
        MediaExtractor::FreeMetaFunc newFreeMeta = nullptr;
        if ((curCreator = (*it)->def.sniff(source, &newConfidence, &newMeta, &newFreeMeta))) {
            if (newConfidence > *confidence) {
                *confidence = newConfidence;
                if (*meta != nullptr && *freeMeta != nullptr) {
                    (*freeMeta)(*meta);
                }
                *meta = newMeta;
                *freeMeta = newFreeMeta;
                plugin = *it;
                bestCreator = curCreator;
            } else {
                if (newMeta != nullptr && newFreeMeta != nullptr) {
                    newFreeMeta(newMeta);
                }
            }
        }
    }

    return bestCreator;
}