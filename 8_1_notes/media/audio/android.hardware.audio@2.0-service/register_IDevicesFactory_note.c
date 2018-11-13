/*
   android::status_t status;
   status = registerPassthroughServiceImplementation<IDevicesFactory>();
*/

//	@system/libhidl/transport/include/hidl/LegacySupport.h
/**
 * Registers passthrough service implementation.
 */
template<class Interface> //    Interface == IDevicesFactory 
__attribute__((warn_unused_result))
status_t registerPassthroughServiceImplementation(std::string name = "default") {
    //这一步的目的
    sp<Interface> service = Interface::getService(name, true /* getStub */);//  sp<IDevicesFactory> service = IDevicesFactory::getService("default", true /* getStub */);

    if (service == nullptr) {
        ALOGE("Could not get passthrough implementation for %s/%s.",Interface::descriptor, name.c_str());
        return EXIT_FAILURE;
    }

    //  IDevicesFactory->isRemote() 为 false //  @out/soong/.intermediates/hardware/interfaces/audio/2.0/android.hardware.audio@2.0_genc++_headers/gen/android/hardware/audio/2.0/IDevicesFactory.h
    LOG_FATAL_IF(service->isRemote(), "Implementation of %s/%s is remote!",Interface::descriptor, name.c_str());

    status_t status = service->registerAsService(name);//这里不用分析了,在rild中已经分析过了

    if (status == OK) {
        ALOGI("Registration complete for %s/%s.",Interface::descriptor, name.c_str());
    } else {
        ALOGE("Could not register service %s/%s (%d).",Interface::descriptor, name.c_str(), status);
    }

    return status;
}

// @out/soong/.intermediates/hardware/interfaces/audio/2.0/android.hardware.audio@2.0_genc++/gen/android/hardware/audio/2.0/DevicesFactoryAll.cpp
//  sp<IDevicesFactory> service = IDevicesFactory::getService("default", true /* getStub */);
// static
::android::sp<IDevicesFactory> IDevicesFactory::getService(const std::string &serviceName = "default",, const bool getStub = true) {
    using ::android::hardware::defaultServiceManager;
    using ::android::hardware::details::waitForHwService;
    using ::android::hardware::getPassthroughServiceManager;
    using ::android::hardware::Return;
    using ::android::sp;
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;

    sp<IDevicesFactory> iface = nullptr;


    //  sm = new BpHwServiceManager(BpHwBinder(0)));
    const sp<::android::hidl::manager::V1_0::IServiceManager> sm = defaultServiceManager();// system/libhidl/transport/ServiceManagement.cpp:135
    if (sm == nullptr) {
        ALOGE("getService: defaultServiceManager() is null");
        return nullptr;
    }

    //  const char* IDevicesFactory::descriptor("android.hardware.audio@2.0::IDevicesFactory"); //@out/soong/.intermediates/hardware/interfaces/audio/2.0/android.hardware.audio@2.0_genc++/gen/android/hardware/audio/2.0/DevicesFactoryAll.cpp
    // getTransport 函数只是确认在/vendor/manifest.xml或者/system/manifest.xml下是否已经配置好
    Return<Transport> transportRet = sm->getTransport(IDevicesFactory::descriptor, serviceName);

    if (!transportRet.isOk()) {
        ALOGE("getService: defaultServiceManager()->getTransport returns %s", transportRet.description().c_str());
        return nullptr;
    }
    Transport transport = transportRet;
    const bool vintfHwbinder = (transport == Transport::HWBINDER);
    const bool vintfPassthru = (transport == Transport::PASSTHROUGH);

#ifdef __ANDROID_TREBLE__

#ifdef __ANDROID_DEBUGGABLE__
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY) && trebleTestingOverride;
#else // __ANDROID_TREBLE__ but not __ANDROID_DEBUGGABLE__
    const bool trebleTestingOverride = false;
    const bool vintfLegacy = false;
#endif // __ANDROID_DEBUGGABLE__

#else // not __ANDROID_TREBLE__
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY);

#endif // __ANDROID_TREBLE__

    // 由于getStub = true 所以下面for循环的代码 不执行
    for (int tries = 0; !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)); tries++) { //getStub = true
        ......
    }



    if (getStub || vintfPassthru || vintfLegacy) { // getStub = true
        const sp<::android::hidl::manager::V1_0::IServiceManager> pm = getPassthroughServiceManager();
        if (pm != nullptr) {
            Return<sp<::android::hidl::base::V1_0::IBase>> ret = pm->get(IDevicesFactory::descriptor, serviceName);
            if (ret.isOk()) {
                sp<::android::hidl::base::V1_0::IBase> baseInterface = ret;//new DevicesFactory();
                if (baseInterface != nullptr) {
                    iface = IDevicesFactory::castFrom(baseInterface);
                    if (!getStub || trebleTestingOverride) {// getStub = true
                        iface = new BsDevicesFactory(iface);
                    }
                }
            }
        }
    }
    return iface;
}



sp<IServiceManager> getPassthroughServiceManager() {
    // struct PassthroughServiceManager : IServiceManager1_1 { }    @system/libhidl/transport/ServiceManagement.cpp:256
    static sp<PassthroughServiceManager> manager(new PassthroughServiceManager());
    return manager;
}



//	Return<sp<::android::hidl::base::V1_0::IBase>> ret = pm->get(IDevicesFactory::descriptor, serviceName);   //	serviceName == "default"
//	system/libhidl/transport/ServiceManagement.cpp
Return<sp<IBase>> PassthroughServiceManager::get(const hidl_string& fqName = "android.hardware.audio@2.0::IDevicesFactory",const hidl_string& name = "default") override {
	sp<IBase> ret = nullptr;

	openLibs(fqName, [&](void* handle, const std::string &lib, const std::string &sym) {// sym == HIDL_FETCH_IDevicesFactory
	    IBase* (*generator)(const char* name);
	    *(void **)(&generator) = dlsym(handle, sym.c_str());
	    if(!generator) {
    		const char* error = dlerror();
    		LOG(ERROR) << "Passthrough lookup opened " << lib << " but could not find symbol " << sym << ": " << (error == nullptr ? "unknown error" : error);
    		dlclose(handle);
    		return true;
	    }

	    ret = (*generator)(name.c_str());

	    if (ret == nullptr) {
    		dlclose(handle);
    		return true; // this module doesn't provide this instance name
	    }

	    registerReference(fqName, name); // fqName == android.hardware.audio@2.0::IDevicesFactory // name == "default"
	    return false;
	});

	return ret; //new DevicesFactory();
}


static void PassthroughServiceManager::openLibs(const std::string& fqName, std::function<bool /* continue */(void* /* handle */,const std::string& /* lib */, const std::string& /* sym */)> eachLib) {
    //fqName looks like android.hardware.foo@1.0::IFoo
    size_t idx = fqName.find("::");

    if (idx == std::string::npos ||  idx + strlen("::") + 1 >= fqName.size()) {
        LOG(ERROR) << "Invalid interface name passthrough lookup: " << fqName;
        return;
    }

    std::string packageAndVersion = fqName.substr(0, idx);//    packageAndVersion= "android.hardware.audio@2.0"
    std::string ifaceName = fqName.substr(idx + strlen("::"));  //  ifaceName = "IDevicesFactory"

    const std::string prefix = packageAndVersion + "-impl"; //  prefix= "android.hardware.audio@2.0-impl"
    const std::string sym = "HIDL_FETCH_" + ifaceName;//    sym = "HIDL_FETCH_IDevicesFactory"

    const int dlMode = RTLD_LAZY;
    void *handle = nullptr;

    dlerror(); // clear

    /**
    system/libhidl/base/include/hidl/HidlInternal.h:94:#define HAL_LIBRARY_PATH_ODM_64BIT    "/odm/lib64/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:98:#define HAL_LIBRARY_PATH_ODM_32BIT    "/odm/lib/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:104:#define HAL_LIBRARY_PATH_ODM    HAL_LIBRARY_PATH_ODM_64BIT
    system/libhidl/base/include/hidl/HidlInternal.h:109:#define HAL_LIBRARY_PATH_ODM    HAL_LIBRARY_PATH_ODM_32BIT

    system/libhidl/base/include/hidl/HidlInternal.h:93:#define HAL_LIBRARY_PATH_VENDOR_64BIT "/vendor/lib64/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:97:#define HAL_LIBRARY_PATH_VENDOR_32BIT "/vendor/lib/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:103:#define HAL_LIBRARY_PATH_VENDOR HAL_LIBRARY_PATH_VENDOR_64BIT
    system/libhidl/base/include/hidl/HidlInternal.h:108:#define HAL_LIBRARY_PATH_VENDOR HAL_LIBRARY_PATH_VENDOR_32BIT

    system/libhidl/base/include/hidl/HidlInternal.h:92:#define HAL_LIBRARY_PATH_VNDK_SP_64BIT "/system/lib64/vndk-sp/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:96:#define HAL_LIBRARY_PATH_VNDK_SP_32BIT "/system/lib/vndk-sp/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:102:#define HAL_LIBRARY_PATH_VNDK_SP HAL_LIBRARY_PATH_VNDK_SP_64BIT
    system/libhidl/base/include/hidl/HidlInternal.h:107:#define HAL_LIBRARY_PATH_VNDK_SP HAL_LIBRARY_PATH_VNDK_SP_32BIT


    system/libhidl/base/include/hidl/HidlInternal.h:91:#define HAL_LIBRARY_PATH_SYSTEM_64BIT "/system/lib64/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:95:#define HAL_LIBRARY_PATH_SYSTEM_32BIT "/system/lib/hw/"
    system/libhidl/base/include/hidl/HidlInternal.h:101:#define HAL_LIBRARY_PATH_SYSTEM HAL_LIBRARY_PATH_SYSTEM_64BIT
    system/libhidl/base/include/hidl/HidlInternal.h:106:#define HAL_LIBRARY_PATH_SYSTEM HAL_LIBRARY_PATH_SYSTEM_32BIT

    */
    std::vector<std::string> paths = {HAL_LIBRARY_PATH_ODM, HAL_LIBRARY_PATH_VENDOR,  HAL_LIBRARY_PATH_VNDK_SP, HAL_LIBRARY_PATH_SYSTEM};
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
        std::vector<std::string> libs = search(path, prefix, ".so");//  path + "android.hardware.audio@2.0-impl.so"

        for (const std::string &lib : libs) {
            const std::string fullPath = path + lib;

            if (path != HAL_LIBRARY_PATH_SYSTEM) {
                handle = android_load_sphal_library(fullPath.c_str(), dlMode);//    @system/core/libvndksupport/linker.c
            } else {
                handle = dlopen(fullPath.c_str(), dlMode);
            }

            if (handle == nullptr) {
                const char* error = dlerror();
                LOG(ERROR) << "Failed to dlopen " << lib << ": "  << (error == nullptr ? "unknown error" : error);
                continue;
            }

            if (!eachLib(handle, lib, sym)) {
                return;
            }
        }
    }
}

void* android_load_sphal_library(const char* name, int flag) {
    struct android_namespace_t* vendor_namespace = get_vendor_namespace();//    @system/core/libvndksupport/linker.c
    if (vendor_namespace != NULL) {
        const android_dlextinfo dlextinfo = {
            .flags = ANDROID_DLEXT_USE_NAMESPACE, .library_namespace = vendor_namespace,
        };
        void* handle = android_dlopen_ext(name, flag, &dlextinfo);//    @bionic/libdl/libdl.c
        if (!handle) {
            ALOGE("Could not load %s from %s namespace: %s.", name, namespace_name, dlerror());
        }
        return handle;
    } else {
        ALOGD("Loading %s from current namespace instead of sphal namespace.", name);
        return dlopen(name, flag);
    }
}


static struct android_namespace_t* get_vendor_namespace() {
    const char* namespace_names[] = {"sphal", "default", NULL};
    static struct android_namespace_t* vendor_namespace = NULL;
    if (vendor_namespace == NULL) {
        int name_idx = 0;
        while (namespace_names[name_idx] != NULL) {
            vendor_namespace = android_get_exported_namespace(namespace_names[name_idx]);
            if (vendor_namespace != NULL) {
                namespace_name = namespace_names[name_idx];
                break;
            }
            name_idx++;
        }
    }
    return vendor_namespace;
}


void* android_dlopen_ext(const char* filename, int flag, const android_dlextinfo* extinfo) {
const void* caller_addr = __builtin_return_address(0);
return __loader_android_dlopen_ext(filename, flag, extinfo, caller_addr);
}




// HIDL_FETCH_IDevicesFactory("default")
IDevicesFactory* HIDL_FETCH_IDevicesFactory(const char* /* name */) {
    return new DevicesFactory();
}


// registerReference("android.hardware.audio@2.0::IDevicesFactory", "default"); 
static void registerReference(const hidl_string &interfaceName, const hidl_string &instanceName) {
    sp<IServiceManager> binderizedManager = defaultServiceManager(); // new BpHwServiceManager(new BpHwBinder(0))
    if (binderizedManager == nullptr) {
        LOG(WARNING) << "Could not registerReference for "  << interfaceName << "/" << instanceName << ": null binderized manager.";
        return;
    }
    auto ret = binderizedManager->registerPassthroughClient(interfaceName, instanceName);//不明白这里的目的是什么?
    if (!ret.isOk()) {
        LOG(WARNING) << "Could not registerReference for " << interfaceName << "/" << instanceName << ": " << ret.description();
        return;
    }
    LOG(VERBOSE) << "Successfully registerReference for " << interfaceName << "/" << instanceName;
}

::android::hardware::Return<void> BpHwServiceManager::registerPassthroughClient(const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name){
    ::android::hardware::Return<void>  _hidl_out = ::android::hidl::manager::V1_0::BpHwServiceManager::_hidl_registerPassthroughClient(this, this, fqName, name);
    return _hidl_out;
}


::android::hardware::Return<void> BpHwServiceManager::_hidl_registerPassthroughClient(::android::hardware::IInterface *_hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name) {
    #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this_instrumentor->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this_instrumentor->getInstrumentationCallbacks();
    #else
    (void) _hidl_this_instrumentor;
    #endif // __ANDROID_DEBUGGABLE__
    atrace_begin(ATRACE_TAG_HAL, "HIDL::IServiceManager::registerPassthroughClient::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&fqName);
        _hidl_args.push_back((void *)&name);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "android.hidl.manager", "1.0", "IServiceManager", "registerPassthroughClient", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    _hidl_err = _hidl_data.writeInterfaceToken(BpHwServiceManager::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl_fqName_parent;

    _hidl_err = _hidl_data.writeBuffer(&fqName, sizeof(fqName), &_hidl_fqName_parent);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::writeEmbeddedToParcel(
            fqName,
            &_hidl_data,
            _hidl_fqName_parent,
            0 /* parentOffset */);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl_name_parent;

    _hidl_err = _hidl_data.writeBuffer(&name, sizeof(name), &_hidl_name_parent);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::writeEmbeddedToParcel(
            name,
            &_hidl_data,
            _hidl_name_parent,
            0 /* parentOffset */);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::IInterface::asBinder(_hidl_this)->transact(8 /* registerPassthroughClient */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "android.hidl.manager", "1.0", "IServiceManager", "registerPassthroughClient", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}
















// status_t status = service->registerAsService(name);
::android::status_t IDevicesFactory::registerAsService(const std::string &serviceName) { // serviceName  == "default"
    ::android::hardware::details::onRegistration("android.hardware.audio@2.0", "IDevicesFactory", serviceName);

    const ::android::sp<::android::hidl::manager::V1_0::IServiceManager> sm = ::android::hardware::defaultServiceManager();// new BpHwServiceManager(new BpHwBinder(0))
    if (sm == nullptr) {
        return ::android::INVALID_OPERATION;
    }
    ::android::hardware::Return<bool> ret = sm->add(serviceName.c_str(), this);
    return ret.isOk() && ret ? ::android::OK : ::android::UNKNOWN_ERROR;
}




