//	@frameworks/av/drm/libdrmframework/DrmManagerClientImpl.cpp
DrmManagerClientImpl* DrmManagerClientImpl::create(int* pUniqueId, bool isNative) {
    sp<IDrmManagerService> service = getDrmManagerService();
    if (service != NULL) {
        *pUniqueId = getDrmManagerService()->addUniqueId(isNative);
        return new DrmManagerClientImpl();
    }
    return new NoOpDrmManagerClientImpl();//NoOpDrmManagerClientImpl 继承 DrmManagerClientImpl 空实现所有方法
}


const sp<IDrmManagerService>& DrmManagerClientImpl::getDrmManagerService() {
    Mutex::Autolock lock(sMutex);
    if (NULL == sDrmManagerService.get()) {
        char value[PROPERTY_VALUE_MAX];
        // property_get 返回值长度
        if (property_get("drm.service.enabled", value, NULL) == 0) {//	没有定义drm.service.enabled，返回
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