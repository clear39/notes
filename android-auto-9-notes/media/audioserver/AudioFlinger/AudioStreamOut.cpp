//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audioflinger/AudioStreamOut.cpp
AudioStreamOut::AudioStreamOut(AudioHwDevice *dev, audio_output_flags_t flags)
        : audioHwDev(dev)
        , stream(NULL)
        , flags(flags)
        , mFramesWritten(0)
        , mFramesWrittenAtStandby(0)
        , mRenderPosition(0)
        , mRateMultiplier(1)
        , mHalFormatHasProportionalFrames(false)
        , mHalFrameSize(0)
{

}

sp<DeviceHalInterface> AudioStreamOut::hwDev() const
{
    // DeviceHalInterface
    return audioHwDev->hwDevice();
}

status_t AudioStreamOut::open(
        audio_io_handle_t handle,
        audio_devices_t devices,
        struct audio_config *config,
        const char *address)
{
    sp<StreamOutHalInterface> outStream;

    audio_output_flags_t customFlags = (config->format == AUDIO_FORMAT_IEC61937) ? (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO) : flags;

    int status = hwDev()->openOutputStream(
            handle,
            devices,
            customFlags,
            config,
            address,
            &outStream);


    ALOGV("AudioStreamOut::open(), HAL returned "
            " stream %p, sampleRate %d, Format %#x, "
            "channelMask %#x, status %d",
            outStream.get(),
            config->sample_rate,
            config->format,
            config->channel_mask,
            status);

    // Some HALs may not recognize AUDIO_FORMAT_IEC61937. But if we declare
    // it as PCM then it will probably work.
    if (status != NO_ERROR && config->format == AUDIO_FORMAT_IEC61937) {
        struct audio_config customConfig = *config;
        customConfig.format = AUDIO_FORMAT_PCM_16_BIT;

        status = hwDev()->openOutputStream(
                handle,
                devices,
                customFlags,
                &customConfig,
                address,
                &outStream);
                
        ALOGV("AudioStreamOut::open(), treat IEC61937 as PCM, status = %d", status);
    }

    if (status == NO_ERROR) {
        stream = outStream;
        mHalFormatHasProportionalFrames = audio_has_proportional_frames(config->format);
        status = stream->getFrameSize(&mHalFrameSize);
    }

    return status;
}