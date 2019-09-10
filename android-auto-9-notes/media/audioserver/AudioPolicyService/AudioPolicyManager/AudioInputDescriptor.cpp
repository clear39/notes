class AudioInputDescriptor: public AudioPortConfig, public AudioSessionInfoProvider{

}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioInputDescriptor.cpp
AudioInputDescriptor::AudioInputDescriptor(const sp<IOProfile>& profile,AudioPolicyClientInterface *clientInterface)
    : mIoHandle(0),
      mDevice(AUDIO_DEVICE_NONE), mPolicyMix(NULL),
      mProfile(profile), mPatchHandle(AUDIO_PATCH_HANDLE_NONE), mId(0),
      mClientInterface(clientInterface)
{
    if (profile != NULL) {
        profile->pickAudioProfile(mSamplingRate, mChannelMask, mFormat);
        if (profile->mGains.size() > 0) {
            profile->mGains[0]->getDefaultConfig(&mGain);
        }
    }
}


status_t AudioInputDescriptor::open(const audio_config_t *config,
                                       audio_devices_t device,
                                       const String8& address,
                                       audio_source_t source,
                                       audio_input_flags_t flags,
                                       audio_io_handle_t *input)
{
    audio_config_t lConfig;
    if (config == nullptr) {
        lConfig = AUDIO_CONFIG_INITIALIZER;
        lConfig.sample_rate = mSamplingRate;
        lConfig.channel_mask = mChannelMask;
        lConfig.format = mFormat;
    } else {
        lConfig = *config;
    }

    mDevice = device;

    ALOGV("opening input for device %08x address %s profile %p name %s", mDevice, address.string(), mProfile.get(), mProfile->getName().string());

    status_t status = mClientInterface->openInput(mProfile->getModuleHandle(),
                                                  input,
                                                  &lConfig,
                                                  &mDevice,
                                                  address,
                                                  source,
                                                  flags);
    LOG_ALWAYS_FATAL_IF(mDevice != device, "%s openInput returned device %08x when given device %08x", __FUNCTION__, mDevice, device);

    if (status == NO_ERROR) {
        LOG_ALWAYS_FATAL_IF(*input == AUDIO_IO_HANDLE_NONE, "%s openInput returned input handle %d for device %08x", __FUNCTION__, *input, device);
        mSamplingRate = lConfig.sample_rate;
        mChannelMask = lConfig.channel_mask;
        mFormat = lConfig.format;
        mId = AudioPort::getNextUniqueId();
        mIoHandle = *input;
        mProfile->curOpenCount++;
    }

    return status;
}

