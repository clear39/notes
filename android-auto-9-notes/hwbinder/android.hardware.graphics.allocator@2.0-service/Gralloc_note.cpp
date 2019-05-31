//@hardware/interfaces/graphics/allocator/2.0/default/Gralloc.cpp
IAllocator* HIDL_FETCH_IAllocator(const char* /* name */) {
    const hw_module_t* module = nullptr;
/*
hw_get_module会在以下目录查找对应的库文件,进行加载
#if defined(__LP64__)
#define HAL_LIBRARY_PATH1 "/system/lib64/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib64/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib64/hw"
#else
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib/hw"
#endif
*/

    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);//由上面可以得到加载的gralloc.imx8.so
    if (err) {
        ALOGE("failed to get gralloc module");
        return nullptr;
    }

    uint8_t major = (module->module_api_version >> 8) & 0xff;
    switch (major) {
        case 1:
            return new Gralloc1Allocator(module);
        case 0:
            return new Gralloc0Allocator(module);
        default:
            ALOGE("unknown gralloc module major version %d", major);
            return nullptr;
    }
}