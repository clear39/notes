class AudioOutputDescriptor: public AudioPortConfig{

}

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioOutputDescriptor.cpp
/***
 * port 为 mixport 标签
 * */
AudioOutputDescriptor::AudioOutputDescriptor(const sp<AudioPort>& port,AudioPolicyClientInterface *clientInterface)
    : mPort(port), mDevice(AUDIO_DEVICE_NONE),
      mClientInterface(clientInterface), mPatchHandle(AUDIO_PATCH_HANDLE_NONE), mId(0)
{
    // clear usage count for all stream types
    for (int i = 0; i < AUDIO_STREAM_CNT; i++) {
        mRefCount[i] = 0;
        mCurVolume[i] = -1.0;
        mMuteCount[i] = 0;
        mStopTime[i] = 0;
    }
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        mStrategyMutedByDevice[i] = false;
    }
    if (mPort.get() != nullptr) {
        mPort->pickAudioProfile(mSamplingRate, mChannelMask, mFormat);
        if (mPort->mGains.size() > 0) {
            mPort->mGains[0]->getDefaultConfig(&mGain);
        }
    }
}

SwAudioOutputDescriptor::SwAudioOutputDescriptor(const sp<IOProfile>& profile,AudioPolicyClientInterface *clientInterface)
    : AudioOutputDescriptor(profile, clientInterface),
    mProfile(profile), mIoHandle(AUDIO_IO_HANDLE_NONE), mLatency(0),
    mFlags((audio_output_flags_t)0), mPolicyMix(NULL),
    mOutput1(0), mOutput2(0), mDirectOpenCount(0),
    mDirectClientSession(AUDIO_SESSION_NONE), mGlobalRefCount(0)
{
    if (profile != NULL) {
        mFlags = (audio_output_flags_t)profile->getFlags();
    }
}


/**
 * AudioPolicyManager::initialize() 中调用 config 为 nullptr
 * 
 * status_t status = outputDesc->open(nullptr, profileType, address,AUDIO_STREAM_DEFAULT, AUDIO_OUTPUT_FLAG_NONE, &output);
 * 
 * 
 * device 为 system/media/audio/include/system/audio-base.h:319:    AUDIO_DEVICE_OUT_BUS                       = 0x1000000u
 * 
*/
status_t SwAudioOutputDescriptor::open(const audio_config_t *config,
                                       audio_devices_t device, // profileType
                                       const String8& address,
                                       audio_stream_type_t stream, // AUDIO_STREAM_DEFAULT
                                       audio_output_flags_t flags, //   AUDIO_OUTPUT_FLAG_NONE
                                       audio_io_handle_t *output)
{
    audio_config_t lConfig;
    if (config == nullptr) {
        // 执行这里
        lConfig = AUDIO_CONFIG_INITIALIZER;
        lConfig.sample_rate = mSamplingRate;
        lConfig.channel_mask = mChannelMask;
        lConfig.format = mFormat;
    } else {
        lConfig = *config;
    }

    mDevice = device;
    // if the selected profile is offloaded and no offload info was specified,
    // create a default one
    if ((mProfile->getFlags() & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) && lConfig.offload_info.format == AUDIO_FORMAT_DEFAULT) {
        //
        flags = (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD);

        lConfig.offload_info = AUDIO_INFO_INITIALIZER;
        lConfig.offload_info.sample_rate = lConfig.sample_rate;
        lConfig.offload_info.channel_mask = lConfig.channel_mask;
        lConfig.offload_info.format = lConfig.format;
        lConfig.offload_info.stream_type = stream; //
        lConfig.offload_info.duration_us = -1;
        lConfig.offload_info.has_video = true; // conservative
        lConfig.offload_info.is_streaming = true; // likely
    }

    mFlags = (audio_output_flags_t)(mFlags | flags);

    ALOGV("opening output for device %08x address %s profile %p name %s", mDevice, address.string(), mProfile.get(), mProfile->getName().string());

    /**
     * 这里 mProfile->getModuleHandle() 是由 AudioFlinger::loadHwModule 调用加载得到的 audio_module_handle_t
     * */
    status_t status = mClientInterface->openOutput(mProfile->getModuleHandle(),
                                                   output,
                                                   &lConfig,
                                                   &mDevice,
                                                   address,
                                                   &mLatency,
                                                   mFlags);
    LOG_ALWAYS_FATAL_IF(mDevice != device,"%s openOutput returned device %08x when given device %08x",__FUNCTION__, mDevice, device);

    if (status == NO_ERROR) {
        LOG_ALWAYS_FATAL_IF(*output == AUDIO_IO_HANDLE_NONE,"%s openOutput returned output handle %d for device %08x",__FUNCTION__, *output, device);
        mSamplingRate = lConfig.sample_rate;
        mChannelMask = lConfig.channel_mask;
        mFormat = lConfig.format;
        mId = AudioPort::getNextUniqueId();
        mIoHandle = *output;
        mProfile->curOpenCount++;
    }

    return status;
}


bool AudioOutputDescriptor::setVolume(float volume, audio_stream_type_t stream,audio_devices_t device __unused, uint32_t delayMs,bool force)
{
    // We actually change the volume if:
    // - the float value returned by computeVolume() changed
    // - the force flag is set
    if (volume != mCurVolume[stream] || force) {
        ALOGV("setVolume() for stream %d, volume %f, delay %d", stream, volume, delayMs);
        mCurVolume[stream] = volume;
        return true;
    }
    return false;
}