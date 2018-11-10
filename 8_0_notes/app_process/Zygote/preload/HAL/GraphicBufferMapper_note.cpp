//	@frameworks/native/libs/ui/GraphicBufferMapper.cpp
void GraphicBufferMapper::preloadHal() {
    Gralloc2::Mapper::preload();//	@frameworks/native/libs/ui/Gralloc2.cpp
}


void Mapper::preload() {
	//	@system/libhidl/transport/include/hidl/ServiceManagement.h
    android::hardware::preloadPassthroughService<hardware::graphics::mapper::V2_0::IMapper>();
}


template<typename I>
static inline void preloadPassthroughService() {
    details::preloadPassthroughService(I::descriptor);//	@system/libhidl/transport/ServiceManagement.cpp
}


void preloadPassthroughService(const std::string &descriptor) {
	//	@system/libhidl/transport/ServiceManagement.cpp
    PassthroughServiceManager::openLibs(descriptor,
        [&](void* /* handle */, const std::string& /* lib */, const std::string& /* sym */) {
            // do nothing
            return true; // open all libs
        });
}



static void PassthroughServiceManager::openLibs(const std::string& fqName,
        std::function<bool /* continue */(void* /* handle */,
            const std::string& /* lib */, const std::string& /* sym */)> eachLib) {
    //fqName looks like android.hardware.foo@1.0::IFoo
    size_t idx = fqName.find("::");

    if (idx == std::string::npos || idx + strlen("::") + 1 >= fqName.size()) {
        LOG(ERROR) << "Invalid interface name passthrough lookup: " << fqName;
        return;
    }

    std::string packageAndVersion = fqName.substr(0, idx);
    std::string ifaceName = fqName.substr(idx + strlen("::"));

    const std::string prefix = packageAndVersion + "-impl";//android.hardware.foo@1.0-impl
    const std::string sym = "HIDL_FETCH_" + ifaceName;//HIDL_FETCH_IFoo

    const int dlMode = RTLD_LAZY;
    void *handle = nullptr;

    dlerror(); // clear

    //	system/libhidl/base/include/hidl/HidlInternal.h
    /*
    #define HAL_LIBRARY_PATH_SYSTEM_64BIT "/system/lib64/hw/"
	#define HAL_LIBRARY_PATH_VENDOR_64BIT "/vendor/lib64/hw/"
	#define HAL_LIBRARY_PATH_ODM_64BIT    "/odm/lib64/hw/"
	#define HAL_LIBRARY_PATH_SYSTEM_32BIT "/system/lib/hw/"
	#define HAL_LIBRARY_PATH_VENDOR_32BIT "/vendor/lib/hw/"
	#define HAL_LIBRARY_PATH_ODM_32BIT    "/odm/lib/hw/"
    */
    std::vector<std::string> paths = {HAL_LIBRARY_PATH_ODM, HAL_LIBRARY_PATH_VENDOR,HAL_LIBRARY_PATH_SYSTEM};
#ifdef LIBHIDL_TARGET_DEBUGGABLE
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride = env && !strcmp(env, "true");
    if (trebleTestingOverride) {
        const char* vtsRootPath = std::getenv("VTS_ROOT_PATH");
        if (vtsRootPath && strlen(vtsRootPath) > 0) {
            const std::string halLibraryPathVtsOverride = std::string(vtsRootPath) + HAL_LIBRARY_PATH_SYSTEM;
            paths.push_back(halLibraryPathVtsOverride);
        }
    }
#endif
    for (const std::string& path : paths) {
    	//	$(path)/android.hardware.foo@1.0-impl.so
        std::vector<std::string> libs = search(path, prefix, ".so");

        for (const std::string &lib : libs) {
            const std::string fullPath = path + lib;

            if (path != HAL_LIBRARY_PATH_SYSTEM) {
                handle = android_load_sphal_library(fullPath.c_str(), dlMode);
            } else {
                handle = dlopen(fullPath.c_str(), dlMode);
            }

            if (handle == nullptr) {
                const char* error = dlerror();
                LOG(ERROR) << "Failed to dlopen " << lib << ": " << (error == nullptr ? "unknown error" : error);
                continue;
            }

            if (!eachLib(handle, lib, sym)) {
                return;
            }
        }
    }
}



//	@hardware/interfaces/graphics/mapper/2.0/default/GrallocMapper.cpp
IMapper* HIDL_FETCH_IMapper(const char* /* name */) {
    const hw_module_t* module = nullptr;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        ALOGE("failed to get gralloc module");
        return nullptr;
    }

    uint8_t major = (module->module_api_version >> 8) & 0xff;
    switch (major) {
        case 1:
            return new Gralloc1Mapper(module);
        case 0:
            return new Gralloc0Mapper(module);
        default:
            ALOGE("unknown gralloc module major version %d", major);
            return nullptr;
    }
}