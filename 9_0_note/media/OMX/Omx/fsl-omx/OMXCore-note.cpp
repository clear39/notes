/**
 *  这里是被封装在  FSLOMXPlugin 中
 *  vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/Android.mk:23:    LOCAL_MODULE:= lib_omx_core_v2_arm11_elinux
 */ 

//  @vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/OMXCore.cpp
OMX_ERRORTYPE OMXCore::OMX_Init()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	lock = NULL;
	if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
		LOG_ERROR("Create mutex for camera device failed.\n");
		return OMX_ErrorInsufficientResources;
	}

	LibMgr = FSL_NEW(ShareLibarayMgr, ());
    if(LibMgr == NULL)return OMX_ErrorInsufficientResources;

	/** Get component info of the core */
	ret = ComponentRegister();  //这里加载对应的组件
	if (ret != OMX_ErrorNone)
	{
		return ret;
	}

	/** Get content pipe info of the core */
	ret = ContentPipeRegister();
	if (ret != OMX_ErrorNone)
	{
		return ret;
	}

	return ret;
}
