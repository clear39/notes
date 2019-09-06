/**
 * 注意这里继承接口 ISoundTriggerHwService
 */ 
class SoundTriggerHwService :
    public BinderService<SoundTriggerHwService>,
    public BnSoundTriggerHwService
{


}


/**
 * trigger (触发)
 * dumpsys media.sound_trigger_hw
 * 
 * 
*/
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/soundtrigger/SoundTriggerHwService.cpp
SoundTriggerHwService::SoundTriggerHwService()
    : BnSoundTriggerHwService(),
      mNextUniqueId(1),
      mMemoryDealer(new MemoryDealer(1024 * 1024, "SoundTriggerHwService")),
      mCaptureState(false)
{

}

void SoundTriggerHwService::onFirstRef()
{
    int rc;

/**
 *  frameworks/av/services/soundtrigger/SoundTriggerHalInterface.h:30:        static sp<SoundTriggerHalInterface> connectModule(const char *moduleName);
    frameworks/av/services/soundtrigger/SoundTriggerHwService.cpp:59:            SoundTriggerHalInterface::connectModule(HW_MODULE_PREFIX);
    frameworks/av/services/soundtrigger/SoundTriggerHalHidl.cpp:103:sp<SoundTriggerHalInterface> SoundTriggerHalInterface::connectModule(const char *moduleName)
    frameworks/av/services/soundtrigger/SoundTriggerHalLegacy.cpp:23:sp<SoundTriggerHalInterface> SoundTriggerHalInterface::connectModule(const char *moduleName)
    根据Android.mk定义宏变量 这里 执行 SoundTriggerHalHidl 中的 connectModule
 */ 
    // /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/soundtrigger/SoundTriggerHalHidl.cpp
    sp<SoundTriggerHalInterface> halInterface = SoundTriggerHalInterface::connectModule(HW_MODULE_PREFIX);

    if (halInterface == 0) {
        ALOGW("could not connect to HAL");
        return;
    }
    sound_trigger_module_descriptor descriptor;
    rc = halInterface->getProperties(&descriptor.properties);
    if (rc != 0) {
        // 当前版本执行到这里
        ALOGE("could not read implementation properties");
        return;
    }
    descriptor.handle = (sound_trigger_module_handle_t)android_atomic_inc(&mNextUniqueId);
    ALOGI("loaded default module %s, handle %d", descriptor.properties.description, descriptor.handle);

    sp<Module> module = new Module(this, halInterface, descriptor);
    mModules.add(descriptor.handle, module);
    mCallbackThread = new CallbackThread(this);
}