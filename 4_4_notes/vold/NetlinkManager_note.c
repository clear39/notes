//	if (!(nm = NetlinkManager::Instance())) {

NetlinkManager *NetlinkManager::sInstance = NULL;

NetlinkManager *NetlinkManager::Instance() {
    if (!sInstance)
        sInstance = new NetlinkManager();
    return sInstance;
}

NetlinkManager::NetlinkManager() {
    mBroadcaster = NULL;
}
