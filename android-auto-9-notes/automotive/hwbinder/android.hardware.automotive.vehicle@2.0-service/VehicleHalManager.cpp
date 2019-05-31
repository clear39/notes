//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/common/src/VehicleHalManager.cpp
class VehicleHalManager : public IVehicle {}

VehicleHalManager::VehicleHalManager(VehicleHal* vehicleHal)
    : mHal(vehicleHal),mSubscriptionManager(std::bind(&VehicleHalManager::onAllClientsUnsubscribed,this, std::placeholders::_1)) {
    init();
}


void VehicleHalManager::init() {
    ALOGI("VehicleHalManager::init");

    mHidlVecOfVehiclePropValuePool.resize(kMaxHidlVecOfVehiclPropValuePoolSize);


    mBatchingConsumer.run(&mEventQueue,
                          kHalEventBatchingTimeWindow,
                          std::bind(&VehicleHalManager::onBatchHalEvent,this, _1));
    // init函数实现在最顶层父类VehicleHal @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/common/include/vhal_v2_0/VehicleHal.h 中
    mHal->init(&mValueObjectPool, 
               std::bind(&VehicleHalManager::onHalEvent, this, _1),
               std::bind(&VehicleHalManager::onHalPropertySetError, this,_1, _2, _3));

    // Initialize index with vehicle configurations received from VehicleHal.
    auto supportedPropConfigs = mHal->listProperties();
    mConfigIndex.reset(new VehiclePropConfigIndex(supportedPropConfigs));

    std::vector<int32_t> supportedProperties(supportedPropConfigs.size());
    for (const auto& config : supportedPropConfigs) {
        supportedProperties.push_back(config.prop);
    }
}