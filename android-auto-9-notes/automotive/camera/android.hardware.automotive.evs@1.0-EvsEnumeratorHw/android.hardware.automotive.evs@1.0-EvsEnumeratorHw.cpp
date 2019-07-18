//  @   /work/workcodes/aosp-p9.x-auto-ga/vendor/nxp-opensource/imx/evs/service.cpp

int main() {
    ALOGI("EVS Hardware Enumerator service is starting");
    android::sp<IEvsEnumerator> service = new EvsEnumerator();

    configureRpcThreadpool(1, true /* callerWillJoin */);

    // Register our service -- if somebody is already registered by our name,
    // they will be killed (their thread pool will throw an exception).
    /**
    ServiceNames.h:17:const static char kEnumeratorServiceName[] = "EvsEnumeratorHw";
    */
    status_t status = service->registerAsService(kEnumeratorServiceName);
    if (status == OK) {
        ALOGD("%s is ready.", kEnumeratorServiceName);
        joinRpcThreadpool();
    } else {
        ALOGE("Could not register service %s (%d).", kEnumeratorServiceName, status);
    }

    // In normal operation, we don't expect the thread pool to exit
    ALOGE("EVS Hardware Enumerator is shutting down");
    return 1;
}