//	@frameworks/av/services/soundtrigger/SoundTriggerHwService.h
class SoundTriggerHwService :public BinderService<SoundTriggerHwService>,public BnSoundTriggerHwService{
	static char const* getServiceName() { return "media.sound_trigger_hw"; }
}


//	@frameworks/av/include/soundtrigger/ISoundTriggerHwService.h
class ISoundTriggerHwService : public IInterface
{
public:

    DECLARE_META_INTERFACE(SoundTriggerHwService);

    virtual status_t listModules(struct sound_trigger_module_descriptor *modules,uint32_t *numModules) = 0;

    virtual status_t attach(const sound_trigger_module_handle_t handle,const sp<ISoundTriggerClient>& client,sp<ISoundTrigger>& module) = 0;

    virtual status_t setCaptureState(bool active) = 0;
};



//	@frameworks/av/services/medialog/MediaLogService.cpp
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

    sp<SoundTriggerHalInterface> halInterface = SoundTriggerHalInterface::connectModule(HW_MODULE_PREFIX);

    if (halInterface == 0) {
        ALOGW("could not connect to HAL");
        return;
    }
    sound_trigger_module_descriptor descriptor;
    rc = halInterface->getProperties(&descriptor.properties);
    if (rc != 0) {
        ALOGE("could not read implementation properties");
        return;
    }
    descriptor.handle = (sound_trigger_module_handle_t)android_atomic_inc(&mNextUniqueId);
    ALOGI("loaded default module %s, handle %d", descriptor.properties.description, descriptor.handle);

    sp<Module> module = new Module(this, halInterface, descriptor);
    mModules.add(descriptor.handle, module);
    mCallbackThread = new CallbackThread(this);
}