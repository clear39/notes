
//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/DeviceDescriptor.cpp
DeviceVector DeviceVector::getDevicesFromTypeAddr(audio_devices_t type, const String8& address) const
{
    DeviceVector devices;
    for (const auto& device : *this) {
        /**
         * devicePort标签的 type属性值
         * 这里device->mAddress解析时没有值
         * 通过dumpsys得到 r_submix初始化 device->mAddress 为 “0”
        */
        if (device->type() == type && device->mAddress == address) {
            devices.add(device);
        }
    }
    return devices;
}
