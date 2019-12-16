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




