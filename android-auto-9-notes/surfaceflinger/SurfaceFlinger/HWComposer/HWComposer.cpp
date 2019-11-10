//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/services/surfaceflinger/DisplayHardware/HWComposer.cpp
/**
 * 
*/
HWComposer::HWComposer(std::unique_ptr<android::Hwc2::Composer> composer)
      : mHwcDevice(std::make_unique<HWC2::Device>(std::move(composer))) {}