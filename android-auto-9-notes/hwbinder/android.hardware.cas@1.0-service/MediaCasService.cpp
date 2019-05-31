MediaCasService::MediaCasService() :
    mCasLoader("createCasFactory"),     // CasFactory @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/native/include/media/cas/CasAPI.h
    mDescramblerLoader("createDescramblerFactory") {  // DescramblerFactory @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/native/include/media/cas/DescramblerAPI.h
}


Return<void> MediaCasService::enumeratePlugins(enumeratePlugins_cb _hidl_cb) {

    ALOGV("%s", __FUNCTION__);

    vector<HidlCasPluginDescriptor> results;
    mCasLoader.enumeratePlugins(&results);

    _hidl_cb(results);
    return Void();
}


template <class T>
bool FactoryLoader<T>::enumeratePlugins(vector<HidlCasPluginDescriptor>* results) {
    ALOGI("enumeratePlugins");

    results->clear();

    String8 dirPath("/vendor/lib/mediacas");
    DIR* pDir = opendir(dirPath.string());

    if (pDir == NULL) {
        ALOGE("Failed to open plugin directory %s", dirPath.string());
        return false;
    }

    Mutex::Autolock autoLock(mMapLock);

    struct dirent* pEntry;
    while ((pEntry = readdir(pDir))) {

        //  /vendor/lib/mediacas/libclearkeycasplugin.so @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/drm/mediacas/plugins/clearkey
        String8 pluginPath = dirPath + "/" + pEntry->d_name;  //    @   /vendor/lib/mediacas/libclearkeycasplugin.so
        if (pluginPath.getPathExtension() == ".so") {
            queryPluginsFromPath(pluginPath, results);
        }
    }
    return true;
}

template <class T>
bool FactoryLoader<T>::queryPluginsFromPath(const String8 &path, vector<HidlCasPluginDescriptor>* results) {
    closeFactory();

    vector<CasPluginDescriptor> descriptors;
    if (!openFactory(path) || mFactory->queryPlugins(&descriptors) != OK) {
        closeFactory();
        return false;
    }

    for (auto it = descriptors.begin(); it != descriptors.end(); it++) {
        results->push_back( HidlCasPluginDescriptor {
                .caSystemId = it->CA_system_id,
                .name = it->name.c_str()});
    }
    return true;
}

template <class T>
bool FactoryLoader<T>::openFactory(const String8 &path) {
    // get strong pointer to open shared library
    ssize_t index = mLibraryPathToOpenLibraryMap.indexOfKey(path);
    if (index >= 0) {
        mLibrary = mLibraryPathToOpenLibraryMap[index].promote();
    } else {
        index = mLibraryPathToOpenLibraryMap.add(path, NULL);
    }

    if (!mLibrary.get()) {
        mLibrary = new SharedLibrary(path);
        if (!*mLibrary) {
            return false;
        }

        mLibraryPathToOpenLibraryMap.replaceValueAt(index, mLibrary);
    }

    // 查找入口并且调用
    CreateFactoryFunc createFactory = (CreateFactoryFunc)mLibrary->lookup(mCreateFactoryFuncName);　　//    createCasFactory
    if (createFactory == NULL || (mFactory = createFactory()) == NULL) {
        return false;
    }
    return true;
}




//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/drm/mediacas/plugins/clearkey/ClearKeyCasPlugin.cpp
android::CasFactory* createCasFactory() {
    return new android::clearkeycas::ClearKeyCasFactory();
}