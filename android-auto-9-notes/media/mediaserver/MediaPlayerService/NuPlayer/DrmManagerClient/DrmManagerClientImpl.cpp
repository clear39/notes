

DrmManagerClientImpl* DrmManagerClientImpl::create( int* pUniqueId, bool isNative) {
    sp<IDrmManagerService> service = getDrmManagerService();
    if (service != NULL) {
        *pUniqueId = getDrmManagerService()->addUniqueId(isNative);
        return new DrmManagerClientImpl();
    }
    return new NoOpDrmManagerClientImpl();
}


const sp<IDrmManagerService>& DrmManagerClientImpl::getDrmManagerService() {
    Mutex::Autolock lock(sMutex);
    if (NULL == sDrmManagerService.get()) {
        char value[PROPERTY_VALUE_MAX];
        if (property_get("drm.service.enabled", value, NULL) == 0) {
            // Drm is undefined for this device
            return sDrmManagerService;
        }

        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder;
        do {
            binder = sm->getService(String16("drm.drmManager"));
            if (binder != 0) {
                break;
            }
            ALOGW("DrmManagerService not published, waiting...");
            struct timespec reqt;
            reqt.tv_sec  = 0;
            reqt.tv_nsec = 500000000; //0.5 sec
            nanosleep(&reqt, NULL);
        } while (true);
        if (NULL == sDeathNotifier.get()) {
            sDeathNotifier = new DeathNotifier();
        }
        binder->linkToDeath(sDeathNotifier);
        sDrmManagerService = interface_cast<IDrmManagerService>(binder);
    }
    return sDrmManagerService;
}










sp<DecryptHandle> DrmManagerClientImpl::openDecryptSession(
            int uniqueId, int fd, off64_t offset,
            off64_t length, const char* mime) {

    return getDrmManagerService()->openDecryptSession(uniqueId, fd, offset, length, mime);
}
