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


