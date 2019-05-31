//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/evs/1.0/default/EvsCamera.cpp
class EvsCamera : public IEvsCamera {

}



EvsCamera::EvsCamera(const char *id) :
        mFramesAllowed(0),
        mFramesInUse(0),
        mStreamState(STOPPED) {

    ALOGD("EvsCamera instantiated");

    mDescription.cameraId = id;

    // Set up dummy data for testing
    if (mDescription.cameraId == kCameraName_Backup) {
        mWidth  = 640;          // full NTSC/VGA
        mHeight = 480;          // full NTSC/VGA
        mDescription.vendorFlags = 0xFFFFFFFF;   // Arbitrary value
    } else {
        mWidth  = 320;          // 1/2 NTSC/VGA
        mHeight = 240;          // 1/2 NTSC/VGA
    }

    mFormat = HAL_PIXEL_FORMAT_RGBA_8888;
    mUsage  = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_CAMERA_WRITE | GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_RARELY;
}