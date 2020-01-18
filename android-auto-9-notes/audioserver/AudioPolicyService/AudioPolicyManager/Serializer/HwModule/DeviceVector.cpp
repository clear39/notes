/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/DeviceDescriptor.h
*/
class DeviceVector : public SortedVector<sp<DeviceDescriptor> >{

}


DeviceVector::DeviceVector() : SortedVector(), mDeviceTypes(AUDIO_DEVICE_NONE) {

}

/**
 * 通过 refreshTypes 统计所有支持的 audio_devices_t 
*/
audio_devices_t types() const { 
        return mDeviceTypes;
}

/**
 * 在添加和移除成员时会重新统计mDeviceTypes(audio_devices_t)
*/
void DeviceVector::refreshTypes()
{
    mDeviceTypes = AUDIO_DEVICE_NONE;
    for (size_t i = 0; i < size(); i++) {
        mDeviceTypes |= itemAt(i)->type();
    }
    ALOGV("DeviceVector::refreshTypes() mDeviceTypes %08x", mDeviceTypes);
}

void DeviceVector::add(const DeviceVector &devices)
{
    bool added = false;
    for (const auto& device : devices) {
        if (indexOf(device) < 0 && SortedVector::add(device) >= 0) {
            added = true;
        }
    }
    if (added) {
        refreshTypes();
    }
}

ssize_t DeviceVector::add(const sp<DeviceDescriptor>& item)
{
    ssize_t ret = indexOf(item);

    if (ret < 0) {
        ret = SortedVector::add(item);
        if (ret >= 0) {
            refreshTypes();
        }
    } else {
        ALOGW("DeviceVector::add device %08x already in", item->type());
        ret = -1;
    }
    return ret;
}

ssize_t DeviceVector::remove(const sp<DeviceDescriptor>& item)
{
    ssize_t ret = indexOf(item);

    if (ret < 0) {
        ALOGW("DeviceVector::remove device %08x not in", item->type());
    } else {
        ret = SortedVector::removeAt(ret);
        if (ret >= 0) {
            refreshTypes();
        }
    }
    return ret;
}