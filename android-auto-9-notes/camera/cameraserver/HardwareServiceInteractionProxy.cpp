//  @   /home/xqli/work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/camera/libcameraservice/common/CameraProviderManager.h
// Standard use case - call into the normal generated static methods which invoke
// the real hardware service manager
struct HardwareServiceInteractionProxy : public ServiceInteractionProxy {
    virtual bool registerForNotifications(const std::string &serviceName, const sp<hidl::manager::V1_0::IServiceNotification> &notification) override {
        return hardware::camera::provider::V2_4::ICameraProvider::registerForNotifications(serviceName, notification);
    }
    virtual sp<hardware::camera::provider::V2_4::ICameraProvider> getService(const std::string &serviceName) override {
        return hardware::camera::provider::V2_4::ICameraProvider::getService(serviceName);
    }
};


    // Tiny proxy for the static methods in a HIDL interface that communicate with the hardware
// service manager, to be replacable in unit tests with a fake.
struct ServiceInteractionProxy {
    virtual bool registerForNotifications(const std::string &serviceName,const sp<hidl::manager::V1_0::IServiceNotification> &notification) = 0;
    virtual sp<hardware::camera::provider::V2_4::ICameraProvider> getService(const std::string &serviceName) = 0;
    virtual ~ServiceInteractionProxy() {}
};