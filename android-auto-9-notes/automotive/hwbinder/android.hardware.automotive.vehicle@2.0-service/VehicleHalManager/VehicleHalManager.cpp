//  @ /work/workcodes/aosp-p9.x-auto-ga/hardware/interfaces/automotive/vehicle/2.0/IVehicleCallback.hal
interface IVehicleCallback {
    oneway onPropertyEvent(vec<VehiclePropValue> propValues);
    oneway onPropertySet(VehiclePropValue propValue);
    oneway onPropertySetError(StatusCode errorCode,int32_t propId,int32_t areaId);
};



interface IVehicle {
  getAllPropConfigs() generates (vec<VehiclePropConfig> propConfigs);
  getPropConfigs(vec<int32_t> props) generates (StatusCode status, vec<VehiclePropConfig> propConfigs);
  get(VehiclePropValue requestedPropValue) generates (StatusCode status, VehiclePropValue propValue);
  set(VehiclePropValue propValue) generates (StatusCode status);
  subscribe(IVehicleCallback callback, vec<SubscribeOptions> options)  generates (StatusCode status);
  unsubscribe(IVehicleCallback callback, int32_t propId)generates (StatusCode status);
  debugDump() generates (string s);
};



















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


Return<void> VehicleHalManager::getAllPropConfigs(getAllPropConfigs_cb _hidl_cb) {
    ALOGI("getAllPropConfigs called");
    hidl_vec<VehiclePropConfig> hidlConfigs;
    auto& halConfig = mConfigIndex->getAllConfigs();

    hidlConfigs.setToExternal(const_cast<VehiclePropConfig *>(halConfig.data()),halConfig.size());

    _hidl_cb(hidlConfigs);

    return Void();
}