//	@vendor/nxp/fsl_imx_omx/stagefright/src/OMXFSLPlugin_new.cpp
OMXPluginBase* createOMXPlugin()
{
    LOGV("createOMXPlugin");
    return (new FSLOMXPlugin());
}



class FSLOMXPlugin : public OMXPluginBase {
    public:
        FSLOMXPlugin() {
            OMX_Init();//	全局导出接口	@vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/OMXCore.cpp
        };

        virtual ~FSLOMXPlugin() {
            OMX_Deinit();
        };

        OMX_ERRORTYPE makeComponentInstance(
                const char *name,
                const OMX_CALLBACKTYPE *callbacks,
                OMX_PTR appData,
                OMX_COMPONENTTYPE **component);

        OMX_ERRORTYPE destroyComponentInstance(
                OMX_COMPONENTTYPE *component);

        OMX_ERRORTYPE enumerateComponents(
                OMX_STRING name,
                size_t size,
                OMX_U32 index);

        OMX_ERRORTYPE getRolesOfComponent(
                const char *name,
                Vector<String8> *roles);
    private:
};


//	@vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/OMXCore.cpp
OMXCore *gCoreHandle = NULL;
static OMX_S32 refCnt = 0;

OMX_ERRORTYPE OMX_Init()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(NULL == gCoreHandle) {
        gCoreHandle = FSL_NEW(OMXCore, ());
        if(NULL == gCoreHandle)
            return OMX_ErrorInsufficientResources;
        ret = gCoreHandle->OMX_Init();
        if(ret != OMX_ErrorNone) {
            FSL_DELETE(gCoreHandle);
            return ret;
        }

        CreatePlatformResMgr();		//	@fsl_imx_omx/OpenMAXIL/src/resource_mgr/PlatformResourceMgr.cpp

        LOG_DEBUG("OMXCore is Created.\n");
    }

    refCnt ++;

    return ret;
}


//	@fsl_imx_omx/OpenMAXIL/src/resource_mgr/PlatformResourceMgr.cpp
/**< C style functions to expose entry point for the shared library */
PlatformResourceMgr *gPlatformResMgr = NULL;

OMX_ERRORTYPE CreatePlatformResMgr()
{
    if(NULL == gPlatformResMgr) {
        gPlatformResMgr = FSL_NEW(PlatformResourceMgr, ());
        if(NULL == gPlatformResMgr)
            return OMX_ErrorInsufficientResources;
        gPlatformResMgr->Init();
    }

    return OMX_ErrorNone;
}