
//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/camera/libcameraservice/common/CameraProviderManager.h
struct ProviderInfo :  virtual public hardware::camera::provider::V2_4::ICameraProviderCallback, virtual public hardware::hidl_death_recipient
{

}


//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/camera/libcameraservice/common/CameraProviderManager.cpp
CameraProviderManager::ProviderInfo::ProviderInfo(const std::string &providerName,sp<provider::V2_4::ICameraProvider>& interface,CameraProviderManager *manager) :
        mProviderName(providerName),
        mInterface(interface),
        mProviderTagid(generateVendorTagId(providerName)),
        mUniqueDeviceCount(0),
        mManager(manager) {
    (void) mManager;
}

status_t CameraProviderManager::ProviderInfo::initialize() {
    /**
     * 将 mProviderName 分离，以 “legacy/0”为例,则mType为legacy，mId为0
    */
    status_t res = parseProviderName(mProviderName, &mType, &mId);
    if (res != OK) {
        ALOGE("%s: Invalid provider name, ignoring", __FUNCTION__);
        return BAD_VALUE;
    }
    ALOGI("Connecting to new camera provider: %s, isRemote? %d", mProviderName.c_str(), mInterface->isRemote());
    // cameraDeviceStatusChange callbacks may be called (and causing new devices added)
    // before setCallback returns
    //这里callback为ICameraProviderCallback
    hardware::Return<Status> status = mInterface->setCallback(this);
    if (!status.isOk()) {
        ALOGE("%s: Transaction error setting up callbacks with camera provider '%s': %s",  __FUNCTION__, mProviderName.c_str(), status.description().c_str());
        return DEAD_OBJECT;
    }
    if (status != Status::OK) {
        ALOGE("%s: Unable to register callbacks with camera provider '%s'",  __FUNCTION__, mProviderName.c_str());
        return mapToStatusT(status);
    }

    hardware::Return<bool> linked = mInterface->linkToDeath(this, /*cookie*/ mId);
    if (!linked.isOk()) {
        ALOGE("%s: Transaction error in linking to camera provider '%s' death: %s", __FUNCTION__, mProviderName.c_str(), linked.description().c_str());
        return DEAD_OBJECT;
    } else if (!linked) {
        ALOGW("%s: Unable to link to provider '%s' death notifications",  __FUNCTION__, mProviderName.c_str());
    }

    // Get initial list of camera devices, if any
    std::vector<std::string> devices;
    hardware::Return<void> ret = mInterface->getCameraIdList([&status, &devices](Status idStatus,const hardware::hidl_vec<hardware::hidl_string>& cameraDeviceNames) {
        status = idStatus;
        if (status == Status::OK) {
            for (size_t i = 0; i < cameraDeviceNames.size(); i++) {
                devices.push_back(cameraDeviceNames[i]);
            }
        } });
    if (!ret.isOk()) {
        ALOGE("%s: Transaction error in getting camera ID list from provider '%s': %s",__FUNCTION__, mProviderName.c_str(), linked.description().c_str());
        return DEAD_OBJECT;
    }
    if (status != Status::OK) {
        ALOGE("%s: Unable to query for camera devices from provider '%s'",__FUNCTION__, mProviderName.c_str());
        return mapToStatusT(status);
    }

    sp<StatusListener> listener = mManager->getStatusListener();
    for (auto& device : devices) {
        std::string id;
        status_t res = addDevice(device,ardware::camera::common::V1_0::CameraDeviceStatus::PRESENT, &id);
        if (res != OK) {
            ALOGE("%s: Unable to enumerate camera device '%s': %s (%d)",__FUNCTION__, device.c_str(), strerror(-res), res);
            continue;
        }
    }

    ALOGI("Camera provider %s ready with %zu camera devices", mProviderName.c_str(), mDevices.size());

    mInitialized = true;
    return OK;
}



status_t CameraProviderManager::ProviderInfo::addDevice(const std::string& name,CameraDeviceStatus initialStatus, /*out*/ std::string* parsedId) {

    ALOGI("Enumerating new camera device: %s", name.c_str());

    uint16_t major, minor;
    std::string type, id;

    /**
     * 解析 name 获得 major，minor，type，id
    */
    status_t res = parseDeviceName(name, &major, &minor, &type, &id);
    if (res != OK) {
        return res;
    }
    if (type != mType) {
        ALOGE("%s: Device type %s does not match provider type %s", __FUNCTION__,  type.c_str(), mType.c_str());
        return BAD_VALUE;
    }
    /**
     * 用于判断是否已经添加
    */
    if (mManager->isValidDeviceLocked(id, major)) {
        ALOGE("%s: Device %s: ID %s is already in use for device major version %d", __FUNCTION__, name.c_str(), id.c_str(), major);
        return BAD_VALUE;
    }

    std::unique_ptr<DeviceInfo> deviceInfo;
    switch (major) {
        case 1:
            deviceInfo = initializeDeviceInfo<DeviceInfo1>(name, mProviderTagid, id, minor);
            break;
        case 3:
            deviceInfo = initializeDeviceInfo<DeviceInfo3>(name, mProviderTagid, id, minor);
            break;
        default:
            ALOGE("%s: Device %s: Unknown HIDL device HAL major version %d:", __FUNCTION__, name.c_str(), major);
            return BAD_VALUE;
    }
    if (deviceInfo == nullptr) return BAD_VALUE;
    deviceInfo->mStatus = initialStatus;
    bool isAPI1Compatible = deviceInfo->isAPI1Compatible();

    mDevices.push_back(std::move(deviceInfo));

    mUniqueCameraIds.insert(id);
    if (isAPI1Compatible) {
        mUniqueAPI1CompatibleCameraIds.push_back(id);
    }

    if (parsedId != nullptr) {
        *parsedId = id;
    }
    return OK;
}










////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * device::V3_2::ICameraDevice  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/camera/device/1.0/ICameraDevice.hal
 * 
 * 
*/
template<>
sp<device::V1_0::ICameraDevice>
CameraProviderManager::ProviderInfo::getDeviceInterface<device::V1_0::ICameraDevice>(const std::string &name) const {
    Status status;
    sp<device::V1_0::ICameraDevice> cameraInterface;
    hardware::Return<void> ret;
    ret = mInterface->getCameraDeviceInterface_V1_x(name, [&status, &cameraInterface](Status s, sp<device::V1_0::ICameraDevice> interface) {
                status = s;
                cameraInterface = interface;
            });
    if (!ret.isOk()) {
        ALOGE("%s: Transaction error trying to obtain interface for camera device %s: %s", __FUNCTION__, name.c_str(), ret.description().c_str());
        return nullptr;
    }
    if (status != Status::OK) {
        ALOGE("%s: Unable to obtain interface for camera device %s: %s", __FUNCTION__,name.c_str(), statusToString(status));
        return nullptr;
    }
    return cameraInterface;
}

/**
 * device::V3_2::ICameraDevice  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/camera/device/3.2/ICameraDevice.hal
 * 
 * 
*/
template<>
sp<device::V3_2::ICameraDevice>
CameraProviderManager::ProviderInfo::getDeviceInterface<device::V3_2::ICameraDevice>(const std::string &name) const {
    Status status;
    sp<device::V3_2::ICameraDevice> cameraInterface;
    hardware::Return<void> ret;
    ret = mInterface->getCameraDeviceInterface_V3_x(name, [&status, &cameraInterface](Status s, sp<device::V3_2::ICameraDevice> interface) {
                status = s;
                cameraInterface = interface;
            });
    if (!ret.isOk()) {
        ALOGE("%s: Transaction error trying to obtain interface for camera device %s: %s",__FUNCTION__, name.c_str(), ret.description().c_str());
        return nullptr;
    }
    if (status != Status::OK) {
        ALOGE("%s: Unable to obtain interface for camera device %s: %s", __FUNCTION__,name.c_str(), statusToString(status));
        return nullptr;
    }
    return cameraInterface;
}