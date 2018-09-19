//system/hwservicemanager

service hwservicemanager /system/bin/hwservicemanager
    user system
    disabled
    group system readproc
    critical
    onrestart setprop hwservicemanager.ready false
    onrestart class_restart hal
    onrestart class_restart early_hal
    writepid /dev/cpuset/system-background/tasks
    class animation
    shutdown critical



//程序入口	system/hwservicemanager/service.cpp
int main() {
    configureRpcThreadpool(1, true /* callerWillJoin */);

    ServiceManager *manager = new ServiceManager();// 默认构造函数，没有实现

    if (!manager->add(serviceName, manager)) {// static std::string serviceName = "default";
        ALOGE("Failed to register hwservicemanager with itself.");
    }

    TokenManager *tokenManager = new TokenManager();

    if (!manager->add(serviceName, tokenManager)) {// static std::string serviceName = "default";
        ALOGE("Failed to register ITokenManager with hwservicemanager.");
    }

    sp<Looper> looper(Looper::prepare(0 /* opts */));

    int binder_fd = -1;

    IPCThreadState::self()->setupPolling(&binder_fd);
    if (binder_fd < 0) {
        ALOGE("Failed to aquire binder FD. Aborting...");
        return -1;
    }
    // Flush after setupPolling(), to make sure the binder driver
    // knows about this thread handling commands.
    IPCThreadState::self()->flushCommands();

    sp<BinderCallback> cb(new BinderCallback);
    if (looper->addFd(binder_fd, Looper::POLL_CALLBACK, Looper::EVENT_INPUT, cb,nullptr) != 1) {
        ALOGE("Failed to add hwbinder FD to Looper. Aborting...");
        return -1;
    }

    // Tell IPCThreadState we're the service manager
    sp<BnHwServiceManager> service = new BnHwServiceManager(manager);
    IPCThreadState::self()->setTheContextObject(service);
    // Then tell binder kernel
    ioctl(binder_fd, BINDER_SET_CONTEXT_MGR, 0);
    // Only enable FIFO inheritance for hwbinder
    // FIXME: remove define when in the kernel
#define BINDER_SET_INHERIT_FIFO_PRIO    _IO('b', 10)

    int rc = ioctl(binder_fd, BINDER_SET_INHERIT_FIFO_PRIO);
    if (rc) {
        ALOGE("BINDER_SET_INHERIT_FIFO_PRIO failed with error %d\n", rc);
    }

    rc = property_set("hwservicemanager.ready", "true");
    if (rc) {
        ALOGE("Failed to set \"hwservicemanager.ready\" (error %d). " "HAL services will not start!\n", rc);
    }

    while (true) {
        looper->pollAll(-1 /* timeoutMillis */);
    }

    return 0;
}

//分析 configureRpcThreadpool(1, true /*callerWillJoin*/);
//	system/libhidl/transport/HidlTransportSupport.cpp
void configureRpcThreadpool(size_t maxThreads, bool callerWillJoin) {
    // TODO(b/32756130) this should be transport-dependent
    configureBinderRpcThreadpool(maxThreads, callerWillJoin);
}

//	system/libhidl/transport/HidlBinderSupport.cpp
void configureBinderRpcThreadpool(size_t maxThreads, bool callerWillJoin) {
    ProcessState::self()->setThreadPoolConfiguration(maxThreads, callerWillJoin /*callerJoinsPool*/);
}

//	system/libhwbinder/ProcessState.cpp 这里基本上和binder一样的源码
sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    gProcess = new ProcessState;
    return gProcess;
}

ProcessState::ProcessState()
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)
    , mExecutingThreadsCount(0)
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
    , mStarvationStartTimeMs(0)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mSpawnThreadOnStart(true)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/hwbinder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
    }
    else {
        ALOGE("Binder driver could not be opened.  Terminating.");
    }
}

static int open_driver()
{
    int fd = open("/dev/hwbinder", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        int vers = 0;
        status_t result = ioctl(fd, BINDER_VERSION, &vers);//获取hwbinder版本号
        if (result == -1) {
            ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            ALOGE("Binder driver protocol(%d) does not match user space protocol(%d)!", vers, BINDER_CURRENT_PROTOCOL_VERSION);
            close(fd);
            fd = -1;
        }
        size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);//设置最大线程
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '/dev/hwbinder' failed: %s\n", strerror(errno));
    }
    return fd;
}


