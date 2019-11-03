class UidPolicy : public BnUidObserver, public virtual IBinder::DeathRecipient
{

}

//  @ /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp

explicit UidPolicy::UidPolicy(wp<AudioPolicyService> service)
    : mService(service), mObserverRegistered(false)
{
}

void AudioPolicyService::UidPolicy::registerSelf()
{
    //  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/libs/binder/ActivityManager.cpp
    ActivityManager am;
    am.registerUidObserver(this, ActivityManager::UID_OBSERVER_GONE | ActivityManager::UID_OBSERVER_IDLE | ActivityManager::UID_OBSERVER_ACTIVE,
                           ActivityManager::PROCESS_STATE_UNKNOWN,
                           String16("audioserver"));

    status_t res = am.linkToDeath(this);
    if (!res)
    {
        Mutex::Autolock _l(mLock);
        mObserverRegistered = true;
    }
    else
    {
        ALOGE("UidPolicy::registerSelf linkToDeath failed: %d", res);
        am.unregisterUidObserver(this);
    }
}

void ActivityManager::registerUidObserver(const sp<IUidObserver> &observer,
                                          const int32_t event,
                                          const int32_t cutpoint,
                                          const String16 &callingPackage)
{
    sp<IActivityManager> service = getService();
    if (service != NULL)
    {
        service->registerUidObserver(observer, event, cutpoint, callingPackage);
    }
}

sp<IActivityManager> ActivityManager::getService()
{
    std::lock_guard<Mutex> scoped_lock(mLock);
    int64_t startTime = 0;
    sp<IActivityManager> service = mService;
    while (service == NULL || !IInterface::asBinder(service)->isBinderAlive())
    {
        sp<IBinder> binder = defaultServiceManager()->checkService(String16("activity"));
        if (binder == NULL)
        {
            // Wait for the activity service to come back...
            if (startTime == 0)
            {
                startTime = uptimeMillis();
                ALOGI("Waiting for activity service");
            }
            else if ((uptimeMillis() - startTime) > 1000000)
            {
                ALOGW("Waiting too long for activity service, giving up");
                service = NULL;
                break;
            }
            usleep(25000);
        }
        else
        {
            service = interface_cast<IActivityManager>(binder);
            mService = service;
        }
    }
    return service;
}