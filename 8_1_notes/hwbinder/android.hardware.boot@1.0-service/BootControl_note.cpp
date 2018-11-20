//	@hardware/interfaces/boot/1.0/default/BootControl.cpp


IBootControl* HIDL_FETCH_IBootControl(const char* /* hal */) {
    int ret = 0;
    boot_control_module_t* module = NULL;
    hw_module_t **hwm = reinterpret_cast<hw_module_t**>(&module);
    //@hardware/libhardware/include/hardware/boot_control.h:29:#define BOOT_CONTROL_HARDWARE_MODULE_ID "bootctrl"
    ret = hw_get_module(BOOT_CONTROL_HARDWARE_MODULE_ID, const_cast<const hw_module_t**>(hwm));
    if (ret)
    {
        ALOGE("hw_get_module %s failed: %d", BOOT_CONTROL_HARDWARE_MODULE_ID, ret);
        return nullptr;
    }
    module->init(module);
    return new BootControl(module);
}


BootControl::BootControl(boot_control_module_t *module) : mModule(module){

}




//	@hardware/qcom/bootctrl/boot_control.cpp

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
	AB_SLOT_A_SUFFIX,//	@hardware/qcom/msm8998/gpt-utils/gpt-utils.h:79:#define AB_SLOT_A_SUFFIX                "_a"
	AB_SLOT_B_SUFFIX,//@hardware/qcom/msm8998/gpt-utils/gpt-utils.h:80:#define AB_SLOT_B_SUFFIX                "_b"
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