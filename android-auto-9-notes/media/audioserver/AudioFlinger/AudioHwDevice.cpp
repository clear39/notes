

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audioflinger/AudioHwDevice.cpp
/**
 * mFlags 只是用于标记 是否可以设置 setMasterVolume setMasterMute
*/
AudioHwDevice::AudioHwDevice(audio_module_handle_t handle,
                  const char *moduleName,
                  sp<DeviceHalInterface> hwDevice,
                  Flags flags)
        : mHandle(handle)
        , mModuleName(strdup(moduleName))
        , mHwDevice(hwDevice)
        , mFlags(flags) {



}


status_t AudioHwDevice::openOutputStream(
        AudioStreamOut **ppStreamOut,
        audio_io_handle_t handle,
        audio_devices_t devices,
        audio_output_flags_t flags,
        struct audio_config *config,
        const char *address)
{

    struct audio_config originalConfig = *config;
    AudioStreamOut *outputStream = new AudioStreamOut(this, flags);

    // Try to open the HAL first using the current format.
    ALOGV("openOutputStream(), try "
            " sampleRate %d, Format %#x, "
            "channelMask %#x",
            config->sample_rate,
            config->format,
            config->channel_mask);


    status_t status = outputStream->open(handle, devices, config, address);

    if (status != NO_ERROR) {
        delete outputStream;
        outputStream = NULL;

        // FIXME Look at any modification to the config.
        // The HAL might modify the config to suggest a wrapped format.
        // Log this so we can see what the HALs are doing.
        ALOGI("openOutputStream(), HAL returned"
            " sampleRate %d, Format %#x, "
            "channelMask %#x, status %d",
            config->sample_rate,
            config->format,
            config->channel_mask,
            status);

        // If the data is encoded then try again using wrapped PCM.
        bool wrapperNeeded = !audio_has_proportional_frames(originalConfig.format)
                && ((flags & AUDIO_OUTPUT_FLAG_DIRECT) != 0)
                && ((flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) == 0);

        if (wrapperNeeded) {
            if (SPDIFEncoder::isFormatSupported(originalConfig.format)) {
                outputStream = new SpdifStreamOut(this, flags, originalConfig.format);
                status = outputStream->open(handle, devices, &originalConfig, address);
                if (status != NO_ERROR) {
                    ALOGE("ERROR - openOutputStream(), SPDIF open returned %d",status);
                    delete outputStream;
                    outputStream = NULL;
                }
            } else {
                ALOGE("ERROR - openOutputStream(), SPDIFEncoder does not support format 0x%08x",originalConfig.format);
            }
        }
    }

    *ppStreamOut = outputStream;
    return status;
}

sp<DeviceHalInterface> AudioHwDevice::hwDevice() const { 
    return mHwDevice; 
}