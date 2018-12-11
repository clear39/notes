//	@vendor/nxp/fsl_imx_omx/OpenMAXIL/src/core/OMXCore.cpp
OMX_ERRORTYPE OMXCore::OMX_Init()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	lock = NULL;
	if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
		LOG_ERROR("Create mutex for camera device failed.\n");
		return OMX_ErrorInsufficientResources;
	}

	LibMgr = FSL_NEW(ShareLibarayMgr, ());// ShareLibarayMgr 没有重载构造函数
        if(LibMgr == NULL)
            return OMX_ErrorInsufficientResources;

	/** Get component info of the core */
	ret = ComponentRegister();
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

	ComponentRegisterFile = fsl_osal_getenv_new("COMPONENT_REGISTER_FILE");//	/vendor/etc/component_register
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
					fsl_osal_strcpy((fsl_osal_char *)pComponentInfo->ComponentName, \
							pRegEntryItem->value);
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

					if (!fsl_osal_strcmp(pRegEntryItem->name, "role_priority"))
					{
						pRoleInfo->priority = fsl_osal_atoi(pRegEntryItem->value);
					}
					else
					{
						EntryIndex --;
					}

					pComponentInfo->RoleList.Add(pRoleInfo);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "library_path"))
				{
					fsl_osal_strcpy((fsl_osal_char *)pComponentInfo->LibName, \
							pRegEntryItem->value);
				}
				else if (!fsl_osal_strcmp(pRegEntryItem->name, "component_entry_function"))
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

	ContentPipeRegisterFile = fsl_osal_getenv_new("CONTENTPIPE_REGISTER_FILE");	//	/vendor/etc/contentpipe_register
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