//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/evs/1.0/default/EvsDisplay.cpp
EvsDisplay::EvsDisplay() {
    ALOGD("EvsDisplay instantiated");

    // Set up our self description
    // NOTE:  These are arbitrary values chosen for testing
    mInfo.displayId             = "Mock Display";
    mInfo.vendorFlags           = 3870;

    // Assemble the buffer description we'll use for our render target
    mBuffer.width       = 320;
    mBuffer.height      = 240;
    mBuffer.format      = HAL_PIXEL_FORMAT_RGBA_8888;
    mBuffer.usage       = GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;
    mBuffer.bufferId    = 0x3870;  // Arbitrary magic number for self recognition
    mBuffer.pixelSize   = 4;
}