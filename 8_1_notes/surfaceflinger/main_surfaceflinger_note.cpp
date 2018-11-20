//	@frameworks/native/services/surfaceflinger/main_surfaceflinger.cpp
int main(int, char**) {
    startHidlServices();

    signal(SIGPIPE, SIG_IGN);
    // When SF is launched in its own process, limit the number of
    // binder threads to 4.
    ProcessState::self()->setThreadPoolMaxThreadCount(4);

    // start the thread pool
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();

    // instantiate surfaceflinger
    sp<SurfaceFlinger> flinger = new SurfaceFlinger();

    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);

    set_sched_policy(0, SP_FOREGROUND);

    // Put most SurfaceFlinger threads in the system-background cpuset
    // Keeps us from unnecessarily using big cores
    // Do this after the binder thread pool init
    if (cpusets_enabled()) set_cpuset_policy(0, SP_SYSTEM);

    // initialize before clients can connect
    flinger->init();

    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false);//"SurfaceFlinger";

    // publish GpuService
    sp<GpuService> gpuservice = new GpuService();
    sm->addService(String16(GpuService::SERVICE_NAME), gpuservice, false);//const char* const GpuService::SERVICE_NAME = "gpu";

    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO");
    }

    // run surface flinger in this thread
    flinger->run();

    return 0;
}

   static char const* getServiceName() ANDROID_API {
        return "SurfaceFlinger";
    }


static status_t startHidlServices() {
    using android::frameworks::displayservice::V1_0::implementation::DisplayService;
    using android::frameworks::displayservice::V1_0::IDisplayService;
    using android::hardware::configstore::getBool;
    using android::hardware::configstore::getBool;
    using android::hardware::configstore::V1_0::ISurfaceFlingerConfigs;
    hardware::configureRpcThreadpool(1 /* maxThreads */,false /* callerWillJoin */);

    status_t err;

    if (getBool<ISurfaceFlingerConfigs,&ISurfaceFlingerConfigs::startGraphicsAllocatorService>(false)) {//false
        err = startGraphicsAllocatorService();
        if (err != OK) {
           return err;
        }
    }

    sp<IDisplayService> displayservice = new DisplayService();
    err = displayservice->registerAsService();

    if (err != OK) {
        ALOGE("Could not register IDisplayService service.");
    }

    return err;
}

template<typename I, android::hardware::Return<void> (I::* func)(std::function<void(const OptionalBool&)>)>
bool getBool(const bool defValue) {
    return get<OptionalBool, I, func>(defValue);
}

template<typename V, typename I, android::hardware::Return<void> (I::* func)(std::function<void(const V&)>)>
decltype(V::value) get(const decltype(V::value) &defValue) {
    using namespace android::hardware::details;
    // static initializer used for synchronizations
    auto getHelper = []()->V {
        V ret;
        sp<I> configs = getService<I>();//sp<ISurfaceFlingerConfigs> configs = getService<ISurfaceFlingerConfigs>();

        if (!configs.get()) {
            // fallback to the default value
            ret.specified = false;
        } else {
            auto status = (*configs.*func)([&ret](V v) {
                ret = v;
            });
            if (!status.isOk()) {
                std::ostringstream oss;
                oss << "HIDL call failed for retrieving a config item from " "configstore : " << status.description().c_str();
                logAlwaysError(oss.str());
                ret.specified = false;
            }
        }

        return ret;
    };



    static V cachedValue = getHelper();

    if (wouldLogInfo()) {//	@hardware/interfaces/configstore/utils/ConfigStoreUtils.set_cpuset_policy		//返回true
        std::string iname = __PRETTY_FUNCTION__;
        // func name starts with "func = " in __PRETTY_FUNCTION__
        auto pos = iname.find("func = ");
        if (pos != std::string::npos) {
            iname = iname.substr(pos + sizeof("func = "));
            iname.pop_back();  // remove trailing ']'
        } else {
            iname += " (unknown)";
        }

        std::ostringstream oss;
        oss << iname << " retrieved: "  << (cachedValue.specified ? cachedValue.value : defValue) << (cachedValue.specified ? "" : " (default)");
        logAlwaysInfo(oss.str());
    }

    return cachedValue.specified ? cachedValue.value : defValue;
}

// a function to retrieve and cache the service handle
// for a particular interface
template <typename I>
sp<I> getService() {
    // static initializer used for synchronizations
    static sp<I> configs = I::getService();//ISurfaceFlingerConfigs::getService();
    return configs;
}

// static
::android::sp<ISurfaceFlingerConfigs> ISurfaceFlingerConfigs::getService(const std::string &serviceName="default", bool getStub=false) {
    using ::android::hardware::defaultServiceManager;
    using ::android::hardware::details::waitForHwService;
    using ::android::hardware::getPassthroughServiceManager;
    using ::android::hardware::Return;
    using ::android::sp;
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;

    sp<ISurfaceFlingerConfigs> iface = nullptr;

    const sp<::android::hidl::manager::V1_0::IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("getService: defaultServiceManager() is null");
        return nullptr;
    }

    Return<Transport> transportRet = sm->getTransport(ISurfaceFlingerConfigs::descriptor, serviceName);
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

    for (int tries = 0; !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)); tries++) {
        if (tries > 1) {
            ALOGI("getService: Will do try %d for %s/%s in 1s...", tries, ISurfaceFlingerConfigs::descriptor, serviceName.c_str());
            sleep(1);
        }
        if (vintfHwbinder && tries > 0) {
            waitForHwService(ISurfaceFlingerConfigs::descriptor, serviceName);
        }
        Return<sp<::android::hidl::base::V1_0::IBase>> ret =   sm->get(ISurfaceFlingerConfigs::descriptor, serviceName);
        if (!ret.isOk()) {
            ALOGE("ISurfaceFlingerConfigs: defaultServiceManager()->get returns %s", ret.description().c_str());
            break;
        }
        sp<::android::hidl::base::V1_0::IBase> base = ret;
        if (base == nullptr) {
            if (tries > 0) {
                ALOGW("ISurfaceFlingerConfigs: found null hwbinder interface");
            }continue;
        }
        Return<sp<ISurfaceFlingerConfigs>> castRet = ISurfaceFlingerConfigs::castFrom(base, true /* emitError */);
        if (!castRet.isOk()) {
            if (castRet.isDeadObject()) {
                ALOGW("ISurfaceFlingerConfigs: found dead hwbinder service");
                continue;
            } else {
                ALOGW("ISurfaceFlingerConfigs: cannot call into hwbinder service: %s; No permission? Check for selinux denials.", castRet.description().c_str());
                break;
            }
        }
        iface = castRet;
        if (iface == nullptr) {
            ALOGW("ISurfaceFlingerConfigs: received incompatible service; bug in hwservicemanager?");
            break;
        }
        return iface;
    }
    if (getStub || vintfPassthru || vintfLegacy) {
        const sp<::android::hidl::manager::V1_0::IServiceManager> pm = getPassthroughServiceManager();
        if (pm != nullptr) {
            Return<sp<::android::hidl::base::V1_0::IBase>> ret =  pm->get(ISurfaceFlingerConfigs::descriptor, serviceName);
            if (ret.isOk()) {
                sp<::android::hidl::base::V1_0::IBase> baseInterface = ret;
                if (baseInterface != nullptr) {
                    iface = ISurfaceFlingerConfigs::castFrom(baseInterface);
                    if (!getStub || trebleTestingOverride) {
                        iface = new BsSurfaceFlingerConfigs(iface);
                    }
                }
            }
        }
    }
    return iface;
}

//
::android::hardware::Return<void> BpHwSurfaceFlingerConfigs::startGraphicsAllocatorService(startGraphicsAllocatorService_cb _hidl_cb){
    ::android::hardware::Return<void>  _hidl_out = ::android::hardware::configstore::V1_0::BpHwSurfaceFlingerConfigs::_hidl_startGraphicsAllocatorService(this, this, _hidl_cb);

    return _hidl_out;
}


::android::hardware::Return<void> BpHwSurfaceFlingerConfigs::_hidl_startGraphicsAllocatorService(::android::hardware::IInterface *_hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, startGraphicsAllocatorService_cb _hidl_cb) {
    #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this_instrumentor->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this_instrumentor->getInstrumentationCallbacks();
    #else
    (void) _hidl_this_instrumentor;
    #endif // __ANDROID_DEBUGGABLE__
    if (_hidl_cb == nullptr) {
        return ::android::hardware::Status::fromExceptionCode(
                ::android::hardware::Status::EX_ILLEGAL_ARGUMENT,
                "Null synchronous callback passed.");
    }

    atrace_begin(ATRACE_TAG_HAL, "HIDL::ISurfaceFlingerConfigs::startGraphicsAllocatorService::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "android.hardware.configstore", "1.0", "ISurfaceFlingerConfigs", "startGraphicsAllocatorService", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    const OptionalBool* _hidl_out_value;

    _hidl_err = _hidl_data.writeInterfaceToken(BpHwSurfaceFlingerConfigs::descriptor);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::IInterface::asBinder(_hidl_this)->transact(12 /* startGraphicsAllocatorService */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    size_t _hidl__hidl_out_value_parent;

    _hidl_err = _hidl_reply.readBuffer(sizeof(*_hidl_out_value), &_hidl__hidl_out_value_parent,  reinterpret_cast<const void **>(&_hidl_out_value));
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_cb(*_hidl_out_value);

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)_hidl_out_value);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "android.hardware.configstore", "1.0", "ISurfaceFlingerConfigs", "startGraphicsAllocatorService", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>();

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<void>(_hidl_status);
}





//	@hardware/interfaces/configstore/utils/ConfigStoreUtils.cpp
bool wouldLogInfo() {
    return WOULD_LOG(INFO);
}

//	@system/core/base/include/android-base/logging.h
// Defines whether the given severity will be logged or silently swallowed.
#define WOULD_LOG(severity) 	(UNLIKELY((SEVERITY_LAMBDA(severity)) >= ::android::base::GetMinimumLogSeverity()) || MUST_LOG_MESSAGE(severity))

//	@system/core/base/include/android-base/logging.h
// A helper macro that produces an expression that accepts both a qualified name and an
// unqualified name for a LogSeverity, and returns a LogSeverity value.
// Note: DO NOT USE DIRECTLY. This is an implementation detail.
#define SEVERITY_LAMBDA(severity) ([&]() {    \
  using ::android::base::VERBOSE;             \
  using ::android::base::DEBUG;               \
  using ::android::base::INFO;                \
  using ::android::base::WARNING;             \
  using ::android::base::ERROR;               \
  using ::android::base::FATAL_WITHOUT_ABORT; \
  using ::android::base::FATAL;               \
  return (severity); }())

//	@system/core/base/logging.cpp
LogSeverity GetMinimumLogSeverity() {
    return gMinimumLogSeverity;//	static LogSeverity gMinimumLogSeverity = INFO;
}




static status_t startGraphicsAllocatorService() {
    using android::hardware::graphics::allocator::V2_0::IAllocator;

    status_t result = hardware::registerPassthroughServiceImplementation<IAllocator>();
    if (result != OK) {
        ALOGE("could not start graphics allocator service");
        return result;
    }

    return OK;
}