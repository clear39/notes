//	@system/core/logd/CommandListener.cpp
CommandListener::CommandListener(LogBuffer* buf, LogReader* /*reader*/, LogListener* /*swl*/)
    : FrameworkListener(getLogSocket()) {
    // registerCmd(new ShutdownCmd(buf, writer, swl));
    registerCmd(new ClearCmd(buf));
    registerCmd(new GetBufSizeCmd(buf));
    registerCmd(new SetBufSizeCmd(buf));
    registerCmd(new GetBufSizeUsedCmd(buf));
    registerCmd(new GetStatisticsCmd(buf));
    registerCmd(new SetPruneListCmd(buf));
    registerCmd(new GetPruneListCmd(buf));
    registerCmd(new GetEventTagCmd(buf));
    registerCmd(new ReinitCmd());
    registerCmd(new ExitCmd(this));
}


int CommandListener::getLogSocket() {
    static const char socketName[] = "logd";
    int sock = android_get_control_socket(socketName);

    if (sock < 0) {
        sock = socket_local_server(socketName, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
    }

    return sock;
}