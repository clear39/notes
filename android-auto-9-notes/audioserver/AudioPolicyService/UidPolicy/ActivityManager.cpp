//  @   frameworks/native/libs/binder/ActivityManager.cpp

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