/**
 *  这里是被封装在  FSLOMXPlugin 中
 *  vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/Android.mk:23:    LOCAL_MODULE:= lib_omx_core_v2_arm11_elinux
 */ 

extern "C"
{
    /**< OMX core handle definition, global variable */
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

            CreatePlatformResMgr();

            LOG_DEBUG("OMXCore is Created.\n");
        }

        refCnt ++;

        return ret;
    }

    OMX_ERRORTYPE OMX_Deinit()
    {
        refCnt --;
        if(refCnt > 0)
            return OMX_ErrorNone;

        if(NULL != gCoreHandle) {
            gCoreHandle->OMX_Deinit();
            FSL_DELETE(gCoreHandle);
            gCoreHandle = NULL;
        }

        DestroyPlatformResMgr();

        LOG_DEBUG("OMXCore is Destroyed.\n");

        return OMX_ErrorNone;
    }

    OMX_ERRORTYPE OMX_ComponentNameEnum(
            OMX_STRING cComponentName,
            OMX_U32 nNameLength,
            OMX_U32 nIndex)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_ComponentNameEnum(cComponentName, nNameLength, nIndex);
    }

    OMX_ERRORTYPE OMX_GetHandle(
            OMX_HANDLETYPE *pHandle,
            OMX_STRING cComponentName,
            OMX_PTR pAppData,
            OMX_CALLBACKTYPE *pCallBacks)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_GetHandle(pHandle, cComponentName, pAppData, pCallBacks);
    }

    OMX_ERRORTYPE OMX_FreeHandle(
            OMX_HANDLETYPE hComponent)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_FreeHandle(hComponent);
    }

    OMX_ERRORTYPE OMX_GetComponentsOfRole(
            OMX_STRING role,
            OMX_U32 *pNumComps,
            OMX_U8 **compNames)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_GetComponentsOfRole(role, pNumComps, compNames);
    }

    OMX_ERRORTYPE OMX_GetRolesOfComponent(
            OMX_STRING compName,
            OMX_U32 *pNumRoles,
            OMX_U8 **roles)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_GetRolesOfComponent(compName, pNumRoles, roles);
    }

    OMX_ERRORTYPE OMX_SetupTunnel(
            OMX_HANDLETYPE hOutput,
            OMX_U32 nPortOutput,
            OMX_HANDLETYPE hInput,
            OMX_U32 nPortInput)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_SetupTunnel(hOutput, nPortOutput, hInput, nPortInput);
    }

    OMX_ERRORTYPE OMX_GetContentPipe(
            OMX_HANDLETYPE *hPipe,
            OMX_STRING szURI)
    {
        if(NULL == gCoreHandle)
            return OMX_ErrorNotReady;

        return gCoreHandle->OMX_GetContentPipe(hPipe, szURI);
    }
}




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


OMX_ERRORTYPE OMXCore::ComponentRegister()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	REG_ERRORTYPE ret_reg;
	OMX_STRING ComponentRegisterFile;
	OMX_U32 EntryIndex;
	List<REG_ENTRY> *RegEntry;
	RegistryAnalyser RegistryAnalyser;

	//	@ vendor/nxp/fsl_imx_omx/OpenMAXIL/release/registry/component_register
	ComponentRegisterFile = fsl_osal_getenv_new("COMPONENT_REGISTER_FILE");		//	@/vendor/etc/component_register
	if (ComponentRegisterFile == NULL)
	{
		LOG_WARNING("Can't get component register file.\n");
		ComponentRegisterFile = (OMX_STRING)"/etc/omx_registry/component_register";
	}

	ret_reg = RegistryAnalyser.Open(ComponentRegisterFile);
	if (ret_reg != REG_SUCCESS)
	{
		LOG_ERROR("Open component register file fail.\n");
		return OMX_ErrorUndefined;
	}

	do
	{
		RegEntry = RegistryAnalyser.GetNextEntry();
		if (RegEntry->GetNodeCnt() == 0)
		{
			LOG_DEBUG("Read register finished.\n");
			break;
		}

		COMPONENT_INFO *pComponentInfo = FSL_NEW(COMPONENT_INFO, ());
		if (pComponentInfo == NULL)
		{
			LOG_ERROR("Can't get memory.\n");
			return OMX_ErrorInsufficientResources;
		}

		fsl_osal_memset(pComponentInfo, 0, sizeof(COMPONENT_INFO));

		for (EntryIndex=0; ;EntryIndex++)
		{
			REG_ENTRY *pRegEntryItem = RegEntry->GetNode(EntryIndex);
			if (pRegEntryItem == NULL)
			{
				break;
			}
			else
			{
				if (!fsl_osal_strcmp(pRegEntryItem->name, "component_name"))
				{
					fsl_osal_strcpy((fsl_osal_char *)pComponentInfo->ComponentName,pRegEntryItem->value);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "component_role"))
				{
					ROLE_INFO *pRoleInfo = FSL_NEW(ROLE_INFO, ());
					fsl_osal_memset(pRoleInfo, 0, sizeof(ROLE_INFO));
					fsl_osal_strcpy((fsl_osal_char *)pRoleInfo->role, pRegEntryItem->value);
					pRoleInfo->priority = 3;
					EntryIndex ++;
					REG_ENTRY *pRegEntryItem = RegEntry->GetNode(EntryIndex);
					if (pRegEntryItem == NULL)
					{
                        FSL_FREE(pRoleInfo);
                        EntryIndex --;
						continue;
					}

					if (!fsl_osal_strcmp(pRegEntryItem->name, "role_priority")) //值越大优先级越高
					{
						pRoleInfo->priority = fsl_osal_atoi(pRegEntryItem->value);
					}
					else
					{
						EntryIndex --;
					}

					pComponentInfo->RoleList.Add(pRoleInfo);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "library_path")) // 组件so库路劲
				{
					fsl_osal_strcpy((fsl_osal_char *)pComponentInfo->LibName, \
							pRegEntryItem->value);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "component_entry_function")) //入口函数名
				{
					fsl_osal_strcpy((fsl_osal_char *)pComponentInfo->EntryFunction, \
							pRegEntryItem->value);
				}
				else
				{
					LOG_WARNING("Unknow register entry.\n");
				}
			}
		}
		/**
		 	@
			component_name=OMX.Freescale.std.audio_decoder.pcm.sw-based;
			library_path=lib_omx_pcm_dec_v2_arm11_elinux.so;
			component_entry_function=PcmDecInit;
			component_role=audio_decoder.pcm;
			role_priority=3;
			$
		 */
		ComponentList.Add(pComponentInfo);

	}while (1);

	ret_reg = RegistryAnalyser.Close();
	if (ret_reg != REG_SUCCESS)
	{
		LOG_ERROR("Registry analyser close fail.\n");
		return OMX_ErrorUndefined;
	}

	return ret;
}

OMX_ERRORTYPE OMXCore::ContentPipeRegister()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	REG_ERRORTYPE ret_reg;
	OMX_STRING ContentPipeRegisterFile;
	OMX_U32 EntryIndex;
	List<REG_ENTRY> *RegEntry;
	RegistryAnalyser RegistryAnalyser;

	//	@ vendor/nxp/fsl_imx_omx/OpenMAXIL/release/registry/contentpipe_register
	/**
	 	@
		content_pipe_name=LOCAL_FILE_PIPE_NEW;
		content_pipe_library_path=lib_omx_local_file_pipe_v2_arm11_elinux.so;
		content_pipe_entry_function=LocalFilePipe_Init;
		$

		@
		content_pipe_name=SHAREDFD_PIPE;
		content_pipe_library_path=lib_omx_shared_fd_pipe_arm11_elinux.so;
		content_pipe_entry_function=SharedFdPipe_Init;
		$

		@
		content_pipe_name=HTTPS_PIPE;
		content_pipe_library_path=lib_omx_https_pipe_v3_arm11_elinux.so;
		content_pipe_entry_function=HttpsPipe_Init;
		$

		@
		content_pipe_name=MMS_PIPE;
		content_pipe_library_path=lib_omx_https_pipe_v3_arm11_elinux.so;
		content_pipe_entry_function=HttpsPipe_Init;
		$

		@
		content_pipe_name=RTPS_PIPE;
		content_pipe_library_path=lib_omx_rtps_pipe_arm11_elinux.so;
		content_pipe_entry_function=RtpsPipe_Init;
		$

		@
		content_pipe_name=UDPS_PIPE;
		content_pipe_library_path=lib_omx_udps_pipe_arm11_elinux.so;
		content_pipe_entry_function=UdpsPipe_Init;
		$

		@
		content_pipe_name=ASYNC_WRITE_PIPE;
		content_pipe_library_path=lib_omx_async_write_pipe_arm11_elinux.so;
		content_pipe_entry_function=AsyncWritePipe_Init;
		$

	 */ 
	ContentPipeRegisterFile = fsl_osal_getenv_new("CONTENTPIPE_REGISTER_FILE"); //	设备中的路径	@/vendor/etc/contentpipe_register
	if (ContentPipeRegisterFile == NULL)
	{
		LOG_WARNING("Can't get content pipe register file.\n");
		ContentPipeRegisterFile = (OMX_STRING)"/etc/omx_registry/contentpipe_register";
	}

	ret_reg = RegistryAnalyser.Open(ContentPipeRegisterFile);
	if (ret_reg != REG_SUCCESS)
	{
		LOG_ERROR("Open contentpipe register file fail.\n");
		return OMX_ErrorUndefined;
	}

	do
	{
		RegEntry = RegistryAnalyser.GetNextEntry();
		if (RegEntry->GetNodeCnt() == 0)
		{
			LOG_DEBUG("Read register finished.\n");
			break;
		}

		CONTENTPIPE_INFO *pContentPipeInfo = FSL_NEW(CONTENTPIPE_INFO, ());
		if (pContentPipeInfo == NULL)
		{
			LOG_ERROR("Can't get memory.\n");
			return OMX_ErrorInsufficientResources;
		}

		fsl_osal_memset(pContentPipeInfo, 0, sizeof(CONTENTPIPE_INFO));

		for (EntryIndex=0; ;EntryIndex++)
		{
			REG_ENTRY *pRegEntryItem = RegEntry->GetNode(EntryIndex);
			if (pRegEntryItem == NULL)
			{
				break;
			}
			else
			{
				if (!fsl_osal_strcmp(pRegEntryItem->name, "content_pipe_name"))
				{
					fsl_osal_strcpy((fsl_osal_char *)pContentPipeInfo->ContentPipeName, \
							pRegEntryItem->value);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "content_pipe_library_path"))
				{
					fsl_osal_strcpy((fsl_osal_char *)pContentPipeInfo->LibName, \
							pRegEntryItem->value);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "content_pipe_entry_function"))
				{
					fsl_osal_strcpy((fsl_osal_char *)pContentPipeInfo->EntryFunction, \
							pRegEntryItem->value);
				}
				else
				{
					LOG_ERROR("Unknow register entry.\n");
				}
			}
		}

		ContentPipeList.Add(pContentPipeInfo);

	}while (1);

	ret_reg = RegistryAnalyser.Close();
	if (ret_reg != REG_SUCCESS)
	{
		LOG_ERROR("Registry analyser close fail.\n");
		return OMX_ErrorUndefined;
	}

	return ret;
}
