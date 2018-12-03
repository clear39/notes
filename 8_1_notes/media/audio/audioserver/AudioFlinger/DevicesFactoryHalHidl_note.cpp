class DevicesFactoryHalHidl : public DevicesFactoryHalInterface
{}


//	@frameworks/av/media/libaudiohal/DevicesFactoryHalHidl.cpp
DevicesFactoryHalHidl::DevicesFactoryHalHidl() {
    mDevicesFactory = IDevicesFactory::getService();	//通过hwbinder和android.hardware.audio@2.0-service通讯
    if (mDevicesFactory != 0) {
        // It is assumed that DevicesFactory is owned by AudioFlinger
        // and thus have the same lifespan.
        mDevicesFactory->linkToDeath(HalDeathHandler::getInstance(), 0 /*cookie*/);
    } else {
        ALOGE("Failed to obtain IDevicesFactory service, terminating process.");
        exit(1);
    }
}



status_t DevicesFactoryHalHidl::openDevice(const char *name, sp<DeviceHalInterface> *device) {
    if (mDevicesFactory == 0) return NO_INIT;
    IDevicesFactory::Device hidlDevice;
    status_t status = nameFromHal(name, &hidlDevice);
    if (status != OK) return status;
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mDevicesFactory->openDevice(
            hidlDevice,
            [&](Result r, const sp<IDevice>& result) {
                retval = r;
                if (retval == Result::OK) {
                    *device = new DeviceHalHidl(result);
                }
            });
    if (ret.isOk()) {
        if (retval == Result::OK) return OK;
        else if (retval == Result::INVALID_ARGUMENTS) return BAD_VALUE;
        else return NO_INIT;
    }
    return FAILED_TRANSACTION;
}



// static
status_t DevicesFactoryHalHidl::nameFromHal(const char *name, IDevicesFactory::Device *device) {
    if (strcmp(name, AUDIO_HARDWARE_MODULE_ID_PRIMARY) == 0) {
        *device = IDevicesFactory::Device::PRIMARY;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_A2DP) == 0) {
        *device = IDevicesFactory::Device::A2DP;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_USB) == 0) {
        *device = IDevicesFactory::Device::USB;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX) == 0) {
        *device = IDevicesFactory::Device::R_SUBMIX;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_STUB) == 0) {
        *device = IDevicesFactory::Device::STUB;
        return OK;
    }
    ALOGE("Invalid device name %s", name);
    return BAD_VALUE;
}
