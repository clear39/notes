//  hardware/interfaces/automotive/vehicle/2.0/default/VehicleService.cpp

int main(int /* argc */, char* /* argv */ []) {
    //  hardware/interfaces/automotive/vehicle/2.0/default/common/src/VehiclePropertyStore.cpp
    //  VehiclePropertyStore 默认构造函数，没有实现部分
    auto store = std::make_unique<VehiclePropertyStore>();
    //  vendor/nxp-opensource/imx/vehicle/impl/vhal_v2_0/EmulatedVehicleHal.cpp
    auto hal = std::make_unique<impl::EmulatedVehicleHal>(store.get());
    auto emulator = std::make_unique<impl::VehicleEmulator>(hal.get());
    auto service = std::make_unique<VehicleHalManager>(hal.get());

    configureRpcThreadpool(4, true /* callerWillJoin */);

    ALOGI("Registering as service...");
    status_t status = service->registerAsService();

    if (status != OK) {
        ALOGE("Unable to register vehicle service (%d)", status);
        return 1;
    }

    ALOGI("Ready");
    joinRpcThreadpool();

    return 1;
}