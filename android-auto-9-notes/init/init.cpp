//	@system/core/init/init.cpp

int main(int argc, char** argv) {
    if (!strcmp(basename(argv[0]), "ueventd")) {
        return ueventd_main(argc, argv);
    }

    if (!strcmp(basename(argv[0]), "watchdogd")) {
        return watchdogd_main(argc, argv);
    }

    if (argc > 1 && !strcmp(argv[1], "subcontext")) {
        InitKernelLogging(argv);
        const BuiltinFunctionMap function_map;
        return SubcontextMain(argc, argv, &function_map);
    }

    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }

    bool is_first_stage = (getenv("INIT_SECOND_STAGE") == nullptr);

    if (is_first_stage) {
        boot_clock::time_point start_time = boot_clock::now();

        // Clear the umask.
        umask(0);

        clearenv();
        setenv("PATH", _PATH_DEFPATH, 1);
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

        if constexpr (WORLD_WRITABLE_KMSG) {
            mknod("/dev/kmsg_debug", S_IFCHR | 0622, makedev(1, 11));
        }

        mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
        mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));

        // Mount staging areas for devices managed by vold
        // See storage config details at http://source.android.com/devices/storage/
        mount("tmpfs", "/mnt", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,"mode=0755,uid=0,gid=1000");
        // /mnt/vendor is used to mount vendor-specific partitions that can not be
        // part of the vendor partition, e.g. because they are mounted read-write.
        mkdir("/mnt/vendor", 0755);

        // Now that tmpfs is mounted on /dev and we have /dev/kmsg, we can actually
        // talk to the outside world...
        InitKernelLogging(argv);

        LOG(INFO) << "init first stage started!";

        //  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/init/init_first_stage.cpp
        if (!DoFirstStageMount()) {
            LOG(FATAL) << "Failed to mount required partitions early ...";
        }

        SetInitAvbVersionInRecovery();

        // Enable seccomp if global boot option was passed (otherwise it is enabled in zygote).
        global_seccomp();

        // Set up SELinux, loading the SELinux policy.
        SelinuxSetupKernelLogging();
        SelinuxInitialize();

        // We're in the kernel domain, so re-exec init to transition to the init domain now
        // that the SELinux policy has been loaded.
        if (selinux_android_restorecon("/init", 0) == -1) {
            PLOG(FATAL) << "restorecon failed of /init failed";
        }

        setenv("INIT_SECOND_STAGE", "true", 1);

        static constexpr uint32_t kNanosecondsPerMillisecond = 1e6;
        uint64_t start_ms = start_time.time_since_epoch().count() / kNanosecondsPerMillisecond;
        setenv("INIT_STARTED_AT", std::to_string(start_ms).c_str(), 1);

        char* path = argv[0];
        char* args[] = { path, nullptr };
        execv(path, args);

        // execv() only returns if an error happened, in which case we
        // panic and never fall through this conditional.
        PLOG(FATAL) << "execv(\"" << path << "\") failed";
    }

    // At this point we're in the second stage of init.
    InitKernelLogging(argv);
    LOG(INFO) << "init second stage started!";

    // Set up a session keyring that all processes will have access to. It
    // will hold things like FBE encryption keys. No process should override
    // its session keyring.
    keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);

    // Indicate that booting is in progress to background fw loaders, etc.
    close(open("/dev/.booting", O_WRONLY | O_CREAT | O_CLOEXEC, 0000));

    //  @   system/core/init/property_service.cpp
    property_init();

    // If arguments are passed both on the command line and in DT,
    // properties set in DT always have priority over the command-line ones.
    //  @   system/core/init/init.cpp
    /**
     * 实际啥事没有干
     */
    process_kernel_dt();
    /**
     * 将内核中传入的参数 /proc/cmdline 中的 以"androidboot."开头的键值对写入prop中，
     * 且名称以 ro.boot. 开头
     */
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
    SelinuxSetupKernelLogging();
    SelabelInitialize();
    SelinuxRestoreContext();

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        PLOG(FATAL) << "epoll_create1 failed";
    }

    sigchld_handler_init();

    if (!IsRebootCapable()) {
        // If init does not have the CAP_SYS_BOOT capability, it is running in a container.
        // In that case, receiving SIGTERM will cause the system to shut down.
        InstallSigtermHandler();
    }

    property_load_boot_defaults();
    /**
     * 啥事没干
     */
    export_oem_lock_status();
    /**
     * 启动socket，并且加入到epoll中
     */
    start_property_service();
    /**
     * lrwxrwxrwx  1 root root 0 1970-01-01 12:56 ci_hdrc.0 -> ../../devices/platform/5b0d0000.usb/ci_hdrc.0/udc/ci_hdrc.0
     */
    set_usb_controller();

    /**
     * 这里注意了
     * 在 Action::AddCommand 会调用 function_map
     */
    const BuiltinFunctionMap function_map;
    Action::set_function_map(&function_map);

    //  @ system/core/init/subcontext.cpp
    subcontexts = InitializeSubcontexts();

    ActionManager& am = ActionManager::GetInstance();
    ServiceList& sm = ServiceList::GetInstance();

    /**
     * 加载所有的init.*.rc文件
     */
    LoadBootScripts(am, sm);

    // Turning this on and letting the INFO logging be discarded adds 0.2s to
    // Nexus 9 boot time, so it's disabled by default.
    if (false) DumpState();


    // Trigger(触发)
    am.QueueEventTrigger("early-init");

    // Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
    am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
    // ... so that we can start queuing up actions that require stuff from /dev.
    am.QueueBuiltinAction(MixHwrngIntoLinuxRngAction, "MixHwrngIntoLinuxRng");
    am.QueueBuiltinAction(SetMmapRndBitsAction, "SetMmapRndBits");
    am.QueueBuiltinAction(SetKptrRestrictAction, "SetKptrRestrict");
    am.QueueBuiltinAction(keychord_init_action, "keychord_init");
    am.QueueBuiltinAction(console_init_action, "console_init");

    // Trigger all the boot actions to get us started.
    am.QueueEventTrigger("init");

    // Repeat mix_hwrng_into_linux_rng in case /dev/hw_random or /dev/random
    // wasn't ready immediately after wait_for_coldboot_done
    am.QueueBuiltinAction(MixHwrngIntoLinuxRngAction, "MixHwrngIntoLinuxRng");

    // Don't mount filesystems or start core system services in charger mode.
    std::string bootmode = GetProperty("ro.bootmode", "");// unknown
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

        if (do_shutdown && !shutting_down) {
            do_shutdown = false;
            /**
             * @ system/core/init/reboot.cpp
             */
            if (HandlePowerctlMessage(shutdown_command)) {
                shutting_down = true;
            }
        }
        /**
         * waiting_for_prop（默认为nullptr）为 在init.*.rc文件中 （例：wait_for_prop sys.all.early_init.ready 1） 触发等待，
         * 等待该属性设置对应的值时，被重置为空
         * 
         * Service::is_exec_service_running() 返回变量 bool Service::is_exec_service_running_ = false; 的值，默认为false
         * 
         * 
         */
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            am.ExecuteOneCommand();
        }
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            if (!shutting_down) {
                auto next_process_restart_time = RestartProcesses();

                // If there's a process that needs restarting, wake up in time for that.
                if (next_process_restart_time) {
                    epoll_timeout_ms = std::chrono::ceil<std::chrono::milliseconds>(*next_process_restart_time - boot_clock::now()).count();
                    if (epoll_timeout_ms < 0){
                        epoll_timeout_ms = 0;
                    } 
                }
            }

            // If there's more work to do, wake up again immediately.
            /**
             * 判断am中是否有消息队列中是否有消息
             */
            if (am.HasMoreCommands()) epoll_timeout_ms = 0;
        }

        /**
         * 事件监听
         */
        epoll_event ev;
        int nr = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd, &ev, 1, epoll_timeout_ms));
        if (nr == -1) {
            PLOG(ERROR) << "epoll_wait failed";
        } else if (nr == 1) {
            ((void (*)()) ev.data.ptr)();
        }
    }

    return 0;
}

//  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/init/init_first_stage.cpp
// Public functions
// ----------------
// Mounts partitions specified by fstab in device tree.
bool DoFirstStageMount() {
    LOG(INFO) << "DoFirstStageMount ... ";
    // Skips first stage mount if we're in recovery mode.
    /**
     * //  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/init/init_first_stage.cpp
     * 判断　/sbin/recovery　是否存在 
     */
    if (IsRecoveryMode()) {
        LOG(INFO) << "First stage mount skipped (recovery mode)";
        return true;
    }

     /**
     * //  "/proc/device-tree/firmware/android/fstab/compatible" 中的内容是否为　android,fstab
     */
    // Firstly checks if device tree fstab entries are compatible.
    if (!is_android_dt_value_expected("fstab/compatible", "android,fstab")) {　// true
        LOG(INFO) << "First stage mount skipped (missing/incompatible fstab in device tree)";
        return true;
    }

     /**
     * //  "/proc/device-tree/firmware/android/vbmeta/compatible" 中的内容是否为　android,vbmeta
     */
    std::unique_ptr<FirstStageMount> handle = FirstStageMount::Create();　  //  FirstStageMountVBootV2
    if (!handle) {
        LOG(ERROR) << "Failed to create FirstStageMount";
        return false;
    }
    return handle->DoFirstStageMount();
}


std::unique_ptr<FirstStageMount> FirstStageMount::Create() {
    LOG(INFO) << "FirstStageMount::Create ... ";
   
    if (IsDtVbmetaCompatible()) { // true
        return std::make_unique<FirstStageMountVBootV2>();
    } else {
        return std::make_unique<FirstStageMountVBootV1>();
    }
}




static void LoadBootScripts(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser = CreateParser(action_manager, service_list);

    std::string bootscript = GetProperty("ro.boot.init_rc", ""); // empty
    if (bootscript.empty()) {
        parser.ParseConfig("/init.rc");
        if (!parser.ParseConfig("/system/etc/init")) {
            late_import_paths.emplace_back("/system/etc/init");
        }
        if (!parser.ParseConfig("/product/etc/init")) {
            late_import_paths.emplace_back("/product/etc/init");
        }
        if (!parser.ParseConfig("/odm/etc/init")) {
            late_import_paths.emplace_back("/odm/etc/init");
        }

        
        if (!parser.ParseConfig("/vendor/etc/init")) {
            late_import_paths.emplace_back("/vendor/etc/init");
        }
    } else {
        ......
        //parser.ParseConfig(bootscript);
    }
}


Parser CreateParser(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser;

    //  static std::vector<Subcontext>* subcontexts;
    parser.AddSectionParser("service", std::make_unique<ServiceParser>(&service_list, subcontexts));
    parser.AddSectionParser("on", std::make_unique<ActionParser>(&action_manager, subcontexts));
    parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser)); // init.rc 文件导入

    return parser;
}



static void process_kernel_dt() {
    //  @   system/core/init/util.cpp
    if (!is_android_dt_value_expected("compatible", "android,firmware")) { // true
        return;
    }

    //  get_android_dt_dir() 得到 /proc/device-tree/firmware/android/    
    std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir(get_android_dt_dir().c_str()), closedir);
    if (!dir) return;


/***
 * autolink_8q:/ # ls -al /proc/device-tree/firmware/android/
    -r--r--r-- 1 root root 17 1970-01-01 09:07 compatible
    drwxr-xr-x 3 root root  0 1970-01-01 09:07 fstab
    -r--r--r-- 1 root root  8 1970-01-01 11:00 name
    drwxr-xr-x 2 root root  0 1970-01-01 09:07 vbmeta
 */
    std::string dt_file;
    struct dirent *dp;
    while ((dp = readdir(dir.get())) != NULL) {
        /**
         * dp->d_type 必须为文件，且不能为 compatible 和 name
         */
        if (dp->d_type != DT_REG || !strcmp(dp->d_name, "compatible") || !strcmp(dp->d_name, "name")) {
            continue;
        }

        std::string file_name = get_android_dt_dir() + dp->d_name;

        android::base::ReadFileToString(file_name, &dt_file);
        std::replace(dt_file.begin(), dt_file.end(), ',', '.');

        property_set("ro.boot."s + dp->d_name, dt_file);
    }
}


bool is_android_dt_value_expected(const std::string& sub_path, const std::string& expected_content) {
    std::string dt_content;
    if (read_android_dt_file(sub_path, &dt_content)) {//    android,firmware
        if (dt_content == expected_content) {
            return true;
        }
    }
    return false;
}


// Reads the content of device tree file under the platform's Android DT directory.
// Returns true if the read is success, false otherwise.
bool read_android_dt_file(const std::string& sub_path, std::string* dt_content) {
    const std::string file_name = get_android_dt_dir() + sub_path;  //  /proc/device-tree/firmware/android/compatible
    if (android::base::ReadFileToString(file_name, dt_content)) {
        if (!dt_content->empty()) {
            dt_content->pop_back();  // Trims the trailing '\0' out.
            return true;
        }
    }
    return false;
}


// FIXME: The same logic is duplicated in system/core/fs_mgr/
const std::string& get_android_dt_dir() {
    // Set once and saves time for subsequent calls to this function
    static const std::string kAndroidDtDir = init_android_dt_dir();  // "/proc/device-tree/firmware/android/"
    return kAndroidDtDir;
}

static std::string init_android_dt_dir() {
    //  const std::string kDefaultAndroidDtDir("/proc/device-tree/firmware/android/");
    // Use the standard procfs-based path by default
    std::string android_dt_dir = kDefaultAndroidDtDir;
    // The platform may specify a custom Android DT path in kernel cmdline
    import_kernel_cmdline(false,
                          [&](const std::string& key, const std::string& value, bool in_qemu) {
                              if (key == "androidboot.android_dt_dir") {
                                  android_dt_dir = value;
                              }
                          });
    LOG(INFO) << "Using Android DT directory " << android_dt_dir;
    return android_dt_dir;
}


void import_kernel_cmdline(bool in_qemu,const std::function<void(const std::string&, const std::string&, bool)>& fn) {
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);

    for (const auto& entry : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> pieces = android::base::Split(entry, "=");
        if (pieces.size() == 2) {
            fn(pieces[0], pieces[1], in_qemu);
        }
    }
}


static void process_kernel_cmdline() {
    // The first pass does the common stuff, and finds if we are in qemu.
    // The second pass is only necessary for qemu to export all kernel params
    // as properties.
    import_kernel_cmdline(false, import_kernel_nv);
    if (qemu[0]) import_kernel_cmdline(true, import_kernel_nv);
}

static void import_kernel_nv(const std::string& key, const std::string& value, bool for_emulator) {
    if (key.empty()) return;

    if (for_emulator) {
        // In the emulator, export any kernel option with the "ro.kernel." prefix.
        property_set("ro.kernel." + key, value);
        return;
    }

    if (key == "qemu") {
        strlcpy(qemu, value.c_str(), sizeof(qemu));
    } else if (android::base::StartsWith(key, "androidboot.")) {
        property_set("ro.boot." + key.substr(12), value);
    }
}



bool IsRebootCapable() {
    if (!CAP_IS_SUPPORTED(CAP_SYS_BOOT)) {
        PLOG(WARNING) << "CAP_SYS_BOOT is not supported";
        return true;
    }

    ScopedCaps caps(cap_get_proc());
    if (!caps) {
        PLOG(WARNING) << "cap_get_proc() failed";
        return true;
    }

    cap_flag_value_t value = CAP_SET;
    if (cap_get_flag(caps.get(), CAP_SYS_BOOT, CAP_EFFECTIVE, &value) != 0) {
        PLOG(WARNING) << "cap_get_flag(CAP_SYS_BOOT, EFFECTIVE) failed";
        return true;
    }
    return value == CAP_SET;
}

 