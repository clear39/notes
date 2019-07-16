//  @   /work/workcodes/aosp-p9.x-auto-ga/vendor/nxp-opensource/imx/evs/EvsDisplay.cpp

class EvsDisplay : public IEvsDisplay {}

EvsDisplay::EvsDisplay()
{
    ALOGD("EvsDisplay instantiated");

    // Set up our self description
    // NOTE:  These are arbitrary values chosen for testing
    mInfo.displayId   = "evs hal Display";
    mInfo.vendorFlags = 3870;

    mWidth = DISPLAY_WIDTH;
    mHeight = DISPLAY_HEIGHT;
    mFormat = HAL_PIXEL_FORMAT_RGBA_8888;

    initialize();
}


// Main entry point
bool EvsDisplay::initialize()
{
    //
    //  Create the native full screen window and get a suitable configuration to match it
    //
    uint32_t layer = -1;
    sp<IDisplay> display = nullptr;
    while (display.get() == nullptr) {
        display = IDisplay::getService();
        if (display.get() == nullptr) {
            ALOGE("%s get display service failed", __func__);
            usleep(200000);
        }
    }

    display->getLayer(DISPLAY_BUFFER_NUM,[&](const auto& tmpError, const auto& tmpLayer) {
            if (tmpError == Error::NONE) {
                layer = tmpLayer;
            }
    });

    if (layer == (uint32_t)-1) {
        ALOGE("%s get layer failed", __func__);
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(mLock);
        mIndex = 0;
        mDisplay = display;
        mLayer = layer;
    }

    // allocate memory.
    fsl::Memory *buffer = nullptr;
    fsl::MemoryManager* allocator = fsl::MemoryManager::getInstance();
    fsl::MemoryDesc desc;
    desc.mWidth = mWidth;
    desc.mHeight = mHeight;
    desc.mFormat = mFormat;
    desc.mFslFormat = mFormat;
    desc.mProduceUsage |= fsl::USAGE_HW_TEXTURE | fsl::USAGE_HW_RENDER | fsl::USAGE_HW_VIDEO_ENCODER;
    desc.mFlag = 0;
    int ret = desc.checkFormat();
    if (ret != 0) {
        ALOGE("%s checkFormat failed", __func__);
        return false;
    }

    for (int i = 0; i < DISPLAY_BUFFER_NUM; i++) {
        buffer = nullptr;
        allocator->allocMemory(desc, &buffer);
        std::unique_lock<std::mutex> lock(mLock);
        mBuffers[i] = buffer;
    }

    return true;
}