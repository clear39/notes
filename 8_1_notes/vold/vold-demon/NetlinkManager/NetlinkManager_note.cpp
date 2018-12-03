//	@system/vold/NetlinkManager.cpp

NetlinkManager *NetlinkManager::Instance() {
    if (!sInstance)
        sInstance = new NetlinkManager();
    return sInstance;
}

NetlinkManager::NetlinkManager() {
    mBroadcaster = NULL;
}



int NetlinkManager::start() {
    struct sockaddr_nl nladdr;
    int sz = 64 * 1024;
    int on = 1;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = getpid();
    nladdr.nl_groups = 0xffffffff;

    if ((mSock = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC,
            NETLINK_KOBJECT_UEVENT)) < 0) {
        SLOGE("Unable to create uevent socket: %s", strerror(errno));
        return -1;
    }

    // When running in a net/user namespace, SO_RCVBUFFORCE is not available.
    // Try using SO_RCVBUF first.
    if ((setsockopt(mSock, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz)) < 0) &&
        (setsockopt(mSock, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz)) < 0)) {
        SLOGE("Unable to set uevent socket SO_RCVBUF/SO_RCVBUFFORCE option: %s", strerror(errno));
        goto out;
    }

    if (setsockopt(mSock, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0) {
        SLOGE("Unable to set uevent socket SO_PASSCRED option: %s", strerror(errno));
        goto out;
    }

    if (bind(mSock, (struct sockaddr *) &nladdr, sizeof(nladdr)) < 0) {
        SLOGE("Unable to bind uevent socket: %s", strerror(errno));
        goto out;
    }

    mHandler = new NetlinkHandler(mSock);
    if (mHandler->start()) {
        SLOGE("Unable to start NetlinkHandler: %s", strerror(errno));
        goto out;
    }

    return 0;

out:
    close(mSock);
    return -1;
}
