//	@out/soong/.intermediates/hardware/interfaces/radio/deprecated/1.0/android.hardware.radio.deprecated@1.0_genc++/gen/android/hardware/radio/deprecated/1.0/OemHookAll.cpp

::android::status_t IOemHook::registerAsService(const std::string &serviceName = "slot1") {
    ::android::hardware::details::onRegistration("android.hardware.radio.deprecated@1.0", "IOemHook", serviceName);

    const ::android::sp<::android::hidl::manager::V1_0::IServiceManager> sm  = ::android::hardware::defaultServiceManager();//sm =  new BpHwServiceManager(BpHwBinder(0)));
    if (sm == nullptr) {
        return ::android::INVALID_OPERATION;
    }
    ::android::hardware::Return<bool> ret = sm->add(serviceName.c_str(), this);
    return ret.isOk() && ret ? ::android::OK : ::android::UNKNOWN_ERROR;
}

