

//  @  hardware/interfaces/graphics/composer/2.1/utils/passthrough/include/composer-passthrough/2.1/HwcLoader.h
static IComposer* HwcLoader::load() {
    const hw_module_t* module = loadModule();
    if (!module) {
        return nullptr;
    }

    auto hal = createHalWithAdapter(module);
    if (!hal) {
        return nullptr;
    }

    return createComposer(std::move(hal));
}

// load hwcomposer2 module
static const hw_module_t* HwcLoader::loadModule() {
    const hw_module_t* module;
    /**
     * hardware/libhardware/include/hardware/hwcomposer_defs.h:47:#define HWC_HARDWARE_MODULE_ID "hwcomposer"
     * 
     * 这里　加载　vendor/lib64/hw/hwcomposer.imx8.so
     * 
     * 由于其中TARGET_HWCOMPOSER_VERSION的值为v2.0;
     * 所以　该so 源码路径　vendor/nxp-opensource/imx/display/hwcomposer_v20
     */
    int error = hw_get_module(HWC_HARDWARE_MODULE_ID, &module);
    if (error) {
        ALOGI("falling back to gralloc module");
        error = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    }

    if (error) {
        ALOGE("failed to get hwcomposer or gralloc module");
        return nullptr;
    }

    return module;
}

// create a ComposerHal instance, insert an adapter if necessary
static std::unique_ptr<hal::ComposerHal> createHalWithAdapter(const hw_module_t* module) {
    bool adapted;
    hwc2_device_t* device = openDeviceWithAdapter(module, &adapted);
    if (!device) {
        return nullptr;
    }
    auto hal = std::make_unique<HwcHal>();
    return hal->initWithDevice(std::move(device), !adapted) ? std::move(hal) : nullptr;
}


 // open hwcomposer2 device, install an adapter if necessary
static hwc2_device_t* openDeviceWithAdapter(const hw_module_t* module, bool* outAdapted) {
    if (module->id && std::string(module->id) == GRALLOC_HARDWARE_MODULE_ID) {
        *outAdapted = true;
        return adaptGrallocModule(module);
    }

    hw_device_t* device;
    int error = module->methods->open(module, HWC_HARDWARE_COMPOSER, &device);
    if (error) {
        ALOGE("failed to open hwcomposer device: %s", strerror(-error));
        return nullptr;
    }

    int major = (device->version >> 24) & 0xf;
    if (major != 2) {
        *outAdapted = true;
        return adaptHwc1Device(std::move(reinterpret_cast<hwc_composer_device_1*>(device)));
    }

    *outAdapted = false;
    return reinterpret_cast<hwc2_device_t*>(device);
}