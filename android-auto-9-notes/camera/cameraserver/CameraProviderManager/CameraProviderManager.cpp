

/**
 * @    /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/camera/libcameraservice/common/CameraProviderManager.h
 * 
 * A manager for all camera providers available on an Android device.
 *
 * Responsible for enumerating providers and the individual camera devices
 * they export, both at startup and as providers and devices are added/removed.
 *
 * Provides methods for requesting information about individual devices and for
 * opening them for active use.
 *  hidl::manager::V1_0::IServiceNotification   @   system/libhidl/transport/manager/1.0/IServiceNotification.hal
 */
class CameraProviderManager : virtual public hidl::manager::V1_0::IServiceNotification {

}


/***
 * 
 *  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/camera/libcameraservice/common/CameraProviderManager.cpp
 * 
 * CameraService::onFirstRef()
 * ---> CameraService::enumerateProviders()
 * --------> status_t initialize(wp<StatusListener> listener, ServiceInteractionProxy *proxy = &sHardwareServiceInteractionProxy);
 * 
 * */
status_t CameraProviderManager::initialize(wp<CameraProviderManager::StatusListener> listener,ServiceInteractionProxy* proxy) {
    std::lock_guard<std::mutex> lock(mInterfaceMutex);
    if (proxy == nullptr) {
        ALOGE("%s: No valid service interaction proxy provided", __FUNCTION__);
        return BAD_VALUE;
    }
    mListener = listener;
    mServiceProxy = proxy;

    // Registering will trigger notifications for all already-known providers
    bool success = mServiceProxy->registerForNotifications(/* instance name, empty means no filter */ "",this);
    if (!success) {
        ALOGE("%s: Unable to register with hardware service manager for notifications " "about camera providers", __FUNCTION__);
        return INVALID_OPERATION;
    }


    // See if there's a passthrough HAL, but let's not complain if there's not
    addProviderLocked(kLegacyProviderName, /*expected*/ false); //  const std::string kLegacyProviderName("legacy/0");
    addProviderLocked(kExternalProviderName, /*expected*/ false);// const std::string kExternalProviderName("external/0");

    return OK;
}


status_t CameraProviderManager::addProviderLocked(const std::string& newProvider, bool expected) {
    /**
     * 
    */
    for (const auto& providerInfo : mProviders) {
        if (providerInfo->mProviderName == newProvider) {
            ALOGW("%s: Camera provider HAL with name '%s' already registered", __FUNCTION__,newProvider.c_str());
            return ALREADY_EXISTS;
        }
    }

    sp<provider::V2_4::ICameraProvider> interface;
    interface = mServiceProxy->getService(newProvider);

    if (interface == nullptr) {
        if (expected) {
            ALOGE("%s: Camera provider HAL '%s' is not actually available", __FUNCTION__,newProvider.c_str());
            return BAD_VALUE;
        } else {
            return OK;
        }
    }

    sp<ProviderInfo> providerInfo = new ProviderInfo(newProvider, interface, this);
    status_t res = providerInfo->initialize();
    if (res != OK) {
        return res;
    }

    mProviders.push_back(providerInfo);

    return OK;
}
