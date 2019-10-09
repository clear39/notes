//  @   hardware/interfaces/audio/4.0/IDevicesFactory.hal
interface IDevicesFactory {

    /**
     * Opens an audio device. To close the device, it is necessary to release
     * references to the returned device object.
     *
     * @param device device name.
     * @return retval operation completion status. Returns INVALID_ARGUMENTS
     *         if there is no corresponding hardware module found,
     *         NOT_INITIALIZED if an error occured while opening the hardware
     *         module.
     * @return result the interface for the created device.
     */
    openDevice(string device) generates (Result retval, IDevice result);

    /**
     * Opens the Primary audio device that must be present.
     * This function is not optional and must return successfully the primary device.
     *
     * This device must have the name "primary".
     *
     * The telephony stack uses this device to control the audio during a voice call.
     *
     * @return retval operation completion status. Must be SUCCESS.
     *         For debuging, return INVALID_ARGUMENTS if there is no corresponding
     *         hardware module found, NOT_INITIALIZED if an error occurred
     *         while opening the hardware module.
     * @return result the interface for the created device.
     */
    openPrimaryDevice() generates (Result retval, IPrimaryDevice result);
};



//  @   hardware/interfaces/audio/core/all-versions/default/include/core/all-versions/default/DevicesFactory.impl.h
/**
 * 入口
*/
IDevicesFactory* HIDL_FETCH_IDevicesFactory(const char* /* name */) {

    /**
     * 
     * @    hardware/interfaces/audio/core/all-versions/default/include/core/all-versions/default/DevicesFactory.h
    */
    return new DevicesFactory();
}


Return<void> DevicesFactory::openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) {
    return openDevice<PrimaryDevice>(AUDIO_HARDWARE_MODULE_ID_PRIMARY, _hidl_cb);
}

Return<void> DevicesFactory::openDevice(const hidl_string& moduleName, openDevice_cb _hidl_cb) {
    if (moduleName == AUDIO_HARDWARE_MODULE_ID_PRIMARY) {
        return openDevice<PrimaryDevice>(moduleName.c_str(), _hidl_cb);
    }
    /**
     * openDevice<implementation::Device>(moduleName, _hidl_cb)
    */
    return openDevice(moduleName.c_str(), _hidl_cb);
}

template <class DeviceShim, class Callback>
Return<void> DevicesFactory::openDevice(const char* moduleName, Callback _hidl_cb) {
    audio_hw_device_t* halDevice;
    Result retval(Result::INVALID_ARGUMENTS);
    sp<DeviceShim> result;
    int halStatus = loadAudioInterface(moduleName, &halDevice);
    if (halStatus == OK) {
        result = new DeviceShim(halDevice);  // PrimaryDevice 或者 Device
        retval = Result::OK;
    } else if (halStatus == -EINVAL) {
        retval = Result::NOT_INITIALIZED;
    }
    _hidl_cb(retval, result);
    return Void();
}



// static
int DevicesFactory::loadAudioInterface(const char* if_name, audio_hw_device_t** dev) {
    const hw_module_t* mod;
    int rc;

    rc = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, if_name, &mod);
    if (rc) {
        ALOGE("%s couldn't load audio hw module %s.%s (%s)", __func__, AUDIO_HARDWARE_MODULE_ID,if_name, strerror(-rc));
        goto out;
    }
    rc = audio_hw_device_open(mod, dev);
    if (rc) {
        ALOGE("%s couldn't open audio hw device in %s.%s (%s)", __func__, AUDIO_HARDWARE_MODULE_ID,if_name, strerror(-rc));
        goto out;
    }
    if ((*dev)->common.version < AUDIO_DEVICE_API_VERSION_MIN) {
        ALOGE("%s wrong audio hw device version %04x", __func__, (*dev)->common.version);
        rc = -EINVAL;
        audio_hw_device_close(*dev);
        goto out;
    }
    return OK;

out:
    *dev = NULL;
    return rc;
}

