//  @   /work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/graphics/composer/2.1/default/service.cpp
int main() {
    // the conventional HAL might start binder services
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->setThreadPoolMaxThreadCount(4);
    android::ProcessState::self()->startThreadPool();

    // same as SF main thread
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK,&param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO: %d", errno);
    }

    //  @   /work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/graphics/composer/2.1/default/passthrough.cpp
    return defaultPassthroughServiceImplementation<IComposer>(4);
}


extern "C" IComposer* HIDL_FETCH_IComposer(const char* /* name */) {
    return HwcLoader::load();
}
