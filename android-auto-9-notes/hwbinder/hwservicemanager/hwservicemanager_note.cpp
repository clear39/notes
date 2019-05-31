//	@system/hwservicemanager/service.cpp

int main() {
    configureRpcThreadpool(1, true /* callerWillJoin */);

    ServiceManager *manager = new ServiceManager();

    if (!manager->add(serviceName, manager)) {
        ALOGE("Failed to register hwservicemanager with itself.");
    }

    TokenManager *tokenManager = new TokenManager();

    if (!manager->add(serviceName, tokenManager)) {
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
        ALOGE("Failed to set \"hwservicemanager.ready\" (error %d). "   "HAL services will not start!\n", rc);
    }

    while (true) {
        looper->pollAll(-1 /* timeoutMillis */);
    }

    return 0;
}
