

//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/drm/drmserver/PlugInManager.h

const char* const PLUGIN_MANAGER_CREATE = "create";
const char* const PLUGIN_MANAGER_DESTROY = "destroy";
const char* const PLUGIN_EXTENSION = ".so";



/**
 * Load a plug-in stored in the specified path
 *
 * @param[in] rsPlugInPath
 *     Plug-in (dynamic library) file path
 * @note Plug-in should be implemented according to the specification
 */
void TPlugInManager::loadPlugIn(const String8& rsPlugInPath) {
    if (contains(rsPlugInPath)) {
        return;
    }

    PlugInContainer* pPlugInContainer = new PlugInContainer();

    pPlugInContainer->hHandle = dlopen(rsPlugInPath.string(), RTLD_LAZY);

    if (NULL == pPlugInContainer->hHandle) {
        delete pPlugInContainer;
        pPlugInContainer = NULL;
        return;
    }

    pPlugInContainer->sPath = rsPlugInPath;
    pPlugInContainer->fpCreate   = (FPCREATE)dlsym(pPlugInContainer->hHandle, PLUGIN_MANAGER_CREATE);
    pPlugInContainer->fpDestory   = (FPDESTORY)dlsym(pPlugInContainer->hHandle, PLUGIN_MANAGER_DESTROY);

    if (NULL != pPlugInContainer->fpCreate && NULL != pPlugInContainer->fpDestory) {
        pPlugInContainer->pInstance = (Type*)pPlugInContainer->fpCreate();
        m_plugInIdList.add(rsPlugInPath);
        m_plugInMap.add(rsPlugInPath, pPlugInContainer);
    } else {
        dlclose(pPlugInContainer->hHandle);
        delete pPlugInContainer;
        pPlugInContainer = NULL;
        return;
    }
}


/**
 * Return file path list of plug-ins stored in the specified directory
 *
 * @param[in] rsDirPath
 *     Directory path in which plug-ins are stored
 * @return plugInFileList
 *     String type Vector in which file path of plug-ins are stored
 */
Vector<String8> TPlugInManager::getPlugInPathList(const String8& rsDirPath) {
    Vector<String8> fileList;
    DIR* pDir = opendir(rsDirPath.string());
    struct dirent* pEntry;

    while (NULL != pDir && NULL != (pEntry = readdir(pDir))) {
        if (!isPlugIn(pEntry)) {
            continue;
        }
        String8 plugInPath;
        plugInPath += rsDirPath;
        plugInPath += "/";
        plugInPath += pEntry->d_name;

        fileList.add(plugInPath);
    }

    if (NULL != pDir) {
        closedir(pDir);
    }

    return fileList;
}