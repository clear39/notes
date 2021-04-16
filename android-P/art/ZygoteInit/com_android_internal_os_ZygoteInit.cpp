



void android_internal_os_ZygoteInit_nativePreloadAppProcessHALs(JNIEnv* env, jclass) {
    android::GraphicBufferMapper::preloadHal();
    // Add preloading here for other HALs that are (a) always passthrough, and
    // (b) loaded by most app processes.
}



//  @	frameworks/native/libs/ui/GraphicBufferMapper.cpp
void GraphicBufferMapper::preloadHal() {
    Gralloc2::Mapper::preload();
}


// @ frameworks/native/libs/ui/Gralloc2.cpp
void Mapper::preload() {
    android::hardware::preloadPassthroughService<hardware::graphics::mapper::V2_0::IMapper>();
}



//	@ /work/workcodes/imx-p9.0.0_2.1.0-auto-ga/system/libhidl/transport/include/hidl/ServiceManagement.h
template<typename I>
static inline void preloadPassthroughService() {
    details::preloadPassthroughService(I::descriptor);
}



void preloadPassthroughService(const std::string &descriptor) {
    PassthroughServiceManager::openLibs(descriptor,
        [&](void* /* handle */, const std::string& /* lib */, const std::string& /* sym */) {
            // do nothing
            return true; // open all libs
        });
}


struct PassthroughServiceManager : IServiceManager1_1 {
	static void openLibs(
        const std::string& fqName,
        const std::function<bool /* continue */ (void* /* handle */, const std::string& /* lib */,
                                                 const std::string& /* sym */)>& eachLib) {
        //fqName looks like android.hardware.foo@1.0::IFoo
        size_t idx = fqName.find("::");

        if (idx == std::string::npos ||
                idx + strlen("::") + 1 >= fqName.size()) {
            LOG(ERROR) << "Invalid interface name passthrough lookup: " << fqName;
            return;
        }

        std::string packageAndVersion = fqName.substr(0, idx);
        std::string ifaceName = fqName.substr(idx + strlen("::"));

        const std::string prefix = packageAndVersion + "-impl";
        const std::string sym = "HIDL_FETCH_" + ifaceName;

        constexpr int dlMode = RTLD_LAZY;
        void* handle = nullptr;

        dlerror(); // clear

        static std::string halLibPathVndkSp = android::base::StringPrintf(
            HAL_LIBRARY_PATH_VNDK_SP_FOR_VERSION, details::getVndkVersionStr().c_str());
        std::vector<std::string> paths = {HAL_LIBRARY_PATH_ODM, HAL_LIBRARY_PATH_VENDOR,
                                          halLibPathVndkSp, HAL_LIBRARY_PATH_SYSTEM};

#ifdef LIBHIDL_TARGET_DEBUGGABLE
        const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
        const bool trebleTestingOverride = env && !strcmp(env, "true");
        if (trebleTestingOverride) {
            // Load HAL implementations that are statically linked
            handle = dlopen(nullptr, dlMode);
            if (handle == nullptr) {
                const char* error = dlerror();
                LOG(ERROR) << "Failed to dlopen self: "
                           << (error == nullptr ? "unknown error" : error);
            } else if (!eachLib(handle, "SELF", sym)) {
                return;
            }

            const char* vtsRootPath = std::getenv("VTS_ROOT_PATH");
            if (vtsRootPath && strlen(vtsRootPath) > 0) {
                const std::string halLibraryPathVtsOverride =
                    std::string(vtsRootPath) + HAL_LIBRARY_PATH_SYSTEM;
                paths.insert(paths.begin(), halLibraryPathVtsOverride);
            }
        }
#endif

        for (const std::string& path : paths) {
            std::vector<std::string> libs = search(path, prefix, ".so");

            for (const std::string &lib : libs) {
                const std::string fullPath = path + lib;struct PassthroughServiceManager : IServiceManager1_1 {
}
