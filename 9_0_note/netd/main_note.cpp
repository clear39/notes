service netd /system/bin/netd
    class main
    socket netd stream 0660 root system
    socket dnsproxyd stream 0660 root inet
    socket mdns stream 0660 root system
    socket fwmarkd stream 0660 root inet
    onrestart restart zygote
    onrestart restart zygote_secondary


int main() {
    using android::net::gCtls;
    Stopwatch s;

    ALOGI("Netd 1.0 starting");
    remove_pid_file();

    blockSigpipe();

    // Before we do anything that could fork, mark CLOEXEC the UNIX sockets that we get from init.
    // FrameworkListener does this on initialization as well, but we only initialize these
    // components after having initialized other subsystems that can fork.
    for (const auto& sock : { CommandListener::SOCKET_NAME,
                              DnsProxyListener::SOCKET_NAME,
                              FwmarkServer::SOCKET_NAME,
                              MDnsSdListener::SOCKET_NAME }) {
        setCloseOnExec(sock);
    }

    NetlinkManager *nm = NetlinkManager::Instance();
    if (nm == nullptr) {
        ALOGE("Unable to create NetlinkManager");
        exit(1);
    };

    gCtls = new android::net::Controllers();
    gCtls->init();

    CommandListener cl;
    nm->setBroadcaster((SocketListener *) &cl);

    if (nm->start()) {
        ALOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    }

    std::unique_ptr<NFLogListener> logListener;
    {
        auto result = makeNFLogListener();
        if (!isOk(result)) {
            ALOGE("Unable to create NFLogListener: %s", toString(result).c_str());
            exit(1);
        }
        logListener = std::move(result.value());
        auto status = gCtls->wakeupCtrl.init(logListener.get());
        if (!isOk(result)) {
            ALOGE("Unable to init WakeupController: %s", toString(result).c_str());
            // We can still continue without wakeup packet logging.
        }
    }

    // Set local DNS mode, to prevent bionic from proxying
    // back to this service, recursively.
    setenv("ANDROID_DNS_MODE", "local", 1);
    DnsProxyListener dpl(&gCtls->netCtrl, &gCtls->eventReporter);
    if (dpl.startListener()) {
        ALOGE("Unable to start DnsProxyListener (%s)", strerror(errno));
        exit(1);
    }

    MDnsSdListener mdnsl;
    if (mdnsl.startListener()) {
        ALOGE("Unable to start MDnsSdListener (%s)", strerror(errno));
        exit(1);
    }

    FwmarkServer fwmarkServer(&gCtls->netCtrl, &gCtls->eventReporter, &gCtls->trafficCtrl);
    if (fwmarkServer.startListener()) {
        ALOGE("Unable to start FwmarkServer (%s)", strerror(errno));
        exit(1);
    }

    Stopwatch subTime;
    status_t ret;
    if ((ret = NetdNativeService::start()) != android::OK) {
        ALOGE("Unable to start NetdNativeService: %d", ret);
        exit(1);
    }
    ALOGI("Registering NetdNativeService: %.1fms", subTime.getTimeAndReset());

    /*
     * Now that we're up, we can respond to commands. Starting the listener also tells
     * NetworkManagementService that we are up and that our binder interface is ready.
     */
    if (cl.startListener()) {
        ALOGE("Unable to start CommandListener (%s)", strerror(errno));
        exit(1);
    }
    ALOGI("Starting CommandListener: %.1fms", subTime.getTimeAndReset());

    write_pid_file();

    // Now that netd is ready to process commands, advertise service
    // availability for HAL clients.
    NetdHwService mHwSvc;
    if ((ret = mHwSvc.start()) != android::OK) {
        ALOGE("Unable to start NetdHwService: %d", ret);
        exit(1);
    }
    ALOGI("Registering NetdHwService: %.1fms", subTime.getTimeAndReset());

    ALOGI("Netd started in %dms", static_cast<int>(s.timeTaken()));

    IPCThreadState::self()->joinThreadPool();

    ALOGI("Netd exiting");

    remove_pid_file();

    exit(0);
}