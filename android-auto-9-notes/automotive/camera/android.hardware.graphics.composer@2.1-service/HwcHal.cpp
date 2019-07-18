//  @   hardware/interfaces/graphics/composer/2.1/utils/passthrough/include/composer-passthrough/2.1/HwcHal.h

using HwcHal = detail::HwcHalImpl<hal::ComposerHal>;





bool initWithModule(const hw_module_t* module) {
    hwc2_device_t* device;
    int result = hwc2_open(module, &device);
    if (result) {
        ALOGE("failed to open hwcomposer2 device: %s", strerror(-result));
        return false;
    }

    return initWithDevice(std::move(device), true);
}