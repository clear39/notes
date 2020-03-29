
/**
 * @    /home/xqli/work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/camera/provider/2.4/default/service.cpp
*/
int main()
{
    ALOGI("Camera provider Service is starting.");
    // The camera HAL may communicate to other vendor components via
    // /dev/vndbinder
    android::ProcessState::initWithDriver("/dev/vndbinder");
    /**
     * 这里defaultPassthroughServiceImplementation内部触发 HIDL_FETCH_ICameraProvider调用
    */
    return defaultPassthroughServiceImplementation<ICameraProvider>("legacy/0", /*maxThreads*/ 6);
}


//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/camera/provider/2.4/default/CameraProvider.cpp
ICameraProvider* HIDL_FETCH_ICameraProvider(const char* name) {
    if (strcmp(name, kLegacyProviderName) == 0) {// const char *kLegacyProviderName = "legacy/0";
        CameraProvider* provider = new CameraProvider();
        if (provider == nullptr) {
            ALOGE("%s: cannot allocate camera provider!", __FUNCTION__);
            return nullptr;
        }
        if (provider->isInitFailed()) {
            ALOGE("%s: camera provider init failed!", __FUNCTION__);
            delete provider;
            return nullptr;
        }
        return provider;
    } else if (strcmp(name, kExternalProviderName) == 0) {//const char *kExternalProviderName = "external/0";
        ExternalCameraProvider* provider = new ExternalCameraProvider();
        return provider;
    }
    ALOGE("%s: unknown instance name: %s", __FUNCTION__, name);
    return nullptr;
}