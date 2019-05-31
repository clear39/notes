

/**
out/soong/.intermediates/hardware/interfaces/radio/1.0/android.hardware.radio@1.0_genc++/gen/android/hardware/radio/1.0/RadioAll.cpp
out/soong/.intermediates/hardware/interfaces/radio/1.1/android.hardware.radio@1.1_genc++/gen/android/hardware/radio/1.1/RadioAll.cpp
*/
::android::status_t IRadio::registerAsService(const std::string &serviceName = "slot1") {
	//这里没有实际用处
	//比较进程和"android.hardware.radio@1.1"是否相同,进行设置
    ::android::hardware::details::onRegistration("android.hardware.radio@1.1", "IRadio", serviceName);

    const ::android::sp<::android::hidl::manager::V1_0::IServiceManager> sm = ::android::hardware::defaultServiceManager();  //sm =  new BpHwServiceManager(BpHwBinder(0)));
    if (sm == nullptr) {
        return ::android::INVALID_OPERATION;
    }
    ::android::hardware::Return<bool> ret = sm->add(serviceName.c_str(), this);
    return ret.isOk() && ret ? ::android::OK : ::android::UNKNOWN_ERROR;
}

//	@system/libhidl/transport/ServiceManagement.cpp
void onRegistration(const std::string &packageName,const std::string& /* interfaceName */,const std::string& /* instanceName */) {
    tryShortenProcessName(packageName);
}


void tryShortenProcessName(const std::string &packageName = "android.hardware.radio@1.1") {
    std::string processName = binaryName();

    if (!startsWith(processName, packageName)) {
        return;
    }

    // e.x. android.hardware.module.foo@1.0 -> foo@1.0
    size_t lastDot = packageName.rfind('.');
    size_t secondDot = packageName.rfind('.', lastDot - 1);

    if (secondDot == std::string::npos) {
        return;
    }

    std::string newName = processName.substr(secondDot + 1,16 /* TASK_COMM_LEN */ - 1);//	newName = "radio@1.1"
    ALOGI("Removing namespace from process name %s to %s.",processName.c_str(), newName.c_str());

    int rc = pthread_setname_np(pthread_self(), newName.c_str());//设置线程名字
    ALOGI_IF(rc != 0, "Removing namespace from process name %s failed.",processName.c_str());
}


std::string binaryName() {
    std::ifstream ifs("/proc/self/cmdline");
    std::string cmdline;
    if (!ifs.is_open()) {
        return "";
    }
    ifs >> cmdline;

    size_t idx = cmdline.rfind("/");
    if (idx != std::string::npos) {
        cmdline = cmdline.substr(idx + 1);
    }

    return cmdline;
}


bool startsWith(const std::string &in, const std::string &prefix) {
    return in.size() >= prefix.size() && in.substr(0, prefix.size()) == prefix;
}



::android::hardware::Return<bool> BpHwServiceManager::add(const ::android::hardware::hidl_string& name, const ::android::sp<::android::hidl::base::V1_0::IBase>& service){
    ::android::hardware::Return<bool>  _hidl_out = ::android::hidl::manager::V1_0::BpHwServiceManager::_hidl_add(this, this, name, service);

    return _hidl_out;
}



::android::hardware::Return<bool> BpHwServiceManager::_hidl_add(::android::hardware::IInterface *_hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, const ::android::hardware::hidl_string& name, const ::android::sp<::android::hidl::base::V1_0::IBase>& service) {
    #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this_instrumentor->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this_instrumentor->getInstrumentationCallbacks();
    #else
    (void) _hidl_this_instrumentor;
    #endif // __ANDROID_DEBUGGABLE__
    atrace_begin(ATRACE_TAG_HAL, "HIDL::IServiceManager::add::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&name);
        _hidl_args.push_back((void *)&service);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "android.hidl.manager", "1.0", "IServiceManager", "add", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    bool _hidl_out_success;

    _hidl_err = _hidl_data.writeInterfaceToken(BpHwServiceManager::descriptor);
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

    if (service == nullptr) {
        _hidl_err = _hidl_data.writeStrongBinder(nullptr);
    } else {
    	//toBinder	@system/libhidl/transport/include/hidl/HidlBinderSupport.h
        ::android::sp<::android::hardware::IBinder> _hidl_binder = ::android::hardware::toBinder< ::android::hidl::base::V1_0::IBase>(service);
        if (_hidl_binder.get() != nullptr) {
            _hidl_err = _hidl_data.writeStrongBinder(_hidl_binder);
        } else {
            _hidl_err = ::android::UNKNOWN_ERROR;
        }
    }
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    ::android::hardware::ProcessState::self()->startThreadPool();
    _hidl_err = ::android::hardware::IInterface::asBinder(_hidl_this)->transact(2 /* add */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    _hidl_err = _hidl_reply.readBool(&_hidl_out_success);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&_hidl_out_success);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "android.hidl.manager", "1.0", "IServiceManager", "add", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<bool>(_hidl_out_success);

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<bool>(_hidl_status);
}



template <typename IType>
sp<IBinder> toBinder(sp<IType> iface) {
    IType *ifacePtr = iface.get();
    if (ifacePtr == nullptr) {
        return nullptr;
    }
    if (ifacePtr->isRemote()) {//false
        return ::android::hardware::IInterface::asBinder(static_cast<BpInterface<IType>*>(ifacePtr));
    } else {
        std::string myDescriptor = details::getDescriptor(ifacePtr);//@system/libhidl/transport/HidlTransportUtils.cpp
        if (myDescriptor.empty()) {
            // interfaceDescriptor fails
            return nullptr;
        }

        // for get + set
        std::unique_lock<std::mutex> _lock = details::gBnMap.lock();//这里是在库加载的时候,进行出事话的

        wp<BHwBinder> wBnObj = details::gBnMap.getLocked(ifacePtr, nullptr);
        sp<IBinder> sBnObj = wBnObj.promote();

        if (sBnObj == nullptr) {
            auto func = details::gBnConstructorMap.get(myDescriptor, nullptr);
            if (!func) {
                return nullptr;
            }

            sBnObj = sp<IBinder>(func(static_cast<void*>(ifacePtr)));

            if (sBnObj != nullptr) {
                details::gBnMap.setLocked(ifacePtr, static_cast<BHwBinder*>(sBnObj.get()));
            }
        }

        return sBnObj;
    }
}

std::string getDescriptor(IBase* interface) {
    std::string myDescriptor{};
    auto ret = interface->interfaceDescriptor([&](const hidl_string &types) {//这里是c++ lamada表达式
        myDescriptor = types.c_str();
    });
    ret.isOk(); // ignored, return empty string if not isOk()
    return myDescriptor;
}



::android::hardware::Return<void> IRadio::interfaceDescriptor(interfaceDescriptor_cb _hidl_cb){
    _hidl_cb(IRadio::descriptor);//	就是赋值
    return ::android::hardware::Void();
}


/***
out/soong/.intermediates/hardware/interfaces/radio/1.0/android.hardware.radio@1.0_genc++/gen/android/hardware/radio/1.0/RadioAll.cpp
out/soong/.intermediates/hardware/interfaces/radio/1.1/android.hardware.radio@1.1_genc++/gen/android/hardware/radio/1.1/RadioAll.cpp
*/
__attribute__((constructor))static void static_constructor() {
    ::android::hardware::details::gBnConstructorMap.set(IRadio::descriptor,
            [](void *iIntf) -> ::android::sp<::android::hardware::IBinder> {
                return new BnHwRadio(static_cast<IRadio *>(iIntf));
            });
    ::android::hardware::details::gBsConstructorMap.set(IRadio::descriptor,
            [](void *iIntf) -> ::android::sp<::android::hidl::base::V1_0::IBase> {
                return new BsRadio(static_cast<IRadio *>(iIntf));
            });
};

__attribute__((destructor))static void static_destructor() {
    ::android::hardware::details::gBnConstructorMap.erase(IRadio::descriptor);
    ::android::hardware::details::gBsConstructorMap.erase(IRadio::descriptor);
};