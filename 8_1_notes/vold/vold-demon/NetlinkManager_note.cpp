//	@system/vold/NetlinkManager.cpp

NetlinkManager *NetlinkManager::Instance() {
    if (!sInstance)
        sInstance = new NetlinkManager();
    return sInstance;
}

NetlinkManager::NetlinkManager() {
    mBroadcaster = NULL;
}
