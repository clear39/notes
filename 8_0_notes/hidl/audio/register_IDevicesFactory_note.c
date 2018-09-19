/*
   android::status_t status;
   status = registerPassthroughServiceImplementation<IDevicesFactory>();
*/

//	system/libhidl/transport/include/hidl/LegacySupport.h
/**
 * Registers passthrough service implementation.
 */
template<class Interface>
__attribute__((warn_unused_result))
status_t registerPassthroughServiceImplementation(std::string name = "default") {
    sp<Interface> service = Interface::getService(name, true /* getStub */);//  sp<IDevicesFactory> service = IDevicesFactory::getService("default", true /* getStub */);

    if (service == nullptr) {
        ALOGE("Could not get passthrough implementation for %s/%s.",Interface::descriptor, name.c_str());
        return EXIT_FAILURE;
    }

    ////IDevicesFactory->isRemote() 为 false
    LOG_FATAL_IF(service->isRemote(), "Implementation of %s/%s is remote!",Interface::descriptor, name.c_str());

    status_t status = service->registerAsService(name);

    if (status == OK) {
        ALOGI("Registration complete for %s/%s.",Interface::descriptor, name.c_str());
    } else {
        ALOGE("Could not register service %s/%s (%d).",Interface::descriptor, name.c_str(), status);
    }

    return status;
}

// out/soong/.intermediates/hardware/interfaces/audio/2.0/android.hardware.audio@2.0_genc++/gen/android/hardware/audio/2.0/DevicesFactoryAll.cpp
//  sp<IDevicesFactory> service = IDevicesFactory::getService("default", true /* getStub */);
// static
::android::sp<IDevicesFactory> IDevicesFactory::getService(const std::string &serviceName, const bool getStub) {
    using ::android::hardware::defaultServiceManager;
    using ::android::hardware::details::waitForHwService;
    using ::android::hardware::getPassthroughServiceManager;
    using ::android::hardware::Return;
    using ::android::sp;
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;

    sp<IDevicesFactory> iface = nullptr;

    const sp<::android::hidl::manager::V1_0::IServiceManager> sm = defaultServiceManager();// system/libhidl/transport/ServiceManagement.cpp:135
    if (sm == nullptr) {
        ALOGE("getService: defaultServiceManager() is null");
        return nullptr;
    }

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

    for (int tries = 0; !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)); tries++) { //getStub = true
        if (tries > 1) {
            ALOGI("getService: Will do try %d for %s/%s in 1s...", tries, IDevicesFactory::descriptor, serviceName.c_str());
            sleep(1);
        }
        if (vintfHwbinder && tries > 0) {
            waitForHwService(IDevicesFactory::descriptor, serviceName);
        }
        Return<sp<::android::hidl::base::V1_0::IBase>> ret = sm->get(IDevicesFactory::descriptor, serviceName);
        if (!ret.isOk()) {
            ALOGE("IDevicesFactory: defaultServiceManager()->get returns %s", ret.description().c_str());
            break;
        }
        sp<::android::hidl::base::V1_0::IBase> base = ret;
        if (base == nullptr) {
            if (tries > 0) {
                ALOGW("IDevicesFactory: found null hwbinder interface");
            }
	    continue;
        }
        Return<sp<IDevicesFactory>> castRet = IDevicesFactory::castFrom(base, true /* emitError */);
        if (!castRet.isOk()) {
            if (castRet.isDeadObject()) {
                ALOGW("IDevicesFactory: found dead hwbinder service");
                continue;
            } else {
                ALOGW("IDevicesFactory: cannot call into hwbinder service: %s; No permission? Check for selinux denials.", castRet.description().c_str());
                break;
            }
        }
        iface = castRet;
        if (iface == nullptr) {
            ALOGW("IDevicesFactory: received incompatible service; bug in hwservicemanager?");
            break;
        }
        return iface;
    }



    if (getStub || vintfPassthru || vintfLegacy) { // getStub = true
        const sp<::android::hidl::manager::V1_0::IServiceManager> pm = getPassthroughServiceManager();
        if (pm != nullptr) {
            Return<sp<::android::hidl::base::V1_0::IBase>> ret = pm->get(IDevicesFactory::descriptor, serviceName);
            if (ret.isOk()) {
                sp<::android::hidl::base::V1_0::IBase> baseInterface = ret;
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

//	system/libhidl/transport/ServiceManagement.cpp:135
sp<IServiceManager> defaultServiceManager() {
    {
        AutoMutex _l(details::gDefaultServiceManagerLock);
        if (details::gDefaultServiceManager != NULL) {
            return details::gDefaultServiceManager;
        }

        if (access("/dev/hwbinder", F_OK|R_OK|W_OK) != 0) {
            // HwBinder not available on this device or not accessible to
            // this process.
            return nullptr;
        }

        waitForHwServiceManager();

        while (details::gDefaultServiceManager == NULL) {
	    //	system/libhidl/transport/include/hidl/HidlBinderSupport.h
            details::gDefaultServiceManager = fromBinder<IServiceManager, BpHwServiceManager, BnHwServiceManager>(ProcessState::self()->getContextObject(NULL));
            if (details::gDefaultServiceManager == NULL) {
                LOG(ERROR) << "Waited for hwservicemanager, but got nullptr.";
                sleep(1);
            }
        }
    }

    return details::gDefaultServiceManager;
}

void waitForHwServiceManager() {
    using std::literals::chrono_literals::operator""s;

    //	static const char* kHwServicemanagerReadyProperty = "hwservicemanager.ready";
    while (!WaitForProperty(kHwServicemanagerReadyProperty, "true", 1s)) { //	system/core/base/properties.cpp:146
        LOG(WARNING) << "Waited for hwservicemanager.ready for a second, waiting another...";
    }
}


//	ProcessState::self()->getContextObject(NULL)
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);
}

struct handle_entry {
    IBinder* binder; // new BpHwBinder(handle);
    RefBase::weakref_type* refs;
};

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        // We need to create a new BpHwBinder if there isn't currently one, OR we
        // are unable to acquire a weak reference on this current one.  See comment
        // in getWeakProxyForHandle() for more info about this.
        IBinder* b = e->binder;// e->binder == NULL
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            b = new BpHwBinder(handle);
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            // This little bit of nastyness is to allow us to add a primary
            // reference to the remote proxy when this team doesn't have one
            // but another team is sending the handle to us.
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}

ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size(); //            Vector<handle_entry>mHandleToObject;
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = NULL;
        e.refs = NULL;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}


//	details::gDefaultServiceManager = fromBinder<IServiceManager, BpHwServiceManager, BnHwServiceManager>(new BpHwBinder(0));
//	system/libhidl/transport/include/hidl/HidlBinderSupport.h
template <typename IType, typename ProxyType, typename StubType>
sp<IType> fromBinder(const sp<IBinder>& binderIface) {// sp<IServiceManager> fromBinder(new BpHwBinder(0))
    using ::android::hidl::base::V1_0::IBase;
    using ::android::hidl::base::V1_0::BnHwBase;

    if (binderIface.get() == nullptr) {
        return nullptr;
    }
    if (binderIface->localBinder() == nullptr) {// 
        return new ProxyType(binderIface); // new BpHwServiceManager(new BpHwBinder(0))
    }
    sp<IBase> base = static_cast<BnHwBase*>(binderIface.get())->getImpl();
    if (details::canCastInterface(base.get(), IType::descriptor)) {
        StubType* stub = static_cast<StubType*>(binderIface.get());// BnHwServiceManager* stub = static_cast<BnHwServiceManager*>(binderIface.get());
        return stub->getImpl();
    } else {
        return nullptr;
    }
}




// status_t status = service->registerAsService(name);
::android::status_t IDevicesFactory::registerAsService(const std::string &serviceName) { // serviceName  == "default"
    ::android::hardware::details::onRegistration("android.hardware.audio@2.0", "IDevicesFactory", serviceName);

    const ::android::sp<::android::hidl::manager::V1_0::IServiceManager> sm = ::android::hardware::defaultServiceManager();
    if (sm == nullptr) {
        return ::android::INVALID_OPERATION;
    }
    ::android::hardware::Return<bool> ret = sm->add(serviceName.c_str(), this);
    return ret.isOk() && ret ? ::android::OK : ::android::UNKNOWN_ERROR;
}



sp<IServiceManager> getPassthroughServiceManager() {
    static sp<PassthroughServiceManager> manager(new PassthroughServiceManager());// PassthroughServiceManager	system/libhidl/transport/ServiceManagement.cpp:252
    return manager;
}


//	Return<sp<::android::hidl::base::V1_0::IBase>> ret = pm->get(IDevicesFactory::descriptor, serviceName);   //	serviceName == "default"
//	system/libhidl/transport/ServiceManagement.cpp
Return<sp<IBase>> PassthroughServiceManager::get(const hidl_string& fqName,const hidl_string& name) override {
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
    auto ret = binderizedManager->registerPassthroughClient(interfaceName, instanceName);
    if (!ret.isOk()) {
        LOG(WARNING) << "Could not registerReference for " << interfaceName << "/" << instanceName << ": " << ret.description();
        return;
    }
    LOG(VERBOSE) << "Successfully registerReference for " << interfaceName << "/" << instanceName;
}


::android::hardware::Return<void> BpHwServiceManager::registerPassthroughClient(const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name) {
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

    _hidl_err = _hidl_data.writeInterfaceToken(IServiceManager::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl_fqName_parent;

    _hidl_err = _hidl_data.writeBuffer(&fqName, sizeof(fqName), &_hidl_fqName_parent);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::writeEmbeddedToParcel(fqName,&_hidl_data,_hidl_fqName_parent,0 /* parentOffset */);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    size_t _hidl_name_parent;

    _hidl_err = _hidl_data.writeBuffer(&name, sizeof(name), &_hidl_name_parent);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::writeEmbeddedToParcel(name,&_hidl_data,_hidl_name_parent,0 /* parentOffset */);

    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = remote()->transact(8 /* registerPassthroughClient */, _hidl_data, &_hidl_reply);
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





static void PassthroughServiceManager::openLibs(const std::string& fqName,std::function<bool /* continue */(void* /* handle */,const std::string& /* lib */, const std::string& /* sym */)> eachLib) {
	//fqName looks like android.hardware.foo@1.0::IFoo
	size_t idx = fqName.find("::");  //android.hardware.audio@2.0::IDevicesFactory

	if (idx == std::string::npos || idx + strlen("::") + 1 >= fqName.size()) {
	    LOG(ERROR) << "Invalid interface name passthrough lookup: " << fqName;
	    return;
	}

	std::string packageAndVersion = fqName.substr(0, idx);// android.hardware.audio@2.0
	std::string ifaceName = fqName.substr(idx + strlen("::")); // IDevicesFactory

	const std::string prefix = packageAndVersion + "-impl";// android.hardware.audio@2.0-impl
	const std::string sym = "HIDL_FETCH_" + ifaceName; // HIDL_FETCH_IDevicesFactory

	const int dlMode = RTLD_LAZY;//bionic/libc/include/dlfcn.h:66:  RTLD_LAZY = 1, //加载动态库的一种模式
	void *handle = nullptr;

	dlerror(); // clear


	//	system/libhidl/base/include/hidl/HidlInternal.h
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
	// 这里会在设备/vendor/lib/hw目录下找到 android.hardware.audio@2.0-impl.so
	for (const std::string& path : paths) {
	    std::vector<std::string> libs = search(path, prefix, ".so");

	    for (const std::string &lib : libs) {
		const std::string fullPath = path + lib;

		if (path != HAL_LIBRARY_PATH_SYSTEM) {// 如果不在system目录下
		    handle = android_load_sphal_library(fullPath.c_str(), dlMode);//	system/core/libvndksupport/linker.c
		} else {
		    handle = dlopen(fullPath.c_str(), dlMode);
		}

		if (handle == nullptr) {
		    const char* error = dlerror();
		    LOG(ERROR) << "Failed to dlopen " << lib << ": " << (error == nullptr ? "unknown error" : error);
		    continue;
		}

		if (!eachLib(handle, lib, sym)) {// sym == HIDL_FETCH_IDevicesFactory
		    return;
		}
	    }
	}
}


#define HAL_LIBRARY_PATH_SYSTEM_64BIT "/system/lib64/hw/"
#define HAL_LIBRARY_PATH_VENDOR_64BIT "/vendor/lib64/hw/"
#define HAL_LIBRARY_PATH_ODM_64BIT    "/odm/lib64/hw/"
#define HAL_LIBRARY_PATH_SYSTEM_32BIT "/system/lib/hw/"
#define HAL_LIBRARY_PATH_VENDOR_32BIT "/vendor/lib/hw/"
#define HAL_LIBRARY_PATH_ODM_32BIT    "/odm/lib/hw/"

#if defined(__LP64__)
#define HAL_LIBRARY_PATH_SYSTEM HAL_LIBRARY_PATH_SYSTEM_64BIT
#define HAL_LIBRARY_PATH_VENDOR HAL_LIBRARY_PATH_VENDOR_64BIT
#define HAL_LIBRARY_PATH_ODM    HAL_LIBRARY_PATH_ODM_64BIT
#else
#define HAL_LIBRARY_PATH_SYSTEM HAL_LIBRARY_PATH_SYSTEM_32BIT
#define HAL_LIBRARY_PATH_VENDOR HAL_LIBRARY_PATH_VENDOR_32BIT
#define HAL_LIBRARY_PATH_ODM    HAL_LIBRARY_PATH_ODM_32BIT
#endif



std::vector<std::string> search(const std::string &path,const std::string &prefix,const std::string &suffix) {
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(path.c_str()), closedir);
    if (!dir) return {};

    std::vector<std::string> results{};

    dirent* dp;
    while ((dp = readdir(dir.get())) != nullptr) {
        std::string name = dp->d_name;

        if (startsWith(name, prefix) && endsWith(name, suffix)) {
            results.push_back(name);
        }
    }

    return results;
}


void* android_load_sphal_library(const char* name, int flag) {
    struct android_namespace_t* sphal_namespace = android_get_exported_namespace("sphal");
    if (sphal_namespace != NULL) {
        const android_dlextinfo dlextinfo = {
            .flags = ANDROID_DLEXT_USE_NAMESPACE,
	    .library_namespace = sphal_namespace,
        };
        void* handle = android_dlopen_ext(name, flag, &dlextinfo);
        if (!handle) {
            ALOGE("Could not load %s from sphal namespace: %s. ",name, dlerror());
        }
        return handle;
    } else {
        ALOGI("sphal namespace is not configured for this process. " "Loading %s from the current namespace instead.",name);
        return dlopen(name, flag);
    }
}




struct IDevicesFactory : public ::android::hidl::base::V1_0::IBase {
    typedef Result Result;

    enum class Device : int32_t {
      PRIMARY = 0,
      A2DP = 1,
      USB = 2,
      R_SUBMIX = 3,
      STUB = 4,
   };
  
   virtual bool isRemote() const override { return false; }

   ......
}








