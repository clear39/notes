//	@frameworks/av/media/libstagefright/omx/OMXMaster.cpp
OMXMaster::OMXMaster()
    : mVendorLibHandle(NULL) {//	void *mVendorLibHandle;

    pid_t pid = getpid();
    char filename[20];
    snprintf(filename, sizeof(filename), "/proc/%d/comm", pid);
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
      ALOGW("couldn't determine process name");
      strlcpy(mProcessName, "<unknown>", sizeof(mProcessName));
    } else {
      ssize_t len = read(fd, mProcessName, sizeof(mProcessName));
      if (len < 2) {
        ALOGW("couldn't determine process name");
        strlcpy(mProcessName, "<unknown>", sizeof(mProcessName));
      } else {
        // the name is newline terminated, so erase the newline
        mProcessName[len - 1] = 0;
      }
      close(fd);
    }

    addVendorPlugin();
    addPlugin(new SoftOMXPlugin);
}



void OMXMaster::addVendorPlugin() {
    addPlugin("libstagefrighthw.so");
}


void OMXMaster::addPlugin(const char *libname) {
    mVendorLibHandle = dlopen(libname, RTLD_NOW);

    if (mVendorLibHandle == NULL) {
        return;
    }

    typedef OMXPluginBase *(*CreateOMXPluginFunc)();
    CreateOMXPluginFunc createOMXPlugin = (CreateOMXPluginFunc)dlsym(mVendorLibHandle, "createOMXPlugin");
    if (!createOMXPlugin)
        createOMXPlugin = (CreateOMXPluginFunc)dlsym(mVendorLibHandle, "_ZN7android15createOMXPluginEv");

    if (createOMXPlugin) {
        addPlugin((*createOMXPlugin)());
    }
}


void OMXMaster::addPlugin(OMXPluginBase *plugin) {
    Mutex::Autolock autoLock(mLock);

    mPlugins.push_back(plugin);

    OMX_U32 index = 0;

    char name[128];
    OMX_ERRORTYPE err;
    while ((err = plugin->enumerateComponents(name, sizeof(name), index++)) == OMX_ErrorNone) {
        String8 name8(name);

        if (mPluginByComponentName.indexOfKey(name8) >= 0) {
            ALOGE("A component of name '%s' already exists, ignoring this one.", name8.string());

            continue;
        }

        mPluginByComponentName.add(name8, plugin);
    }

    if (err != OMX_ErrorNoMore) {
        ALOGE("OMX plugin failed w/ error 0x%08x after registering %zu " "components", err, mPluginByComponentName.size());
    }
}