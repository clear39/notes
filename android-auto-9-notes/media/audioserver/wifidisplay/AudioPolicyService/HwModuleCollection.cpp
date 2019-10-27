

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/HwModule.cpp
/**
 * 
 * AudioPolicyManager::setDeviceConnectionStateInt
 * --> HwModuleCollection::getDeviceDescriptor
*/
sp<DeviceDescriptor> HwModuleCollection::getDeviceDescriptor(const audio_devices_t device,
                                                             const char *device_address,
                                                             const char *device_name,
                                                             bool matchAdress) const
{
    String8 address = (device_address == nullptr) ? String8("") : String8(device_address);
    // handle legacy remote submix case where the address was not always specified
    if (device_distinguishes_on_address(device) && (address.length() == 0)) {
        address = String8("0");
    }

    for (const auto& hwModule : *this) {
        DeviceVector declaredDevices = hwModule->getDeclaredDevices();
        DeviceVector deviceList = declaredDevices.getDevicesFromTypeAddr(device, address);
        if (!deviceList.isEmpty()) {
            return deviceList.itemAt(0);
        }
        if (!matchAdress) {
            deviceList = declaredDevices.getDevicesFromType(device);
            if (!deviceList.isEmpty()) {
                return deviceList.itemAt(0);
            }
        }
    }

    sp<DeviceDescriptor> devDesc = new DeviceDescriptor(device);
    devDesc->setName(String8(device_name));
    devDesc->mAddress = address;
    return devDesc;
}