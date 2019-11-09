//  @   system/core/adb/sockets.cpp
asocket* create_local_service_socket(const char* name, atransport* transport) {
#if !ADB_HOST
    // 执行
    if (!strcmp(name, "jdwp")) {
        return create_jdwp_service_socket();
    }
    if (!strcmp(name, "track-jdwp")) {
        return create_jdwp_tracker_service_socket();
    }
#endif
    /**
     * @    system/core/adb/services.cpp
    */
    int fd = service_to_fd(name, transport);
    if (fd < 0) {
        return nullptr;
    }

    asocket* s = create_local_socket(fd);
    D("LS(%d): bound to '%s' via %d", s->id, name, fd);

#if !ADB_HOST
    // 执行
    if ((!strncmp(name, "root:", 5) && getuid() != 0 && __android_log_is_debuggable()) ||
        (!strncmp(name, "unroot:", 7) && getuid() == 0) ||
        !strncmp(name, "usb:", 4) ||
        !strncmp(name, "tcpip:", 6)) {
        D("LS(%d): enabling exit_on_close", s->id);
        s->exit_on_close = 1;
    }
#endif

    return s;
}