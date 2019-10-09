

/***
 * @    hardware/interfaces/audio/4.0/IDevice.hal
 * */


//  @   hardware/interfaces/audio/core/all-versions/default/include/core/all-versions/default/Device.impl.h
Device::Device(audio_hw_device_t* device) : mDevice(device) {

}



Return<void> Device::openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                     const AudioConfig& config, AudioInputFlagBitfield flags,
                                     AudioSource source, openInputStream_cb _hidl_cb) {
    audio_config_t halConfig;
    HidlUtils::audioConfigToHal(config, &halConfig);
    audio_stream_in_t* halStream;
    ALOGV(
        "open_input_stream handle: %d devices: %x flags: %#x "
        "srate: %d format %#x channels %x address %s source %d",
        ioHandle, static_cast<audio_devices_t>(device.device),
        static_cast<audio_input_flags_t>(flags), halConfig.sample_rate, halConfig.format,
        halConfig.channel_mask, deviceAddressToHal(device).c_str(),
        static_cast<audio_source_t>(source));


    int status = mDevice->open_input_stream(
        mDevice, ioHandle, static_cast<audio_devices_t>(device.device), &halConfig, &halStream,
        static_cast<audio_input_flags_t>(flags), deviceAddressToHal(device).c_str(),
        static_cast<audio_source_t>(source));

    ALOGV("open_input_stream status %d stream %p", status, halStream);
    
    sp<IStreamIn> streamIn;
    if (status == OK) {
        streamIn = new StreamIn(this, halStream);
    }
    AudioConfig suggestedConfig;
    HidlUtils::audioConfigFromHal(halConfig, &suggestedConfig);
    _hidl_cb(analyzeStatus("open_input_stream", status), streamIn, suggestedConfig);
    return Void();
}

