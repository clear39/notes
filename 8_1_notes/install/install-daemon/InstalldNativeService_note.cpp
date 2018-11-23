//		frameworks/native/cmds/installd/InstalldNativeService.cpp
status_t InstalldNativeService::start() {
    IPCThreadState::self()->disableBackgroundScheduling(true);
    status_t ret = BinderService<InstalldNativeService>::publish();
    if (ret != android::OK) {
        return ret;
    }
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();
    ps->giveThreadPoolName();
    return android::OK;
}


//	@frameworks/native/libs/binder/include/binder/BinderService.h
template<typename SERVICE>
class BinderService
{
public:
    static status_t publish(bool allowIsolated = false) {
        sp<IServiceManager> sm(defaultServiceManager());
        return sm->addService(String16(SERVICE::getServiceName()),new SERVICE(), allowIsolated);
    }
}


static char const* getServiceName() { return "installd"; }


//	frameworks/native/cmds/installd/binder/android/os/IInstalld.aidl