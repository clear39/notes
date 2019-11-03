

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/soundtrigger/SoundTriggerHalHidl.cpp


/* static */
sp<SoundTriggerHalInterface> SoundTriggerHalInterface::connectModule(const char *moduleName)
{
    return new SoundTriggerHalHidl(moduleName);
}

SoundTriggerHalHidl::SoundTriggerHalHidl(const char *moduleName) : mModuleName(moduleName), mNextUniqueId(1)
{
    LOG_ALWAYS_FATAL_IF(strcmp(mModuleName, "primary") != 0, "Treble soundtrigger only supports primary module");
}

int SoundTriggerHalHidl::getProperties(struct sound_trigger_properties *properties)
{
    sp<ISoundTriggerHw> soundtrigger = getService();
    if (soundtrigger == 0) {
        return -ENODEV;
    }

    ISoundTriggerHw::Properties halProperties;
    Return<void> hidlReturn;
    int ret;
    {
        AutoMutex lock(mHalLock);
        hidlReturn = soundtrigger->getProperties([&](int rc, auto res) {
            ret = rc;
            halProperties = res;
            ALOGI("getProperties res implementor %s", res.implementor.c_str());
        });
    }

    if (hidlReturn.isOk()) {
        if (ret == 0) {
            convertPropertiesFromHal(properties, &halProperties);
        }
    } else {
        ALOGE("getProperties error %s", hidlReturn.description().c_str());
        return FAILED_TRANSACTION;
    }
    ALOGI("getProperties ret %d", ret);
    return ret;
}

sp<ISoundTriggerHw> SoundTriggerHalHidl::getService()
{
    AutoMutex lock(mLock);
    if (mISoundTrigger == 0) {
        if (mModuleName == NULL) {
            mModuleName = "primary";
        }
        mISoundTrigger = ISoundTriggerHw::getService();
        if (mISoundTrigger != 0) {
            mISoundTrigger->linkToDeath(HalDeathHandler::getInstance(), 0 /*cookie*/);
        }
    }
    return mISoundTrigger;
}
