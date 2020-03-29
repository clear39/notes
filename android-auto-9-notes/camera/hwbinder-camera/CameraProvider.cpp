//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/camera/provider/2.4/default/CameraProvider.cpp
CameraProvider::CameraProvider() :
        camera_module_callbacks_t({sCameraDeviceStatusChange,sTorchModeStatusChange}) {
    mInitFailed = initialize();
}


/**
 * static callback forwarding methods from HAL to instance
 */
void CameraProvider::sCameraDeviceStatusChange(const struct camera_module_callbacks* callbacks,int camera_id,int new_status) {
    CameraProvider* cp = const_cast<CameraProvider*>(static_cast<const CameraProvider*>(callbacks));
    bool found = false;

    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    Mutex::Autolock _l(cp->mCbLock);
    char cameraId[kMaxCameraIdLen];
    snprintf(cameraId, sizeof(cameraId), "%d", camera_id);
    std::string cameraIdStr(cameraId);
    cp->mCameraStatusMap[cameraIdStr] = (camera_device_status_t) new_status;
    if (cp->mCallbacks != nullptr) {
        CameraDeviceStatus status = (CameraDeviceStatus) new_status;
        for (auto const& deviceNamePair : cp->mCameraDeviceNames) {
            if (cameraIdStr.compare(deviceNamePair.first) == 0) {
                cp->mCallbacks->cameraDeviceStatusChange(deviceNamePair.second, status);
                found = true;
            }
        }

        switch (status) {
        case CameraDeviceStatus::PRESENT:
        case CameraDeviceStatus::ENUMERATING:
            if (!found) {
                cp->addDeviceNames(camera_id, status, true);
            }
            break;
        case CameraDeviceStatus::NOT_PRESENT:
            if (found) {
                cp->removeDeviceNames(camera_id);
            }
        }
    }
}

void CameraProvider::sTorchModeStatusChange(const struct camera_module_callbacks* callbacks,const char* camera_id,int new_status) {
    CameraProvider* cp = const_cast<CameraProvider*>(static_cast<const CameraProvider*>(callbacks));

    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    Mutex::Autolock _l(cp->mCbLock);
    if (cp->mCallbacks != nullptr) {
        std::string cameraIdStr(camera_id);
        TorchModeStatus status = (TorchModeStatus) new_status;
        for (auto const& deviceNamePair : cp->mCameraDeviceNames) {
            if (cameraIdStr.compare(deviceNamePair.first) == 0) {
                cp->mCallbacks->torchModeStatusChange(deviceNamePair.second, status);
            }
        }
    }
}

/***
 * 
 * 
 * 
*/
bool CameraProvider::initialize() {
    camera_module_t *rawModule;
    int err = hw_get_module(CAMERA_HARDWARE_MODULE_ID,(const hw_module_t **)&rawModule);
    if (err < 0) {
        ALOGE("Could not load camera HAL module: %d (%s)", err, strerror(-err));
        return true;
    }

    mModule = new CameraModule(rawModule);
    err = mModule->init();
    if (err != OK) {
        ALOGE("Could not initialize camera HAL module: %d (%s)", err, strerror(-err));
        mModule.clear();
        return true;
    }
    ALOGI("Loaded \"%s\" camera module", mModule->getModuleName());

    // Setup vendor tags here so HAL can setup vendor keys in camera characteristics
    VendorTagDescriptor::clearGlobalVendorTagDescriptor();
    if (!setUpVendorTags()) {
        ALOGE("%s: Vendor tag setup failed, will not be available.", __FUNCTION__);
    }

    // Setup callback now because we are going to try openLegacy next
    err = mModule->setCallbacks(this);
    if (err != OK) {
        ALOGE("Could not set camera module callback: %d (%s)", err, strerror(-err));
        mModule.clear();
        return true;
    }

    mPreferredHal3MinorVersion = property_get_int32("ro.vendor.camera.wrapper.hal3TrebleMinorVersion", 3);
    ALOGV("Preferred HAL 3 minor version is %d", mPreferredHal3MinorVersion);
    switch(mPreferredHal3MinorVersion) {
        case 2:
        case 3:
            // OK
            break;
        default:
            ALOGW("Unknown minor camera device HAL version %d in property " "'camera.wrapper.hal3TrebleMinorVersion', defaulting to 3", mPreferredHal3MinorVersion);
            mPreferredHal3MinorVersion = 3;
    }

    mNumberOfLegacyCameras = mModule->getNumberOfCameras();
    for (int i = 0; i < mNumberOfLegacyCameras; i++) {
        struct camera_info info;
        auto rc = mModule->getCameraInfo(i, &info);
        if (rc != NO_ERROR) {
            ALOGE("%s: Camera info query failed!", __func__);
            mModule.clear();
            return true;
        }

        if (checkCameraVersion(i, info) != OK) {
            ALOGE("%s: Camera version check failed!", __func__);
            mModule.clear();
            return true;
        }

        char cameraId[kMaxCameraIdLen];
        snprintf(cameraId, sizeof(cameraId), "%d", i);
        std::string cameraIdStr(cameraId);
        mCameraStatusMap[cameraIdStr] = CAMERA_DEVICE_STATUS_PRESENT;

        addDeviceNames(i);
    }

    return false; // mInitFailed
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/***
 * CameraProvider::initialize()
 * ---->
 * 
*/
bool CameraProvider::setUpVendorTags() {
    ATRACE_CALL();
    vendor_tag_ops_t vOps = vendor_tag_ops_t();

    // Check if vendor operations have been implemented
    if (!mModule->isVendorTagDefined()) {
        ALOGI("%s: No vendor tags defined for this device.", __FUNCTION__);
        return true;
    }

    mModule->getVendorTagOps(&vOps);

    // Ensure all vendor operations are present
    if (vOps.get_tag_count == nullptr || vOps.get_all_tags == nullptr ||
            vOps.get_section_name == nullptr || vOps.get_tag_name == nullptr ||
            vOps.get_tag_type == nullptr) {
        ALOGE("%s: Vendor tag operations not fully defined. Ignoring definitions."
               , __FUNCTION__);
        return false;
    }

    // Read all vendor tag definitions into a descriptor
    sp<VendorTagDescriptor> desc;
    status_t res;
    if ((res = VendorTagDescriptor::createDescriptorFromOps(&vOps, /*out*/desc))
            != OK) {
        ALOGE("%s: Could not generate descriptor from vendor tag operations,"
              "received error %s (%d). Camera clients will not be able to use"
              "vendor tags", __FUNCTION__, strerror(res), res);
        return false;
    }

    // Set the global descriptor to use with camera metadata
    VendorTagDescriptor::setAsGlobalVendorTagDescriptor(desc);
    const SortedVector<String8>* sectionNames = desc->getAllSectionNames();
    size_t numSections = sectionNames->size();
    std::vector<std::vector<VendorTag>> tagsBySection(numSections);
    int tagCount = desc->getTagCount();
    std::vector<uint32_t> tags(tagCount);
    desc->getTagArray(tags.data());
    for (int i = 0; i < tagCount; i++) {
        VendorTag vt;
        vt.tagId = tags[i];
        vt.tagName = desc->getTagName(tags[i]);
        vt.tagType = (CameraMetadataType) desc->getTagType(tags[i]);
        ssize_t sectionIdx = desc->getSectionIndex(tags[i]);
        tagsBySection[sectionIdx].push_back(vt);
    }
    mVendorTagSections.resize(numSections);
    for (size_t s = 0; s < numSections; s++) {
        mVendorTagSections[s].sectionName = (*sectionNames)[s].string();
        mVendorTagSections[s].tags = tagsBySection[s];
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> CameraProvider::getVendorTags(getVendorTags_cb _hidl_cb)  {
    _hidl_cb(Status::OK, mVendorTagSections);
    return Void();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> CameraProvider::getCameraIdList(getCameraIdList_cb _hidl_cb)  {
    std::vector<hidl_string> deviceNameList;
    for (auto const& deviceNamePair : mCameraDeviceNames) {
        if (mCameraStatusMap[deviceNamePair.first] == CAMERA_DEVICE_STATUS_PRESENT) {
            deviceNameList.push_back(deviceNamePair.second);
        }
    }
    hidl_vec<hidl_string> hidlDeviceNameList(deviceNameList);
    _hidl_cb(Status::OK, hidlDeviceNameList);
    return Void();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> CameraProvider::isSetTorchModeSupported(isSetTorchModeSupported_cb _hidl_cb) {
    bool support = mModule->isSetTorchModeSupported();
    _hidl_cb (Status::OK, support);
    return Void();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> CameraProvider::getCameraDeviceInterface_V1_x(const hidl_string& cameraDeviceName, getCameraDeviceInterface_V1_x_cb _hidl_cb)  {
    std::string cameraId, deviceVersion;
    bool match = matchDeviceName(cameraDeviceName, &deviceVersion, &cameraId);
    if (!match) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    std::string deviceName(cameraDeviceName.c_str());
    ssize_t index = mCameraDeviceNames.indexOf(std::make_pair(cameraId, deviceName));
    if (index == NAME_NOT_FOUND) { // Either an illegal name or a device version mismatch
        Status status = Status::OK;
        ssize_t idx = mCameraIds.indexOf(cameraId);
        if (idx == NAME_NOT_FOUND) {
            ALOGE("%s: cannot find camera %s!", __FUNCTION__, cameraId.c_str());
            status = Status::ILLEGAL_ARGUMENT;
        } else { // invalid version
            ALOGE("%s: camera device %s does not support version %s!", __FUNCTION__, cameraId.c_str(), deviceVersion.c_str());
            status = Status::OPERATION_NOT_SUPPORTED;
        }
        _hidl_cb(status, nullptr);
        return Void();
    }

    if (mCameraStatusMap.count(cameraId) == 0 ||  mCameraStatusMap[cameraId] != CAMERA_DEVICE_STATUS_PRESENT) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    sp<android::hardware::camera::device::V1_0::implementation::CameraDevice> device = new android::hardware::camera::device::V1_0::implementation::CameraDevice(mModule, cameraId, mCameraDeviceNames);

    if (device == nullptr) {
        ALOGE("%s: cannot allocate camera device for id %s", __FUNCTION__, cameraId.c_str());
        _hidl_cb(Status::INTERNAL_ERROR, nullptr);
        return Void();
    }

    if (device->isInitFailed()) {
        ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraId.c_str());
        device = nullptr;
        _hidl_cb(Status::INTERNAL_ERROR, nullptr);
        return Void();
    }

    _hidl_cb (Status::OK, device);
    return Void();
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> CameraProvider::getCameraDeviceInterface_V3_x(const hidl_string& cameraDeviceName, getCameraDeviceInterface_V3_x_cb _hidl_cb)  {
    std::string cameraId, deviceVersion;
    bool match = matchDeviceName(cameraDeviceName, &deviceVersion, &cameraId);
    if (!match) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    std::string deviceName(cameraDeviceName.c_str());
    ssize_t index = mCameraDeviceNames.indexOf(std::make_pair(cameraId, deviceName));
    if (index == NAME_NOT_FOUND) { // Either an illegal name or a device version mismatch
        Status status = Status::OK;
        ssize_t idx = mCameraIds.indexOf(cameraId);
        if (idx == NAME_NOT_FOUND) {
            ALOGE("%s: cannot find camera %s!", __FUNCTION__, cameraId.c_str());
            status = Status::ILLEGAL_ARGUMENT;
        } else { // invalid version
            ALOGE("%s: camera device %s does not support version %s!", __FUNCTION__, cameraId.c_str(), deviceVersion.c_str());
            status = Status::OPERATION_NOT_SUPPORTED;
        }
        _hidl_cb(status, nullptr);
        return Void();
    }

    if (mCameraStatusMap.count(cameraId) == 0 || mCameraStatusMap[cameraId] != CAMERA_DEVICE_STATUS_PRESENT) {
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    sp<android::hardware::camera::device::V3_2::ICameraDevice> device;
    if (deviceVersion == kHAL3_4) {
        ALOGV("Constructing v3.4 camera device");
        sp<android::hardware::camera::device::V3_2::implementation::CameraDevice> deviceImpl = new android::hardware::camera::device::V3_4::implementation::CameraDevice(mModule, cameraId, mCameraDeviceNames);
        if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
            ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraId.c_str());
            device = nullptr;
            _hidl_cb(Status::INTERNAL_ERROR, nullptr);
            return Void();
        }

        device = deviceImpl;
        _hidl_cb (Status::OK, device);
        return Void();
    }

    // Since some Treble HAL revisions can map to the same legacy HAL version(s), we default
    // to the newest possible Treble HAL revision, but allow for override if needed via
    // system property.
    switch (mPreferredHal3MinorVersion) {
        case 2: { // Map legacy camera device v3 HAL to Treble camera device HAL v3.2
            ALOGV("Constructing v3.2 camera device");
            sp<android::hardware::camera::device::V3_2::implementation::CameraDevice> deviceImpl =
                    new android::hardware::camera::device::V3_2::implementation::CameraDevice(
                    mModule, cameraId, mCameraDeviceNames);
            if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
                ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraId.c_str());
                device = nullptr;
                _hidl_cb(Status::INTERNAL_ERROR, nullptr);
                return Void();
            }
            device = deviceImpl;
            break;
        }
        case 3: { // Map legacy camera device v3 HAL to Treble camera device HAL v3.3
            ALOGV("Constructing v3.3 camera device");
            sp<android::hardware::camera::device::V3_2::implementation::CameraDevice> deviceImpl =
                    new android::hardware::camera::device::V3_3::implementation::CameraDevice(
                    mModule, cameraId, mCameraDeviceNames);
            if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
                ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraId.c_str());
                device = nullptr;
                _hidl_cb(Status::INTERNAL_ERROR, nullptr);
                return Void();
            }
            device = deviceImpl;
            break;
        }
        default:
            ALOGE("%s: Unknown HAL minor version %d!", __FUNCTION__, mPreferredHal3MinorVersion);
            device = nullptr;
            _hidl_cb(Status::INTERNAL_ERROR, nullptr);
            return Void();
    }
    _hidl_cb (Status::OK, device);
    return Void();
}