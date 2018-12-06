/*
const sp<::android::hidl::manager::V1_0::IServiceManager> pm = getPassthroughServiceManager();
if (pm != nullptr) {
    Return<sp<::android::hidl::base::V1_0::IBase>> ret =  pm->get(IDevicesFactory::descriptor, serviceName);
    if (ret.isOk()) {
        sp<::android::hidl::base::V1_0::IBase> baseInterface = ret;
        if (baseInterface != nullptr) {
            iface = IDevicesFactory::castFrom(baseInterface);
            if (!getStub || trebleTestingOverride) {
                iface = new BsDevicesFactory(iface);
            }
        }
    }
}
*/


sp<IServiceManager1_0> getPassthroughServiceManager() {
    return getPassthroughServiceManager1_1();
}

sp<IServiceManager1_1> getPassthroughServiceManager1_1() {
    static sp<PassthroughServiceManager> manager(new PassthroughServiceManager());
    return manager;
}




struct PassthroughServiceManager : IServiceManager1_1 {

    static void openLibs(const std::string& fqName,
            std::function<bool /* continue */(void* /* handle */,
                const std::string& /* lib */, const std::string& /* sym */)> eachLib) {
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

        const int dlMode = RTLD_LAZY;
        void *handle = nullptr;

        dlerror(); // clear

        std::vector<std::string> paths = {HAL_LIBRARY_PATH_ODM, HAL_LIBRARY_PATH_VENDOR,
                                          HAL_LIBRARY_PATH_VNDK_SP, HAL_LIBRARY_PATH_SYSTEM};
#ifdef LIBHIDL_TARGET_DEBUGGABLE
        const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
        const bool trebleTestingOverride = env && !strcmp(env, "true");
        if (trebleTestingOverride) {
            const char* vtsRootPath = std::getenv("VTS_ROOT_PATH");
            if (vtsRootPath && strlen(vtsRootPath) > 0) {
                const std::string halLibraryPathVtsOverride =
                    std::string(vtsRootPath) + HAL_LIBRARY_PATH_SYSTEM;
                paths.push_back(halLibraryPathVtsOverride);
            }
        }
#endif
        for (const std::string& path : paths) {
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
                    LOG(ERROR) << "Failed to dlopen " << lib << ": "
                               << (error == nullptr ? "unknown error" : error);
                    continue;
                }

                if (!eachLib(handle, lib, sym)) {
                    return;
                }
            }
        }
    }

    Return<sp<IBase>> get(const hidl_string& fqName,const hidl_string& name) override {
        sp<IBase> ret = nullptr;

        openLibs(fqName, [&](void* handle, const std::string &lib, const std::string &sym) {
            IBase* (*generator)(const char* name);
            *(void **)(&generator) = dlsym(handle, sym.c_str());
            if(!generator) {
                const char* error = dlerror();
                LOG(ERROR) << "Passthrough lookup opened " << lib
                           << " but could not find symbol " << sym << ": "
                           << (error == nullptr ? "unknown error" : error);
                dlclose(handle);
                return true;
            }

            ret = (*generator)(name.c_str());

            if (ret == nullptr) {
                dlclose(handle);
                return true; // this module doesn't provide this instance name
            }

            registerReference(fqName, name);
            return false;
        });

        return ret;
    }

    Return<bool> add(const hidl_string& /* name */,const sp<IBase>& /* service */) override {
        LOG(FATAL) << "Cannot register services with passthrough service manager.";
        return false;
    }

    Return<Transport> getTransport(const hidl_string& /* fqName */,
                                   const hidl_string& /* name */) {
        LOG(FATAL) << "Cannot getTransport with passthrough service manager.";
        return Transport::EMPTY;
    }

    Return<void> list(list_cb /* _hidl_cb */) override {
        LOG(FATAL) << "Cannot list services with passthrough service manager.";
        return Void();
    }
    Return<void> listByInterface(const hidl_string& /* fqInstanceName */,
                                 listByInterface_cb /* _hidl_cb */) override {
        // TODO: add this functionality
        LOG(FATAL) << "Cannot list services with passthrough service manager.";
        return Void();
    }

    Return<bool> registerForNotifications(const hidl_string& /* fqName */,
                                          const hidl_string& /* name */,
                                          const sp<IServiceNotification>& /* callback */) override {
        // This makes no sense.
        LOG(FATAL) << "Cannot register for notifications with passthrough service manager.";
        return false;
    }

    Return<void> debugDump(debugDump_cb _hidl_cb) override {
        using Arch = ::android::hidl::base::V1_0::DebugInfo::Architecture;
        using std::literals::string_literals::operator""s;
        static std::vector<std::pair<Arch, std::vector<const char*>>> sAllPaths{
            {Arch::IS_64BIT,
             {HAL_LIBRARY_PATH_ODM_64BIT, HAL_LIBRARY_PATH_VENDOR_64BIT,
              HAL_LIBRARY_PATH_VNDK_SP_64BIT, HAL_LIBRARY_PATH_SYSTEM_64BIT}},
            {Arch::IS_32BIT,
             {HAL_LIBRARY_PATH_ODM_32BIT, HAL_LIBRARY_PATH_VENDOR_32BIT,
              HAL_LIBRARY_PATH_VNDK_SP_32BIT, HAL_LIBRARY_PATH_SYSTEM_32BIT}}};
        std::map<std::string, InstanceDebugInfo> map;
        for (const auto &pair : sAllPaths) {
            Arch arch = pair.first;
            for (const auto &path : pair.second) {
                std::vector<std::string> libs = search(path, "", ".so");
                for (const std::string &lib : libs) {
                    std::string matchedName;
                    std::string implName;
                    if (matchPackageName(lib, &matchedName, &implName)) {
                        std::string instanceName{"* ("s + path + ")"s};
                        if (!implName.empty()) instanceName += " ("s + implName + ")"s;
                        map.emplace(path + lib, InstanceDebugInfo{.interfaceName = matchedName,
                                                                  .instanceName = instanceName,
                                                                  .clientPids = {},
                                                                  .arch = arch});
                    }
                }
            }
        }
        fetchPidsForPassthroughLibraries(&map);
        hidl_vec<InstanceDebugInfo> vec;
        vec.resize(map.size());
        size_t idx = 0;
        for (auto&& pair : map) {
            vec[idx++] = std::move(pair.second);
        }
        _hidl_cb(vec);
        return Void();
    }

    Return<void> registerPassthroughClient(const hidl_string &, const hidl_string &) override {
        // This makes no sense.
        LOG(FATAL) << "Cannot call registerPassthroughClient on passthrough service manager. "
                   << "Call it on defaultServiceManager() instead.";
        return Void();
    }

    Return<bool> unregisterForNotifications(const hidl_string& /* fqName */,
                                            const hidl_string& /* name */,
                                            const sp<IServiceNotification>& /* callback */) override {
        // This makes no sense.
        LOG(FATAL) << "Cannot unregister for notifications with passthrough service manager.";
        return false;
    }

};