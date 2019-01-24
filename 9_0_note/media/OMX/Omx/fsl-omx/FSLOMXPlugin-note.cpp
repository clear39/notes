//  @vendor/nxp/fsl_imx_omx/stagefright/src/OMXFSLPlugin_new.cpp

// 创建入口
extern "C" {
    OMXPluginBase* createOMXPlugin()
    {
        LOGV("createOMXPlugin");
        return (new FSLOMXPlugin());
    }
}


FSLOMXPlugin::FSLOMXPlugin() {
    /**
     * 这里调用的是vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/OMXCore.cpp
     */
    OMX_Init();//  vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/Android.mk:23:    LOCAL_MODULE:= lib_omx_core_v2_arm11_elinux
};


OMX_ERRORTYPE FSLOMXPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(name == NULL)
        return OMX_ErrorBadParameter;

    FSLOMXWrapper *pWrapper = new FSLOMXWrapper(name);
    if(pWrapper == NULL)
        return OMX_ErrorInsufficientResources;

    OMX_HANDLETYPE handle = NULL;
    ret = OMX_GetHandle(&handle, (char*)name, (OMX_PTR)pWrapper, (OMX_CALLBACKTYPE*)&pWrapper->wrapperCallback);
    if(ret != OMX_ErrorNone){
        delete pWrapper;
        return ret;
    }

    *component = pWrapper->MakeWapper(handle);
    if(*component == NULL){
        delete pWrapper;
        return OMX_ErrorUndefined;
    }

    pWrapper->BackupCallbacks((OMX_CALLBACKTYPE*)callbacks, appData);

    LOGV("makeComponentInstance done, instance is: %p", *component);

    return OMX_ErrorNone;
}







