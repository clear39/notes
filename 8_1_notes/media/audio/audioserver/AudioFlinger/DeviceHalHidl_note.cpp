//	@frameworks/av/media/libaudiohal/ConversionHelperHidl.h
class ConversionHelperHidl {}

//	@frameworks/av/media/libaudiohal/include/media/audiohal/DeviceHalInterface.h
class DeviceHalInterface : public RefBase{}

//	@frameworks/av/media/libaudiohal/DeviceHalHidl.h
class DeviceHalHidl : public DeviceHalInterface, public ConversionHelperHidl{}



DeviceHalHidl::DeviceHalHidl(const sp<IDevice>& device)
        : ConversionHelperHidl("Device"), mDevice(device),
          mPrimaryDevice(IPrimaryDevice::castFrom(device)) {//	sp<IPrimaryDevice> mPrimaryDevice;  // Null if it's not a primary device.
}



// static 
::android::hardware::Return<::android::sp<IPrimaryDevice>> IPrimaryDevice::castFrom(const ::android::sp<IDevice>& parent, bool emitError) {
    return ::android::hardware::details::castInterface<IPrimaryDevice, IDevice, BpHwPrimaryDevice>(parent, "android.hardware.audio@2.0::IPrimaryDevice", emitError);
}



template <typename IChild, typename IParent, typename BpChild>//	IPrimaryDevice, IDevice, BpHwPrimaryDevice>
Return<sp<IChild>> castInterface(sp<IParent> parent, const char* childIndicator, bool emitError) {
    if (parent.get() == nullptr) {
        // casts always succeed with nullptrs.
        return nullptr;
    }
    Return<bool> canCastRet = details::canCastInterface(parent.get(), childIndicator, emitError);
    if (!canCastRet.isOk()) {
        // call fails, propagate the error if emitError
        return emitError ? details::StatusOf<bool, sp<IChild>>(canCastRet) : Return<sp<IChild>>(sp<IChild>(nullptr));
    }

    if (!canCastRet) {
        return sp<IChild>(nullptr); // cast failed.
    }
    // TODO b/32001926 Needs to be fixed for socket mode.
    if (parent->isRemote()) {
        // binderized mode. Got BpChild. grab the remote and wrap it.
        return sp<IChild>(new BpChild(toBinder<IParent>(parent)));
    }
    // Passthrough mode. Got BnChild and BsChild.
    return sp<IChild>(static_cast<IChild *>(parent.get()));
}


//	@system/libhidl/transport/HidlTransportUtils.cpp
Return<bool> canCastInterface(IBase* interface, const char* castTo, bool emitError) {
    if (interface == nullptr) {
        return false;
    }

    // b/68217907
    // Every HIDL interface is a base interface.
    //	const char* IDevice::descriptor("android.hardware.audio@2.0::IDevice");
   
    if (std::string(IBase::descriptor) == castTo) {
        return true;
    }

    bool canCast = false;
    auto chainRet = interface->interfaceChain([&](const hidl_vec<hidl_string> &types) {
        for (size_t i = 0; i < types.size(); i++) {
            if (types[i] == castTo) {
                canCast = true;
                break;
            }
        }
    });

    if (!chainRet.isOk()) {
        // call fails, propagate the error if emitError
        return emitError
                ? details::StatusOf<void, bool>(chainRet)
                : Return<bool>(false);
    }

    return canCast;
}


// Methods from ::android::hidl::base::V1_0::IBase follow.
::android::hardware::Return<void> IDevice::interfaceChain(interfaceChain_cb _hidl_cb){
    _hidl_cb({
        IDevice::descriptor,
        ::android::hidl::base::V1_0::IBase::descriptor,
    });
    return ::android::hardware::Void();
}