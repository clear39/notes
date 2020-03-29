


DrmManager::DrmManager() :
    mDecryptSessionId(0),
    mConvertId(0) {
    srand(time(NULL));
    memset(mUniqueIdArray, 0, sizeof(bool) * kMaxNumUniqueIds);
}




status_t DrmManager::loadPlugIns() {
    String8 pluginDirPath("/system/lib/drm");
    loadPlugIns(pluginDirPath);
    return DRM_NO_ERROR;
}


/**
 * 
*/
status_t DrmManager::loadPlugIns(const String8& plugInDirPath) {
    mPlugInManager.loadPlugIns(plugInDirPath);
    Vector<String8> plugInPathList = mPlugInManager.getPlugInIdList();
    for (size_t i = 0; i < plugInPathList.size(); ++i) {
        String8 plugInPath = plugInPathList[i];
        DrmSupportInfo* info = mPlugInManager.getPlugIn(plugInPath).getSupportInfo(0);
        if (NULL != info) {
            if (mSupportInfoToPlugInIdMap.indexOfKey(*info) < 0) {
                mSupportInfoToPlugInIdMap.add(*info, plugInPath);
            }
            delete info;
        }
    }
    return DRM_NO_ERROR;
}








DecryptHandle* DrmManager::openDecryptSession(int uniqueId, int fd, off64_t offset, off64_t length, const char* mime) {

    Mutex::Autolock _l(mDecryptLock);
    status_t result = DRM_ERROR_CANNOT_HANDLE;
    Vector<String8> plugInIdList = mPlugInManager.getPlugInIdList();

    DecryptHandle* handle = new DecryptHandle();
    if (NULL != handle) {
        handle->decryptId = mDecryptSessionId + 1;
        for (size_t index = 0; index < plugInIdList.size(); index++) {
            const String8& plugInId = plugInIdList.itemAt(index);
            IDrmEngine& rDrmEngine = mPlugInManager.getPlugIn(plugInId);
            result = rDrmEngine.openDecryptSession(uniqueId, handle, fd, offset, length, mime);

            if (DRM_NO_ERROR == result) {
                ++mDecryptSessionId;
                mDecryptSessionMap.add(mDecryptSessionId, &rDrmEngine);
                break;
            }
        }
    }
    if (DRM_NO_ERROR != result) {
        delete handle; handle = NULL;
    }
    return handle;
}
