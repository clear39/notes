//	@hardware/interfaces/audio/2.0/IDevicesFactory.hal
interface IDevicesFactory {
    typedef android.hardware.audio@2.0::Result Result;

    enum Device : int32_t {
        PRIMARY,
        A2DP,
        USB,
        R_SUBMIX,
        STUB
    };

    /**
     * Opens an audio device. To close the device, it is necessary to release
     * references to the returned device object.
     *
     * @param device device type.
     * @return retval operation completion status. Returns INVALID_ARGUMENTS
     *         if there is no corresponding hardware module found,
     *         NOT_INITIALIZED if an error occured while opening the hardware
     *         module.
     * @return result the interface for the created device.
     */
    openDevice(Device device) generates (Result retval, IDevice result);
};





//	@hardware/interfaces/audio/2.0/default/DevicesFactory.h
struct DevicesFactory : public IDevicesFactory {
    // Methods from ::android::hardware::audio::V2_0::IDevicesFactory follow.
    Return<void> openDevice(IDevicesFactory::Device device, openDevice_cb _hidl_cb)  override;

  private:
    static const char* deviceToString(IDevicesFactory::Device device);
    static int loadAudioInterface(const char *if_name, audio_hw_device_t **dev);

};



// Methods from ::android::hardware::audio::V2_0::IDevicesFactory follow.
Return<void> DevicesFactory::openDevice(IDevicesFactory::Device device, openDevice_cb _hidl_cb)  {
    audio_hw_device_t *halDevice;
    Result retval(Result::INVALID_ARGUMENTS);
    sp<IDevice> result;
    const char* moduleName = deviceToString(device);
    if (moduleName != nullptr) {
        int halStatus = loadAudioInterface(moduleName, &halDevice);
        if (halStatus == OK) {
            if (device == IDevicesFactory::Device::PRIMARY) {
                result = new PrimaryDevice(halDevice);
            } else {
                result = new ::android::hardware::audio::V2_0::implementation::Device(halDevice);
            }
            retval = Result::OK;
        } else if (halStatus == -EINVAL) {
            retval = Result::NOT_INITIALIZED;
        }
    }
    _hidl_cb(retval, result);
    return Void();
}



// static
int DevicesFactory::loadAudioInterface(const char *if_name, audio_hw_device_t **dev)
{
    const hw_module_t *mod;
    int rc;

    rc = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, if_name, &mod);
    if (rc) {
        ALOGE("%s couldn't load audio hw module %s.%s (%s)", __func__,AUDIO_HARDWARE_MODULE_ID, if_name, strerror(-rc));
        goto out;
    }
    rc = audio_hw_device_open(mod, dev);
    if (rc) {
        ALOGE("%s couldn't open audio hw device in %s.%s (%s)", __func__,AUDIO_HARDWARE_MODULE_ID, if_name, strerror(-rc));
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