//	@system/core/init/init.cpp

int main(int argc, char** argv) {
    if (!strcmp(basename(argv[0]), "ueventd")) {
        return ueventd_main(argc, argv);
    }

    if (!strcmp(basename(argv[0]), "watchdogd")) {
        return watchdogd_main(argc, argv);
    }

    if (REBOOT_BOOTLOADER_ON_PANIC) {
        install_reboot_signal_handlers();
    }

    add_environment("PATH", _PATH_DEFPATH);

    bool is_first_stage = (getenv("INIT_SECOND_STAGE") == nullptr);

    if (is_first_stage) {
        boot_clock::time_point start_time = boot_clock::now();

        // Clear the umask.
        umask(0);

        // Get the basic filesystem setup we need put together in the initramdisk
        // on / and then we'll let the rc file figure out the rest.
        mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755");
        mkdir("/dev/pts", 0755);
        mkdir("/dev/socket", 0755);
        mount("devpts", "/dev/pts", "devpts", 0, NULL);
        #define MAKE_STR(x) __STRING(x)
        mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC));
        // Don't expose the raw commandline to unprivileged processes.
        chmod("/proc/cmdline", 0440);
        gid_t groups[] = { AID_READPROC };
        setgroups(arraysize(groups), groups);
        mount("sysfs", "/sys", "sysfs", 0, NULL);
        mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL);
        mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
        mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
        mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));

        // Now that tmpfs is mounted on /dev and we have /dev/kmsg, we can actually
        // talk to the outside world...
        InitKernelLogging(argv);

        LOG(INFO) << "init first stage started! pid[" << getpid()<<"]";

        if (!DoFirstStageMount()) {
            LOG(ERROR) << "Failed to mount required partitions early ...";
            panic();
        }

        SetInitAvbVersionInRecovery();

        // Set up SELinux, loading the SELinux policy.
        selinux_initialize(true);

        // We're in the kernel domain, so re-exec init to transition to the init domain now
        // that the SELinux policy has been loaded.
        if (selinux_android_restorecon("/init", 0) == -1) {
            PLOG(ERROR) << "restorecon failed";
            security_failure();
        }

        setenv("INIT_SECOND_STAGE", "true", 1);

        static constexpr uint32_t kNanosecondsPerMillisecond = 1e6;
        uint64_t start_ms = start_time.time_since_epoch().count() / kNanosecondsPerMillisecond;
        setenv("INIT_STARTED_AT", StringPrintf("%" PRIu64, start_ms).c_str(), 1);

        char* path = argv[0];
        char* args[] = { path, nullptr };
        execv(path, args);

        // execv() only returns if an error happened, in which case we
        // panic and never fall through this conditional.
        PLOG(ERROR) << "execv(\"" << path << "\") failed";
        security_failure();
    }

    // At this point we're in the second stage of init.
    InitKernelLogging(argv);

    LOG(INFO) << "init second stage started! pid[" << getpid()<<"]";

    // Set up a session keyring that all processes will have access to. It
    // will hold things like FBE encryption keys. No process should override
    // its session keyring.
    keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);

    // Indicate that booting is in progress to background fw loaders, etc.
    close(open("/dev/.booting", O_WRONLY | O_CREAT | O_CLOEXEC, 0000));

    property_init();

    // If arguments are passed both on the command line and in DT,
    // properties set in DT always have priority over the command-line ones.
    process_kernel_dt();
    process_kernel_cmdline();

    // Propagate the kernel variables to internal variables
    // used by init as well as the current required properties.
    export_kernel_boot_props();

    // Make the time that init started available for bootstat to log.
    property_set("ro.boottime.init", getenv("INIT_STARTED_AT"));
    property_set("ro.boottime.init.selinux", getenv("INIT_SELINUX_TOOK"));

    // Set libavb version for Framework-only OTA match in Treble build.
    const char* avb_version = getenv("INIT_AVB_VERSION");
    if (avb_version) property_set("ro.boot.avb_version", avb_version);

    // Clean up our environment.
    unsetenv("INIT_SECOND_STAGE");
    unsetenv("INIT_STARTED_AT");
    unsetenv("INIT_SELINUX_TOOK");
    unsetenv("INIT_AVB_VERSION");

    // Now set up SELinux for second stage.
    selinux_initialize(false);
    selinux_restore_context();

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        PLOG(ERROR) << "epoll_create1 failed";
        exit(1);
    }

    signal_handler_init();

    property_load_boot_defaults();
    export_oem_lock_status();
    start_property_service();
    set_usb_controller();

    const BuiltinFunctionMap function_map;
    Action::set_function_map(&function_map);

    ActionManager& am = ActionManager::GetInstance();
    ServiceManager& sm = ServiceManager::GetInstance();
    Parser& parser = Parser::GetInstance();

    parser.AddSectionParser("service", std::make_unique<ServiceParser>(&sm));
    parser.AddSectionParser("on", std::make_unique<ActionParser>(&am));
    parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser));
    std::string bootscript = GetProperty("ro.boot.init_rc", "");//bootscript为空
    if (bootscript.empty()) {
        parser.ParseConfig("/init.rc");

        /**
		-rw-r--r-- 1 root root  117 2018-09-03 04:04 android.hidl.allocator@1.0-service.rc
		-rw-r--r-- 1 root root 9483 2018-09-03 04:04 atrace.rc
		-rw-r--r-- 1 root root 2698 2018-09-03 04:04 atrace_userdebug.rc
		-rw-r--r-- 1 root root  472 2018-09-03 04:04 audioserver.rc
		-rw-r--r-- 1 root root  174 2018-09-03 04:04 bootanim.rc
		-rw-r--r-- 1 root root 1932 2018-09-03 04:04 bootstat.rc
		-rw-r--r-- 1 root root  205 2018-09-03 04:04 cameraserver.rc
		-rw-r--r-- 1 root root  146 2018-09-03 04:04 drmserver.rc
		-rw-r--r-- 1 root root  624 2018-09-03 04:04 dumpstate.rc
		-rw-r--r-- 1 root root  152 2018-09-03 04:04 gatekeeperd.rc
		-rw-r--r-- 1 root root  334 2018-09-03 04:04 hwservicemanager.rc
		-rw-r--r-- 1 root root  338 2018-09-03 04:04 init-debug.rc
		-rw-r--r-- 1 root root 4674 2018-09-03 04:04 installd.rc
		-rw-r--r-- 1 root root  168 2018-09-03 04:04 keystore.rc
		-rw-r--r-- 1 root root  176 2018-09-03 04:04 lmkd.rc
		-rw-r--r-- 1 root root 2521 2018-09-03 04:04 logcatd.rc
		-rw-r--r-- 1 root root  631 2018-09-03 04:04 logd.rc
		-rw-r--r-- 1 root root  291 2018-09-03 04:04 logtagd.rc
		-rw-r--r-- 1 root root  142 2018-09-03 04:04 mdnsd.rc
		-rw-r--r-- 1 root root  158 2018-09-03 04:04 mediadrmserver.rc
		-rw-r--r-- 1 root root  166 2018-09-03 04:04 mediaextractor.rc
		-rw-r--r-- 1 root root  178 2018-09-03 04:04 mediametrics.rc
		-rw-r--r-- 1 root root  230 2018-09-03 04:04 mediaserver.rc
		-rw-r--r-- 1 root root  178 2018-09-03 04:04 mtpd.rc
		-rw-r--r-- 1 root root  209 2018-09-03 04:04 netd.rc
		-rw-r--r-- 1 root root  161 2018-09-03 04:04 perfprofd.rc
		-rw-r--r-- 1 root root  230 2018-09-03 04:04 racoon.rc
		-rw-r--r-- 1 root root  455 2018-09-03 04:04 servicemanager.rc
		-rw-r--r-- 1 root root  191 2018-09-03 04:04 storaged.rc
		-rw-r--r-- 1 root root  559 2018-09-03 04:04 surfaceflinger.rc
		-rw-r--r-- 1 root root  337 2018-09-03 04:04 tombstoned.rc
		-rw-r--r-- 1 root root  390 2018-09-03 04:04 uncrypt.rc
		-rw-r--r-- 1 root root  546 2018-09-03 04:04 vdc.rc
		-rw-r--r-- 1 root root  378 2018-09-03 04:18 vold.rc
		-rw-r--r-- 1 root root  805 2018-09-03 04:04 webview_zygote32.rc
		-rw-r--r-- 1 root root 3577 2018-09-03 04:04 wifi-events.rc
		-rw-r--r-- 1 root root  100 2018-09-03 04:04 wificond.rc
         *
         */
        parser.set_is_system_etc_init_loaded(parser.ParseConfig("/system/etc/init"));

        /**
        android.hardware.audio@2.0-service.rc
		android.hardware.bluetooth@1.0-service.rc
		android.hardware.camera.provider@2.4-service.rc
		android.hardware.configstore@1.0-service.rc
		android.hardware.gnss@1.0-service.rc
		android.hardware.graphics.allocator@2.0-service.rc
		android.hardware.light@2.0-service.rc
		android.hardware.media.omx@1.0-service.rc
		android.hardware.memtrack@1.0-service.rc
		android.hardware.power@1.0-service.rc
		android.hardware.sensors@1.0-service.rc
		android.hardware.usb@1.0-service.rc
		android.hardware.wifi@1.0-service.rc
		hostapd.android.rc
		rild.rc
		vndservicemanager.rc
         */
        parser.set_is_vendor_etc_init_loaded(parser.ParseConfig("/vendor/etc/init"));
        //空
        parser.set_is_odm_etc_init_loaded(parser.ParseConfig("/odm/etc/init"));
    } else {
        parser.ParseConfig(bootscript);
        parser.set_is_system_etc_init_loaded(true);
        parser.set_is_vendor_etc_init_loaded(true);
        parser.set_is_odm_etc_init_loaded(true);
    }

    // Turning this on and letting the INFO logging be discarded adds 0.2s to
    // Nexus 9 boot time, so it's disabled by default.
    if (false) DumpState();

    am.QueueEventTrigger("early-init");

    // Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
    am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
    // ... so that we can start queuing up actions that require stuff from /dev.
    am.QueueBuiltinAction(mix_hwrng_into_linux_rng_action, "mix_hwrng_into_linux_rng");
    am.QueueBuiltinAction(set_mmap_rnd_bits_action, "set_mmap_rnd_bits");
    am.QueueBuiltinAction(set_kptr_restrict_action, "set_kptr_restrict");
    am.QueueBuiltinAction(keychord_init_action, "keychord_init");
    am.QueueBuiltinAction(console_init_action, "console_init");

    // Trigger all the boot actions to get us started.
    am.QueueEventTrigger("init");

    // Repeat mix_hwrng_into_linux_rng in case /dev/hw_random or /dev/random
    // wasn't ready immediately after wait_for_coldboot_done
    am.QueueBuiltinAction(mix_hwrng_into_linux_rng_action, "mix_hwrng_into_linux_rng");

    // Don't mount filesystems or start core system services in charger mode.
    std::string bootmode = GetProperty("ro.bootmode", ""); //bootmode = "unknown"
    if (bootmode == "charger") {
        am.QueueEventTrigger("charger");
    } else {
        am.QueueEventTrigger("late-init");//执行这里
    }

    // Run all property triggers based on current state of the properties.
    am.QueueBuiltinAction(queue_property_triggers_action, "queue_property_triggers");

    while (true) {
        // By default, sleep until something happens.
        int epoll_timeout_ms = -1;

        if (!(waiting_for_prop || sm.IsWaitingForExec())) {
            am.ExecuteOneCommand();
        }
        if (!(waiting_for_prop || sm.IsWaitingForExec())) {
            if (!shutting_down) restart_processes();

            // If there's a process that needs restarting, wake up in time for that.
            if (process_needs_restart_at != 0) {
                epoll_timeout_ms = (process_needs_restart_at - time(nullptr)) * 1000;
                if (epoll_timeout_ms < 0) epoll_timeout_ms = 0;
            }

            // If there's more work to do, wake up again immediately.
            if (am.HasMoreCommands()) epoll_timeout_ms = 0;
        }

        epoll_event ev;
        int nr = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd, &ev, 1, epoll_timeout_ms));
        if (nr == -1) {
            PLOG(ERROR) << "epoll_wait failed";
        } else if (nr == 1) {
            ((void (*)()) ev.data.ptr)();
        }
    }

 