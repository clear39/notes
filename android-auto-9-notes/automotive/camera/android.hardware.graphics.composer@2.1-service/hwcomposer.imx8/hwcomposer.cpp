//  @   /work/workcodes/aosp-p9.x-auto-ga/vendor/nxp-opensource/imx/display/hwcomposer_v20/hwcomposer.cpp
hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG, // @ hardware/libhardware/include/hardware/hardware.h:34:#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
    .version_major = 3,
    .version_minor = 0,
    .id = HWC_HARDWARE_MODULE_ID,  // @  hardware/libhardware/include/hardware/hwcomposer_defs.h:47:#define HWC_HARDWARE_MODULE_ID "hwcomposer"
    .name = "Freescale i.MX hwcomposer module",
    .author = "Freescale Semiconductor, Inc.",
    .methods = &hwc_module_methods,
    .dso = NULL,
    .reserved = {0}
};

static struct hw_module_methods_t hwc_module_methods = {
    .open = hwc_device_open
};

static int hwc_device_open(const struct hw_module_t* module, const char* name,struct hw_device_t** device)
{
    int status = -EINVAL;
    struct hwc2_context_t *dev = NULL;
    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        return status;
    }

    dev = (hwc2_context_t*)malloc(sizeof(*dev));

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));
    /* initialize the procs */
    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.module = const_cast<hw_module_t*>(module);
    dev->device.common.close = hwc_device_close;
    dev->device.common.version = HWC_DEVICE_API_VERSION_2_0;  // 0x02000001
    dev->device.getCapabilities = hwc_get_capabilities;
    dev->device.getFunction = hwc_get_function;

    dev->mListener = new DisplayListener(dev);
    dev->checkHDMI = true;
    dev->color_tranform = false;

    *device = &dev->device.common;
    ALOGI("%s,%d", __FUNCTION__, __LINE__);
    return 0;
}