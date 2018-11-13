/** @hide */
public abstract class HwBinder implements IHwBinder {

	   //	@frameworks/base/core/jni/android_os_HwBinder.cpp
	    public static native final IHwBinder getService(String iface,String serviceName)throws RemoteException, NoSuchElementException;

}


static jobject JHwBinder_native_getService( JNIEnv *env, jclass /* clazzObj */, jstring ifaceNameObj = "android.hardware.radio@1.0::IRadio",string serviceNameObj = "slot1") {
    using ::android::hidl::base::V1_0::IBase;
    using ::android::hidl::manager::V1_0::IServiceManager;

    if (ifaceNameObj == NULL) {
        jniThrowException(env, "java/lang/NullPointerException", NULL);
        return NULL;
    }
    if (serviceNameObj == NULL) {
        jniThrowException(env, "java/lang/NullPointerException", NULL);
        return NULL;
    }

    //	@system/libhidl/transport/ServiceManagement.cpp:136:
    //	BpHwServiceManager实现 @out/soong/.intermediates/system/libhidl/transport/manager/1.0/
    auto manager = hardware::defaultServiceManager();	//	auto manager =  new BpHwServiceManager(BpHwBinder(0)));

    if (manager == nullptr) {
        LOG(ERROR) << "Could not get hwservicemanager.";
        signalExceptionForError(env, UNKNOWN_ERROR, true /* canThrowRemoteException */);
        return NULL;
    }

    const char *ifaceNameCStr = env->GetStringUTFChars(ifaceNameObj, NULL);
    if (ifaceNameCStr == NULL) {
        return NULL; // XXX exception already pending?
    }
    std::string ifaceName(ifaceNameCStr);
    env->ReleaseStringUTFChars(ifaceNameObj, ifaceNameCStr);
    ::android::hardware::hidl_string ifaceNameHStr;
    ifaceNameHStr.setToExternal(ifaceName.c_str(), ifaceName.size());

    const char *serviceNameCStr = env->GetStringUTFChars(serviceNameObj, NULL);
    if (serviceNameCStr == NULL) {
        return NULL; // XXX exception already pending?
    }
    std::string serviceName(serviceNameCStr);
    env->ReleaseStringUTFChars(serviceNameObj, serviceNameCStr);
    ::android::hardware::hidl_string serviceNameHStr;
    serviceNameHStr.setToExternal(serviceName.c_str(), serviceName.size());

    LOG(INFO) << "Looking for service "<< ifaceName<< "/"<< serviceName;


    //	auto manager =  new BpHwServiceManager(BpHwBinder(0)));
    Return<IServiceManager::Transport> transportRet = manager->getTransport(ifaceNameHStr, serviceNameHStr);

    if (!transportRet.isOk()) {
        signalExceptionForError(env, UNKNOWN_ERROR, true /* canThrowRemoteException */);
        return NULL;
    }

    IServiceManager::Transport transport = transportRet;

#ifdef __ANDROID_TREBLE__


#ifdef __ANDROID_DEBUGGABLE__
    const char* testingOverride = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool vintfLegacy = (transport == IServiceManager::Transport::EMPTY) && testingOverride && !strcmp(testingOverride, "true");
#else // __ANDROID_TREBLE__ but not __ANDROID_DEBUGGABLE__
    const bool vintfLegacy = false;
#endif // __ANDROID_DEBUGGABLE__



#else // not __ANDROID_TREBLE__
    const bool vintfLegacy = (transport == IServiceManager::Transport::EMPTY);
#endif // __ANDROID_TREBLE__";

    if (transport != IServiceManager::Transport::HWBINDER && !vintfLegacy) {
        LOG(ERROR) << "service " << ifaceName << " declares transport method " << toString(transport) << " but framework expects hwbinder.";
        signalExceptionForError(env, NAME_NOT_FOUND, true /* canThrowRemoteException */);
        return NULL;
    }

	//	auto manager =  new BpHwServiceManager(BpHwBinder(0)));
    Return<sp<hidl::base::V1_0::IBase>> ret = manager->get(ifaceNameHStr, serviceNameHStr);

    if (!ret.isOk()) {
        signalExceptionForError(env, UNKNOWN_ERROR, true /* canThrowRemoteException */);
        return NULL;
    }

    sp<hardware::IBinder> service = hardware::toBinder<hidl::base::V1_0::IBase>(ret);

    if (service == NULL) {
        signalExceptionForError(env, NAME_NOT_FOUND);
        return NULL;
    }

    LOG(INFO) << "Starting thread pool.";
    ::android::hardware::ProcessState::self()->startThreadPool();

    return JHwRemoteBinder::NewObject(env, service);
}



//	@out/soong/.intermediates/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen/android/hidl/manager/1.0
//	@out/soong/.intermediates/system/libhidl/transport/manager/1.1/android.hidl.manager@1.1_genc++_headers/gen/android/hidl/manager/1.1
::android::hardware::Return<::android::hidl::manager::V1_0::IServiceManager::Transport> BpHwServiceManager::getTransport(const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name){
    ::android::hardware::Return<::android::hidl::manager::V1_0::IServiceManager::Transport>  _hidl_out = ::android::hidl::manager::V1_0::BpHwServiceManager::_hidl_getTransport(this, this, fqName, name);
    return _hidl_out;
}

//	struct BpHwServiceManager : public ::android::hardware::BpInterface<IServiceManager>, public ::android::hardware::details::HidlInstrumentor {
::android::hardware::Return<IServiceManager::Transport> BpHwServiceManager::_hidl_getTransport(::android::hardware::IInterface *_hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name) {
 #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this_instrumentor->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this_instrumentor->getInstrumentationCallbacks();
 #else
    (void) _hidl_this_instrumentor;
#endif // __ANDROID_DEBUGGABLE__



    atrace_begin(ATRACE_TAG_HAL, "HIDL::IServiceManager::getTransport::client");
#ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&fqName);
        _hidl_args.push_back((void *)&name);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "android.hidl.manager", "1.0", "IServiceManager", "getTransport", &_hidl_args);
        }
    }
  #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    IServiceManager::Transport _hidl_out_transport;

    //		struct BpHwServiceManager : public ::android::hardware::BpInterface<IServiceManager>, public ::android::hardware::details::HidlInstrumentor {
    //	@	out/soong/.intermediates/system/libhidl/transport/manager/1.1/android.hidl.manager@1.1_genc++_headers/gen/android/hidl/manager/1.1/BpHwServiceManager.h
    //	BpHwServiceManager::descriptor是由 IServiceManager继承得来

     //	system/libhwbinder/include/hwbinder/IInterface.h:58:class BpInterface : public INTERFACE, public IInterface, public BpHwRefBase



    //	const char* IServiceManager::descriptor("android.hidl.manager@1.1::IServiceManager"); 		
    //	@out/soong/.intermediates/system/libhidl/transport/manager/1.1/android.hidl.manager@1.1_genc++/gen/android/hidl/manager/1.1/ServiceManagerAll.cpp
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

    _hidl_err = ::android::hardware::IInterface::asBinder(_hidl_this)->transact(3 /* getTransport */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    _hidl_err = _hidl_reply.readUint8((uint8_t *)&_hidl_out_transport);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    atrace_end(ATRACE_TAG_HAL);



#ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&_hidl_out_transport);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "android.hidl.manager", "1.0", "IServiceManager", "getTransport", &_hidl_args);
        }
    }
#endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<IServiceManager::Transport>(_hidl_out_transport);

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<IServiceManager::Transport>(_hidl_status);
}





// Methods from ::android::hidl::manager::V1_0::IServiceManager follow.
::android::hardware::Return<::android::sp<::android::hidl::base::V1_0::IBase>> BpHwServiceManager::get(const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name){
    ::android::hardware::Return<::android::sp<::android::hidl::base::V1_0::IBase>>  _hidl_out = ::android::hidl::manager::V1_0::BpHwServiceManager::_hidl_get(this, this, fqName, name);

    return _hidl_out;
}

// Methods from IServiceManager follow.
::android::hardware::Return<::android::sp<::android::hidl::base::V1_0::IBase>> BpHwServiceManager::_hidl_get(::android::hardware::IInterface *_hidl_this, ::android::hardware::details::HidlInstrumentor *_hidl_this_instrumentor, const ::android::hardware::hidl_string& fqName, const ::android::hardware::hidl_string& name) {
    #ifdef __ANDROID_DEBUGGABLE__
    bool mEnableInstrumentation = _hidl_this_instrumentor->isInstrumentationEnabled();
    const auto &mInstrumentationCallbacks = _hidl_this_instrumentor->getInstrumentationCallbacks();
    #else
    (void) _hidl_this_instrumentor;
    #endif // __ANDROID_DEBUGGABLE__
    atrace_begin(ATRACE_TAG_HAL, "HIDL::IServiceManager::get::client");
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&fqName);
        _hidl_args.push_back((void *)&name);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_ENTRY, "android.hidl.manager", "1.0", "IServiceManager", "get", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    ::android::hardware::Parcel _hidl_data;
    ::android::hardware::Parcel _hidl_reply;
    ::android::status_t _hidl_err;
    ::android::hardware::Status _hidl_status;

    ::android::sp<::android::hidl::base::V1_0::IBase> _hidl_out_service;

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

    _hidl_err = ::android::hardware::IInterface::asBinder(_hidl_this)->transact(1 /* get */, _hidl_data, &_hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    _hidl_err = ::android::hardware::readFromParcel(&_hidl_status, _hidl_reply);
    if (_hidl_err != ::android::OK) { goto _hidl_error; }

    if (!_hidl_status.isOk()) { return _hidl_status; }

    {
        ::android::sp<::android::hardware::IBinder> _hidl__hidl_out_service_binder;
        _hidl_err = _hidl_reply.readNullableStrongBinder(&_hidl__hidl_out_service_binder);
        if (_hidl_err != ::android::OK) { goto _hidl_error; }

        _hidl_out_service = ::android::hardware::fromBinder<::android::hidl::base::V1_0::IBase,::android::hidl::base::V1_0::BpHwBase,::android::hidl::base::V1_0::BnHwBase>(_hidl__hidl_out_service_binder);
    }

    atrace_end(ATRACE_TAG_HAL);
    #ifdef __ANDROID_DEBUGGABLE__
    if (UNLIKELY(mEnableInstrumentation)) {
        std::vector<void *> _hidl_args;
        _hidl_args.push_back((void *)&_hidl_out_service);
        for (const auto &callback: mInstrumentationCallbacks) {
            callback(InstrumentationEvent::CLIENT_API_EXIT, "android.hidl.manager", "1.0", "IServiceManager", "get", &_hidl_args);
        }
    }
    #endif // __ANDROID_DEBUGGABLE__

    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<::android::sp<::android::hidl::base::V1_0::IBase>>(_hidl_out_service);

_hidl_error:
    _hidl_status.setFromStatusT(_hidl_err);
    return ::android::hardware::Return<::android::sp<::android::hidl::base::V1_0::IBase>>(_hidl_status);
}