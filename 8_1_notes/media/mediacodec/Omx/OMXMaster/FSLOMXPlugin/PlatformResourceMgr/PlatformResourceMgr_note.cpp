//	@vendor/nxp/fsl_imx_omx/OpenMAXIL/src/resource_mgr/PlatformResourceMgr.cpp
OMX_ERRORTYPE PlatformResourceMgr::Init()
{
    PlatformDataList = NULL;
    PlatformDataList = FSL_NEW(List<PLATFORM_DATA>, ());
    if(PlatformDataList == NULL)
        return OMX_ErrorInsufficientResources;

    lock = NULL;
    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
        DeInit();
        return OMX_ErrorInsufficientResources;
    }

    return OMX_ErrorNone;
}