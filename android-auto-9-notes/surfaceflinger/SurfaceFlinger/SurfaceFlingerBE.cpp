//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
SurfaceFlingerBE::SurfaceFlingerBE()
      : mHwcServiceName(getHwcServiceName()),
        mRenderEngine(nullptr),
        mFrameBuckets(),
        mTotalTime(0),
        mLastSwapTime(0),
        mComposerSequenceId(0) {
}

std::string getHwcServiceName() {
    char value[PROPERTY_VALUE_MAX] = {};
    property_get("debug.sf.hwc_service_name", value, "default");
    ALOGI("Using HWComposer service: '%s'", value);
    return std::string(value);
}