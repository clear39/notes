class HwModule : public RefBase{

}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/HwModule.cpp

HwModule::HwModule(const char *name, uint32_t halVersionMajor, uint32_t halVersionMinor)
    : mName(String8(name)),mHandle(AUDIO_MODULE_HANDLE_NONE)
{
    setHalVersion(halVersionMajor, halVersionMinor);
}

void HwModule::setHalVersion(uint32_t major, uint32_t minor) {
    mHalVersion = (major << 8) | (minor & 0xff);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 在配置文件解析中调用
 * 每个成员对应标签 mixPort
*/
void HwModule::setProfiles(const IOProfileCollection &profiles)
{
    for (size_t i = 0; i < profiles.size(); i++) {
        addProfile(profiles[i]);
    }
}

status_t HwModule::addProfile(const sp<IOProfile> &profile)
{
    switch (profile->getRole()) {
    case AUDIO_PORT_ROLE_SOURCE:  // output
        return addOutputProfile(profile);
    case AUDIO_PORT_ROLE_SINK: // input
        return addInputProfile(profile);
    case AUDIO_PORT_ROLE_NONE:
        return BAD_VALUE;
    }
    return BAD_VALUE;
}

status_t HwModule::addOutputProfile(const String8& name, const audio_config_t *config,audio_devices_t device, const String8& address)
{
    sp<IOProfile> profile = new OutputProfile(name);

    profile->addAudioProfile(new AudioProfile(config->format, config->channel_mask,config->sample_rate));

    sp<DeviceDescriptor> devDesc = new DeviceDescriptor(device);
    devDesc->mAddress = address;
    profile->addSupportedDevice(devDesc);

    return addOutputProfile(profile);
}

status_t HwModule::addOutputProfile(const sp<IOProfile> &profile)
{
    profile->attach(this);
    mOutputProfiles.add(profile);
    mPorts.add(profile);
    return NO_ERROR;
}

status_t HwModule::addInputProfile(const sp<IOProfile> &profile)
{
    profile->attach(this);
    mInputProfiles.add(profile);
    mPorts.add(profile);
    return NO_ERROR;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * 在 AudioPolicyManager::initialize()  中通过 AudioFlinger的loadHwModule 加载之后，进行设置
 */ 
void HwModule::setHandle(audio_module_handle_t handle) {
    ALOGW_IF(mHandle != AUDIO_MODULE_HANDLE_NONE,"HwModule handle is changing from %d to %d", mHandle, handle);
    mHandle = handle;
}


const OutputProfileCollection &getOutputProfiles() const {
     return mOutputProfiles; 
}


