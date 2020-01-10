
//  @   hardware/interfaces/graphics/allocator/2.0/utils/passthrough/include/allocator-passthrough/2.0/GrallocLoader.h

static IAllocator*GrallocLoader::load() {
    const hw_module_t* module = loadModule();
    if (!module) {
        return nullptr;
    }
    auto hal = createHal(module);
    if (!hal) {
        return nullptr;
    }
    return createAllocator(std::move(hal));
}



// load the gralloc module
static const hw_module_t* GrallocLoader::loadModule() {
    const hw_module_t* module;
    //  @   hardware/libhardware/include/hardware/gralloc.h:60:#define GRALLOC_HARDWARE_MODULE_ID "gralloc"
    /**
     * @    device/autolink/imx8q/BoardConfigCommon.mk:13:   TARGET_GRALLOC_VERSION := v3
     * 
     * 对应目录 vendor/nxp-opensource/imx/display/gralloc_v3
    */
    int error = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (error) {
        ALOGE("failed to get gralloc module");
        return nullptr;
    }

    return module;
}


 // create an AllocatorHal instance
static std::unique_ptr<hal::AllocatorHal> GrallocLoader::createHal(const hw_module_t* module) {
    /***
     * hardware/libhardware/include/hardware/hardware.h:111:    uint16_t module_api_version;
    * hardware/libhardware/include/hardware/hardware.h:112:     #define version_major module_api_version
    * 这里返回1.0
     */
    int major = getModuleMajorApiVersion(module);
    switch (major) {
        case 1: {
            //  using Gralloc1Hal = detail::Gralloc1HalImpl<hal::AllocatorHal>;
            auto hal = std::make_unique<Gralloc1Hal>();
            return hal->initWithModule(module) ? std::move(hal) : nullptr;
        }
        case 0: {
            //  using Gralloc0Hal = detail::Gralloc0HalImpl<hal::AllocatorHal>;
            auto hal = std::make_unique<Gralloc0Hal>();
            return hal->initWithModule(module) ? std::move(hal) : nullptr;
        }
        default:
            ALOGE("unknown gralloc module major version %d", major);
            return nullptr;
    }
}


// return the major api version of the module
static int GrallocLoader::getModuleMajorApiVersion(const hw_module_t* module) {
    return (module->module_api_version >> 8) & 0xff;
}



// create an IAllocator instance
static IAllocator*  GrallocLoader::createAllocator(std::unique_ptr<hal::AllocatorHal> hal) {
    auto allocator = std::make_unique<hal::Allocator>();
    return allocator->init(std::move(hal)) ? allocator.release() : nullptr;
}
