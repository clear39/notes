//  @   /system/core/adb/daemon/main.cpp
int main(int argc, char** argv) {
    while (true) {
        static struct option opts[] = {
            {"root_seclabel", required_argument, nullptr, 's'},
            {"device_banner", required_argument, nullptr, 'b'},
            {"version", no_argument, nullptr, 'v'},
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", opts, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 's':
            root_seclabel = optarg; // --root_seclabel=u:r:su:s0
            break;
        case 'b':
            adb_device_banner = optarg;
            break;
        case 'v':
            printf("Android Debug Bridge Daemon version %d.%d.%d\n", ADB_VERSION_MAJOR,ADB_VERSION_MINOR, ADB_SERVER_VERSION);
            return 0;
        default:
            // getopt already prints "adbd: invalid option -- %c" for us.
            return 1;
        }
    }

    close_stdin();

    debuggerd_init(nullptr);
    /**
     * adb 日志初始化
    */
    adb_trace_init(argv);

    D("Handling main()");

    return adbd_main(DEFAULT_ADB_PORT);
}




int adbd_main(int server_port) {
    umask(0);

    signal(SIGPIPE, SIG_IGN);

    /**
     * @    system/core/adb/transport.cpp
    */
    init_transport_registration();

    // We need to call this even if auth isn't enabled because the file
    // descriptor will always be open.
    /**
     * @    system/core/adb/adbd_auth.cpp
     * 
    */
    adbd_cloexec_auth_socket();
    /**
     * ALLOW_ADBD_NO_AUTH 在 Android.mk 中定义
     * 
     * 如果 eng和userdebug 版本 则 ALLOW_ADBD_NO_AUTH 为 1，否则为 0
     * ro.adb.secure 为空
    */
    if (ALLOW_ADBD_NO_AUTH && !android::base::GetBoolProperty("ro.adb.secure", false)) {
        auth_required = false;
    }

    adbd_auth_init();

    // Our external storage path may be different than apps, since
    // we aren't able to bind mount after dropping root.
    const char* adb_external_storage = getenv("ADB_EXTERNAL_STORAGE");
    if (adb_external_storage != nullptr) {
        setenv("EXTERNAL_STORAGE", adb_external_storage, 1);
    } else {
        D("Warning: ADB_EXTERNAL_STORAGE is not set.  Leaving EXTERNAL_STORAGE" " unchanged.\n");
    }

    drop_privileges(server_port);

    bool is_usb = false;
    /**
     * /dev/usb-ffs/adb/ep0
    */
    if (access(USB_FFS_ADB_EP0, F_OK) == 0) {
        // Listen on USB.
        usb_init();
        is_usb = true;
    }

    // If one of these properties is set, also listen on that port.
    // If one of the properties isn't set and we couldn't listen on usb, listen
    // on the default port.
    std::string prop_port = android::base::GetProperty("service.adb.tcp.port", "");
    if (prop_port.empty()) {
        prop_port = android::base::GetProperty("persist.adb.tcp.port", "");
    }

    int port;
    if (sscanf(prop_port.c_str(), "%d", &port) == 1 && port > 0) {
        D("using port=%d", port);
        // Listen on TCP port specified by service.adb.tcp.port property.
        setup_port(port);
    } else if (!is_usb) {
        // Listen on default port.
        /**
         * system/core/adb/adb.h:184:#define DEFAULT_ADB_LOCAL_TRANSPORT_PORT 5555
        */
        setup_port(DEFAULT_ADB_LOCAL_TRANSPORT_PORT);
    }

    D("adbd_main(): pre init_jdwp()");
    init_jdwp();
    D("adbd_main(): post init_jdwp()");

    D("Event loop starting");
    fdevent_loop();

    return 0;
}







static void setup_port(int port) {
    local_init(port);
    setup_mdns(port);
}
