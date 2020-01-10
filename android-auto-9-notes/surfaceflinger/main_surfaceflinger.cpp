
//  @   frameworks/native/services/surfaceflinger/main_surfaceflinger.cpp
int main(int, char**) {
    signal(SIGPIPE, SIG_IGN);

    hardware::configureRpcThreadpool(1 /* maxThreads */,false /* callerWillJoin */);
    /**
     * 判读是否本地启动 IAllocator 还是通过hwbinder访问启动服务
     * 这里 startGraphicsAllocatorService 没有做任何事情，通过 hwbinder访问启动服务
    */
    startGraphicsAllocatorService();

    // When SF is launched in its own process, limit the number of
    // binder threads to 4.
    ProcessState::self()->setThreadPoolMaxThreadCount(4);

    // start the thread pool
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();

    // instantiate surfaceflinger
    sp<SurfaceFlinger> flinger = new SurfaceFlinger();

    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);

    set_sched_policy(0, SP_FOREGROUND);

    // Put most SurfaceFlinger threads in the system-background cpuset
    // Keeps us from unnecessarily using big cores
    // Do this after the binder thread pool init
    if (cpusets_enabled()) set_cpuset_policy(0, SP_SYSTEM);

    // initialize before clients can connect
    flinger->init();

    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false,IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);

    // publish GpuService
    /**
     * dumpsys gpu
     * 
     * cmd gpu vkjson
     * 
    */
    sp<GpuService> gpuservice = new GpuService();
    sm->addService(String16(GpuService::SERVICE_NAME), gpuservice, false);

    startDisplayService(); // dependency on SF getting registered above

    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO");
    }

    // run surface flinger in this thread
    flinger->run();

    return 0;
}



static status_t startGraphicsAllocatorService() {
    using android::hardware::configstore::getBool;
    using android::hardware::configstore::V1_0::ISurfaceFlingerConfigs;
    /**
     * 返回false,表示有单独hwbinder服务启动
     * 01-01 08:00:06.774  1579  1579 I ConfigStore: android::hardware::configstore::V1_0::ISurfaceFlingerConfigs::startGraphicsAllocatorService retrieved: 0
    */
    if (!getBool<ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::startGraphicsAllocatorService>(false)) {
        return OK;
    }

    using android::hardware::graphics::allocator::V2_0::IAllocator;

    status_t result = hardware::registerPassthroughServiceImplementation<IAllocator>();
    if (result != OK) {
        ALOGE("could not start graphics allocator service");
        return result;
    }

    return OK;
}



static status_t startDisplayService() {
    using android::frameworks::displayservice::V1_0::implementation::DisplayService;
    using android::frameworks::displayservice::V1_0::IDisplayService;

    sp<IDisplayService> displayservice = new DisplayService();
    status_t err = displayservice->registerAsService();

    if (err != OK) {
        ALOGE("Could not register IDisplayService service.");
    }

    return err;
}