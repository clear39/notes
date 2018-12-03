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
