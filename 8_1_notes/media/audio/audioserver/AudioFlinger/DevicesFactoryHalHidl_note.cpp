class DevicesFactoryHalHidl : public DevicesFactoryHalInterface
{}


//	@frameworks/av/media/libaudiohal/DevicesFactoryHalHidl.cpp
DevicesFactoryHalHidl::DevicesFactoryHalHidl() {
    mDevicesFactory = IDevicesFactory::getService();	//通过hwbinder和android.hardware.audio@2.0-service通讯
    if (mDevicesFactory != 0) {
        // It is assumed that DevicesFactory is owned by AudioFlinger
        // and thus have the same lifespan.
        mDevicesFactory->linkToDeath(HalDeathHandler::getInstance(), 0 /*cookie*/);
    } else {
        ALOGE("Failed to obtain IDevicesFactory service, terminating process.");
        exit(1);
    }
}


// static
::android::sp<IDevicesFactory> IDevicesFactory::getService(const std::string &serviceName="default", bool getStub=false)) {
    using ::android::hardware::defaultServiceManager;
    using ::android::hardware::details::waitForHwService;
    using ::android::hardware::getPassthroughServiceManager;
    using ::android::hardware::Return;
    using ::android::sp;
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;

    sp<IDevicesFactory> iface = nullptr;

    const sp<::android::hidl::manager::V1_0::IServiceManager> sm = defaultServiceManager();
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
    const bool vintfHwbinder = (transport == Transport::HWBINDER);// 返回HWBINDER
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

    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride =  env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY);

#endif // __ANDROID_TREBLE__

    for (int tries = 0; !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)); tries++) {
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
            }continue;
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


    if (getStub || vintfPassthru || vintfLegacy) {
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
    }
    return iface;
}



status_t DevicesFactoryHalHidl::openDevice(const char *name, sp<DeviceHalInterface> *device) {
    if (mDevicesFactory == 0) return NO_INIT;
    IDevicesFactory::Device hidlDevice;
    status_t status = nameFromHal(name, &hidlDevice);
    if (status != OK) return status;
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mDevicesFactory->openDevice(
            hidlDevice,
            [&](Result r, const sp<IDevice>& result) {
                retval = r;
                if (retval == Result::OK) {
                    *device = new DeviceHalHidl(result);
                }
            });
    if (ret.isOk()) {
        if (retval == Result::OK) return OK;
        else if (retval == Result::INVALID_ARGUMENTS) return BAD_VALUE;
        else return NO_INIT;
    }
    return FAILED_TRANSACTION;
}



// static
status_t DevicesFactoryHalHidl::nameFromHal(const char *name, IDevicesFactory::Device *device) {
    if (strcmp(name, AUDIO_HARDWARE_MODULE_ID_PRIMARY) == 0) {
        *device = IDevicesFactory::Device::PRIMARY;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_A2DP) == 0) {
        *device = IDevicesFactory::Device::A2DP;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_USB) == 0) {
        *device = IDevicesFactory::Device::USB;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX) == 0) {
        *device = IDevicesFactory::Device::R_SUBMIX;
        return OK;
    } else if(strcmp(name, AUDIO_HARDWARE_MODULE_ID_STUB) == 0) {
        *device = IDevicesFactory::Device::STUB;
        return OK;
    }
    ALOGE("Invalid device name %s", name);
    return BAD_VALUE;
}
