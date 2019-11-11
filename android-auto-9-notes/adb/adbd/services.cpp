
//  @   system/core/adb/services.cpp

/**
 * asocket* create_local_service_socket(const char* name, atransport* transport) {
 * --> int fd = service_to_fd(name, transport);
 * 
 * 
*/

int service_to_fd(const char* name, atransport* transport) {
    int ret = -1;

    if (is_socket_spec(name)) {
        std::string error;
        ret = socket_spec_connect(name, &error);
        if (ret < 0) {
            LOG(ERROR) << "failed to connect to socket '" << name << "': " << error;
        }
#if !ADB_HOST
    // 执行
    } else if(!strncmp("dev:", name, 4)) {
        ret = unix_open(name + 4, O_RDWR | O_CLOEXEC);
    } else if(!strncmp(name, "framebuffer:", 12)) {
        ret = create_service_thread("fb", framebuffer_service, nullptr);
    } else if (!strncmp(name, "jdwp:", 5)) {
        ret = create_jdwp_connection_fd(atoi(name+5));
    } else if(!strncmp(name, "shell", 5)) {
        ret = ShellService(name + 5, transport);
    } else if(!strncmp(name, "exec:", 5)) {
        ret = StartSubprocess(name + 5, nullptr, SubprocessType::kRaw, SubprocessProtocol::kNone);
    } else if(!strncmp(name, "sync:", 5)) {
        ret = create_service_thread("sync", file_sync_service, nullptr);
    } else if(!strncmp(name, "remount:", 8)) {
        ret = create_service_thread("remount", remount_service, nullptr);
    } else if(!strncmp(name, "reboot:", 7)) {
        void* arg = strdup(name + 7);
        if (arg == NULL) return -1;
        ret = create_service_thread("reboot", reboot_service, arg);
        if (ret < 0) free(arg);
    } else if(!strncmp(name, "root:", 5)) {
        ret = create_service_thread("root", restart_root_service, nullptr);
    } else if(!strncmp(name, "unroot:", 7)) {
        ret = create_service_thread("unroot", restart_unroot_service, nullptr);
    } else if(!strncmp(name, "backup:", 7)) {
        ret = StartSubprocess(android::base::StringPrintf("/system/bin/bu backup %s",(name + 7)).c_str(),nullptr, SubprocessType::kRaw, SubprocessProtocol::kNone);
    } else if(!strncmp(name, "restore:", 8)) {
        ret = StartSubprocess("/system/bin/bu restore", nullptr, SubprocessType::kRaw,SubprocessProtocol::kNone);
    } else if(!strncmp(name, "tcpip:", 6)) {
        int port;
        if (sscanf(name + 6, "%d", &port) != 1) {
            return -1;
        }
        ret = create_service_thread("tcp", restart_tcp_service, reinterpret_cast<void*>(port));
    } else if(!strncmp(name, "usb:", 4)) {
        ret = create_service_thread("usb", restart_usb_service, nullptr);
    } else if (!strncmp(name, "reverse:", 8)) {
        ret = reverse_service(name + 8, transport);
    } else if(!strncmp(name, "disable-verity:", 15)) {
        ret = create_service_thread("verity-on", set_verity_enabled_state_service,reinterpret_cast<void*>(0));
    } else if(!strncmp(name, "enable-verity:", 15)) {
        ret = create_service_thread("verity-off", set_verity_enabled_state_service,reinterpret_cast<void*>(1));
    } else if (!strcmp(name, "reconnect")) {
        ret = create_service_thread("reconnect", reconnect_service, transport);
#endif
    }
    if (ret >= 0) {
        close_on_exec(ret);
    }
    return ret;
}



/**
 * @    system/core/adb/socket_spec.cpp
*/

static auto& kLocalSocketTypes = *new std::unordered_map<std::string, LocalSocketType>({
#if ADB_HOST
    { "local", { ANDROID_SOCKET_NAMESPACE_FILESYSTEM, !ADB_WINDOWS } },
#else
    { "local", { ANDROID_SOCKET_NAMESPACE_RESERVED, !ADB_WINDOWS } },
#endif
    { "localreserved", { ANDROID_SOCKET_NAMESPACE_RESERVED, !ADB_HOST } },
    { "localabstract", { ANDROID_SOCKET_NAMESPACE_ABSTRACT, ADB_LINUX } },
    { "localfilesystem", { ANDROID_SOCKET_NAMESPACE_FILESYSTEM, !ADB_WINDOWS } },
});


bool is_socket_spec(const std::string& spec) {
    for (const auto& it : kLocalSocketTypes) {
        std::string prefix = it.first + ":";
        if (StartsWith(spec, prefix)) {
            return true;
        }
    }
    return StartsWith(spec, "tcp:");
}
