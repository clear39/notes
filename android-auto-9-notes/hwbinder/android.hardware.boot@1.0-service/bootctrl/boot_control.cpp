

//  @   hardware/qcom/bootctrl/boot_control.cpp

static hw_module_methods_t boot_control_module_methods = {
	.open = NULL,
};

boot_control_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = 1,
		.hal_api_version = 0,
		.id = BOOT_CONTROL_HARDWARE_MODULE_ID,
		.name = "Boot control HAL",
		.author = "Code Aurora Forum",
		.methods = &boot_control_module_methods,
	},
	.init = boot_control_init,
	.getNumberSlots = get_number_slots,	//	Return<uint32_t> BootControl::getNumberSlots()
	.getCurrentSlot = get_current_slot,		//	Return<uint32_t> BootControl::getCurrentSlot()
	.markBootSuccessful = mark_boot_successful,	//Return<void> BootControl::markBootSuccessful(markBootSuccessful_cb _hidl_cb)
	.setActiveBootSlot = set_active_boot_slot,			//Return<void> BootControl::setActiveBootSlot(uint32_t slot, setActiveBootSlot_cb _hidl_cb)
	.setSlotAsUnbootable = set_slot_as_unbootable,	//Return<void> BootControl::setSlotAsUnbootable(uint32_t slot, setSlotAsUnbootable_cb _hidl_cb) 
	.isSlotBootable = is_slot_bootable,	//Return<BoolResult> BootControl::isSlotBootable(uint32_t slot) 
	.getSuffix = get_suffix,//Return<void> BootControl::getSuffix(uint32_t slot, getSuffix_cb _hidl_cb) 	
	.isSlotMarkedSuccessful = is_slot_marked_successful,//Return<BoolResult> BootControl::isSlotMarkedSuccessful(uint32_t slot)
};



void boot_control_init(struct boot_control_module *module)
{
	if (!module) {
		ALOGE("Invalid argument passed to %s", __func__);
		return;
	}
	return;
}


unsigned get_number_slots(struct boot_control_module *module)
{
	struct dirent *de = NULL;
	DIR *dir_bootdev = NULL;
	unsigned slot_count = 0;
	if (!module) {
		ALOGE("%s: Invalid argument", __func__);
		goto error;
	}
	dir_bootdev = opendir(BOOTDEV_DIR);//	#define BOOTDEV_DIR "/dev/block/bootdevice/by-name"
	if (!dir_bootdev) {
		ALOGE("%s: Failed to open bootdev dir (%s)",__func__,strerror(errno));
		goto error;
	}
	while ((de = readdir(dir_bootdev))) {
		if (de->d_name[0] == '.')
			continue;
		if (!strncmp(de->d_name, BOOT_IMG_PTN_NAME, strlen(BOOT_IMG_PTN_NAME)))//#define BOOT_IMG_PTN_NAME "boot"
			slot_count++;
	}
	closedir(dir_bootdev);
	return slot_count;
error:
	if (dir_bootdev)
		closedir(dir_bootdev);
	return 0;
}


const char *slot_suffix_arr[] = {
	AB_SLOT_A_SUFFIX,//	    @   hardware/qcom/msm8998/gpt-utils/gpt-utils.h:79:#define AB_SLOT_A_SUFFIX                "_a"
	AB_SLOT_B_SUFFIX,// @   hardware/qcom/msm8998/gpt-utils/gpt-utils.h:80:#define AB_SLOT_B_SUFFIX                "_b"
	NULL};

unsigned get_current_slot(struct boot_control_module *module)
{
	uint32_t num_slots = 0;
	char bootSlotProp[PROPERTY_VALUE_MAX] = {'\0'};
	unsigned i = 0;
	if (!module) {
		ALOGE("%s: Invalid argument", __func__);
		goto error;
	}
	num_slots = get_number_slots(module);
	if (num_slots <= 1) {
		//Slot 0 is the only slot around.
		return 0;
	}
	property_get(BOOT_SLOT_PROP, bootSlotProp, "N/A");//	#define BOOT_SLOT_PROP "ro.boot.slot_suffix"
	if (!strncmp(bootSlotProp, "N/A", strlen("N/A"))) {
		ALOGE("%s: Unable to read boot slot property",__func__);
		goto error;
	}
	//Iterate through a list of partitons named as boot+suffix
	//and see which one is currently active.
	for (i = 0; slot_suffix_arr[i] != NULL ; i++) {
		if (!strncmp(bootSlotProp,slot_suffix_arr[i],strlen(slot_suffix_arr[i])))
				return i;
	}
error:
	//The HAL spec requires that we return a number between
	//0 to num_slots - 1. Since something went wrong here we
	//are just going to return the default slot.
	return 0;
}


int mark_boot_successful(struct boot_control_module *module)
{
	unsigned cur_slot = 0;
	if (!module) {
		ALOGE("%s: Invalid argument", __func__);
		goto error;
	}
	cur_slot = get_current_slot(module);
	if (update_slot_attribute(slot_suffix_arr[cur_slot],ATTR_BOOT_SUCCESSFUL)) {
		goto error;
	}
	return 0;
error:
	ALOGE("%s: Failed to mark boot successful", __func__);
	return -1;
}

//Set a particular attribute for all the partitions in a
//slot
static int update_slot_attribute(const char *slot,enum part_attr_type ab_attr)
{
	unsigned int i = 0;
	char buf[PATH_MAX];
	struct stat st;
	struct gpt_disk *disk = NULL;
	uint8_t *pentry = NULL;
	uint8_t *pentry_bak = NULL;
	int rc = -1;
	uint8_t *attr = NULL;
	uint8_t *attr_bak = NULL;
	char partName[MAX_GPT_NAME_SIZE + 1] = {0};
	const char ptn_list[][MAX_GPT_NAME_SIZE] = { AB_PTN_LIST };
	int slot_name_valid = 0;
	if (!slot) {
		ALOGE("%s: Invalid argument", __func__);
		goto error;
	}
	for (i = 0; slot_suffix_arr[i] != NULL; i++)
	{
		if (!strncmp(slot, slot_suffix_arr[i],strlen(slot_suffix_arr[i])))
				slot_name_valid = 1;
	}
	if (!slot_name_valid) {
		ALOGE("%s: Invalid slot name", __func__);
		goto error;
	}
	for (i=0; i < ARRAY_SIZE(ptn_list); i++) {
		memset(buf, '\0', sizeof(buf));
		//Check if A/B versions of this ptn exist
		snprintf(buf, sizeof(buf) - 1,"%s/%s%s",BOOT_DEV_DIR,ptn_list[i],AB_SLOT_A_SUFFIX);
		if (stat(buf, &st)) {
			//partition does not have _a version
			continue;
		}
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf) - 1,"%s/%s%s",BOOT_DEV_DIR,ptn_list[i],AB_SLOT_B_SUFFIX);
		if (stat(buf, &st)) {
			//partition does not have _a version
			continue;
		}
		memset(partName, '\0', sizeof(partName));
		snprintf(partName,
				sizeof(partName) - 1,
				"%s%s",
				ptn_list[i],
				slot);
		disk = gpt_disk_alloc();
		if (!disk) {
			ALOGE("%s: Failed to alloc disk struct",
					__func__);
			goto error;
		}
		rc = gpt_disk_get_disk_info(partName, disk);
		if (rc != 0) {
			ALOGE("%s: Failed to get disk info for %s",
					__func__,
					partName);
			goto error;
		}
		pentry = gpt_disk_get_pentry(disk, partName, PRIMARY_GPT);
		pentry_bak = gpt_disk_get_pentry(disk, partName, SECONDARY_GPT);
		if (!pentry || !pentry_bak) {
			ALOGE("%s: Failed to get pentry/pentry_bak for %s",__func__,partName);
			goto error;
		}
		attr = pentry + AB_FLAG_OFFSET;
		attr_bak = pentry_bak + AB_FLAG_OFFSET;
		if (ab_attr == ATTR_BOOT_SUCCESSFUL) {
			*attr = (*attr) | AB_PARTITION_ATTR_BOOT_SUCCESSFUL;
			*attr_bak = (*attr_bak) | AB_PARTITION_ATTR_BOOT_SUCCESSFUL;
		} else if (ab_attr == ATTR_UNBOOTABLE) {
			*attr = (*attr) | AB_PARTITION_ATTR_UNBOOTABLE;
			*attr_bak = (*attr_bak) | AB_PARTITION_ATTR_UNBOOTABLE;
		} else if (ab_attr == ATTR_SLOT_ACTIVE) {
			*attr = (*attr) | AB_PARTITION_ATTR_SLOT_ACTIVE;
			*attr_bak = (*attr) | AB_PARTITION_ATTR_SLOT_ACTIVE;
		} else {
			ALOGE("%s: Unrecognized attr", __func__);
			goto error;
		}
		if (gpt_disk_update_crc(disk)) {
			ALOGE("%s: Failed to update crc for %s",__func__,partName);
			goto error;
		}
		if (gpt_disk_commit(disk)) {
			ALOGE("%s: Failed to write back entry for %s",__func__,partName);
			goto error;
		}
		gpt_disk_free(disk);
		disk = NULL;
	}
	return 0;
error:
	if (disk)
		gpt_disk_free(disk);
	return -1;
}
