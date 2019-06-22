//  @system/vold/vold.rc
service vold /system/bin/vold \
        --blkid_context=u:r:blkid:s0 --blkid_untrusted_context=u:r:blkid_untrusted:s0 \
        --fsck_context=u:r:fsck:s0 --fsck_untrusted_context=u:r:fsck_untrusted:s0
    class core
    ioprio be 2
    writepid /dev/cpuset/foreground/tasks
    shutdown critical
    group reserved_disk



//	@system/vold/main.cpp
int main(int argc, char** argv) {
    atrace_set_tracing_enabled(false);
    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));

    LOG(INFO) << "Vold 3.0 (the awakening) firing up";

    ATRACE_BEGIN("main");


    LOG(VERBOSE) << "Detected support for:"
            << (android::vold::IsFilesystemSupported("ext4") ? " ext4" : "")
            << (android::vold::IsFilesystemSupported("f2fs") ? " f2fs" : "")
            << (android::vold::IsFilesystemSupported("vfat") ? " vfat" : "");

    VolumeManager *vm;
    NetlinkManager *nm;

    parse_args(argc, argv);//解析SELinux

    sehandle = selinux_android_file_context_handle();
    if (sehandle) {
        selinux_android_set_sehandle(sehandle);
    }

    mkdir("/dev/block/vold", 0755);

    /* For when cryptfs checks and mounts an encrypted filesystem */
    klog_set_level(6);

    /* Create our singleton managers */
    if (!(vm = VolumeManager::Instance())) {
        LOG(ERROR) << "Unable to create VolumeManager";
        exit(1);
    }

    if (!(nm = NetlinkManager::Instance())) {
        LOG(ERROR) << "Unable to create NetlinkManager";
        exit(1);
    }

    if (android::base::GetBoolProperty("vold.debug", false)) {
        vm->setDebug(true);
    }

    if (vm->start()) {
        PLOG(ERROR) << "Unable to start VolumeManager";
        exit(1);
    }

    bool has_adoptable;
    bool has_quota;
    bool has_reserved;

    if (process_config(vm, &has_adoptable, &has_quota, &has_reserved)) {
        PLOG(ERROR) << "Error reading configuration... continuing anyways";
    }

    ATRACE_BEGIN("VoldNativeService::start");
    if (android::vold::VoldNativeService::start() != android::OK) {
        LOG(ERROR) << "Unable to start VoldNativeService";
        exit(1);
    }
    ATRACE_END();

    LOG(DEBUG) << "VoldNativeService::start() completed OK";

    ATRACE_BEGIN("NetlinkManager::start");
    if (nm->start()) {
        PLOG(ERROR) << "Unable to start NetlinkManager";
        exit(1);
    }
    ATRACE_END();

    // This call should go after listeners are started to avoid
    // a deadlock between vold and init (see b/34278978 for details)
    android::base::SetProperty("vold.has_adoptable", has_adoptable ? "1" : "0");
    android::base::SetProperty("vold.has_quota", has_quota ? "1" : "0");
    android::base::SetProperty("vold.has_reserved", has_reserved ? "1" : "0");

    // Do coldboot here so it won't block booting,
    // also the cold boot is needed in case we have flash drive
    // connected before Vold launched
    coldboot("/sys/block");

    ATRACE_END();

    android::IPCThreadState::self()->joinThreadPool();
    LOG(INFO) << "vold shutting down";

    exit(0);
}



