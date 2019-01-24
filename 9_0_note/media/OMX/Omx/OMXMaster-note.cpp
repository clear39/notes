/**
 * 该类被封装在Omx中，运行进程为media.codec hw/android.hardware.media.omx@1.0-service
 */ 
//  @frameworks/av/media/libstagefright/omx/OMXMaster.cpp
OMXMaster::OMXMaster()
    : mVendorLibHandle(NULL) {

    pid_t pid = getpid();
    char filename[20];
    snprintf(filename, sizeof(filename), "/proc/%d/comm", pid);

    /**
     * autolink_8q:/ # cat /proc/1736/comm                                                                                                                                                                          
     * omx@1.0-service
     * mProcessName = "omx@1.0-service"
     */ 
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

    addVendorPlugin();              //这里添加硬件编解码组件
    addPlugin(new SoftOMXPlugin);   //这里是软实现
}

void OMXMaster::addVendorPlugin() {
    // 源码路劲 vendor/nxp/fsl_imx_omx/stagefright
    /**
     * 从 vendor/nxp/fsl_imx_omx/stagefright/Android.mk 文件中 
     * $(findstring x2.3,x$(PLATFORM_VERSION))  中  PLATFORM_VERSION（值为9），其值既不是x2.3，也为x3.
     * 
     * 所以该函数接口入口 @vendor/nxp/fsl_imx_omx/stagefright/src/OMXFSLPlugin_new.cpp
     */ 
    addPlugin("libstagefrighthw.so");       
}

void OMXMaster::addPlugin(const char *libname) {
    mVendorLibHandle = android_load_sphal_library(libname, RTLD_NOW);

    if (mVendorLibHandle == NULL) {
        return;
    }

    typedef OMXPluginBase *(*CreateOMXPluginFunc)();
    CreateOMXPluginFunc createOMXPlugin = (CreateOMXPluginFunc)dlsym(mVendorLibHandle, "createOMXPlugin");
    if (!createOMXPlugin)
        createOMXPlugin = (CreateOMXPluginFunc)dlsym(mVendorLibHandle, "_ZN7android15createOMXPluginEv");

    if (createOMXPlugin) {
        //  该函数接口入口 @vendor/nxp/fsl_imx_omx/stagefright/src/OMXFSLPlugin_new.cpp
        addPlugin((*createOMXPlugin)()); //这里创建 FSLOMXPlugin 组件 （源码路劲 vendor/nxp/fsl_imx_omx/stagefright）
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
            ALOGE("A component of name '%s' already exists, ignoring this one.",name8.string());

            continue;
        }

        mPluginByComponentName.add(name8, plugin);
    }

    if (err != OMX_ErrorNoMore) {
        ALOGE("OMX plugin failed w/ error 0x%08x after registering %zu " "components", err, mPluginByComponentName.size());
    }
}