
//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/evs/1.0/default/EvsEnumerator.cpp

EvsEnumerator::EvsEnumerator() {
    ALOGD("EvsEnumerator created");

    // Add sample camera data to our list of cameras
    // In a real driver, this would be expected to can the available hardware


    //  std::list<EvsEnumerator::CameraRecord>   EvsEnumerator::sCameraList;

    // const char EvsCamera::kCameraName_Backup[]    = "backup";
    sCameraList.emplace_back(EvsCamera::kCameraName_Backup);  
    sCameraList.emplace_back("LaneView");
    sCameraList.emplace_back("right turn");
}


//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/evs/1.0/default/EvsEnumerator.cpp
struct CameraRecord {
    CameraDesc          desc;
    wp<EvsCamera>       activeInstance;

    CameraRecord(const char *cameraId) : desc() { desc.cameraId = cameraId; }
};


Return<sp<IEvsCamera>> EvsEnumerator::openCamera(const hidl_string& cameraId) {
    ALOGD("openCamera");

    // Find the named camera
    CameraRecord *pRecord = nullptr;
    for (auto &&cam : sCameraList) {
        if (cam.desc.cameraId == cameraId) {
            // Found a match!
            pRecord = &cam;
            break;
        }
    }

    // Is this a recognized camera id?
    if (!pRecord) {
        ALOGE("Requested camera %s not found", cameraId.c_str());
        return nullptr;
    }

    // Has this camera already been instantiated by another caller?
    sp<EvsCamera> pActiveCamera = pRecord->activeInstance.promote();
    if (pActiveCamera != nullptr) {
        ALOGW("Killing previous camera because of new caller");
        closeCamera(pActiveCamera);
    }

    // Construct a camera instance for the caller
    pActiveCamera = new EvsCamera(cameraId.c_str());
    pRecord->activeInstance = pActiveCamera;
    if (pActiveCamera == nullptr) {
        ALOGE("Failed to allocate new EvsCamera object for %s\n", cameraId.c_str());
    }

    return pActiveCamera;
}


Return<sp<IEvsDisplay>> EvsEnumerator::openDisplay() {
    ALOGD("openDisplay");

    // If we already have a display active, then we need to shut it down so we can
    // give exclusive access to the new caller.
    sp<EvsDisplay> pActiveDisplay = sActiveDisplay.promote();
    if (pActiveDisplay != nullptr) {
        ALOGW("Killing previous display because of new caller");
        closeDisplay(pActiveDisplay);
    }

    // Create a new display interface and return it
    pActiveDisplay = new EvsDisplay();
    sActiveDisplay = pActiveDisplay;

    ALOGD("Returning new EvsDisplay object %p", pActiveDisplay.get());
    return pActiveDisplay;
}





