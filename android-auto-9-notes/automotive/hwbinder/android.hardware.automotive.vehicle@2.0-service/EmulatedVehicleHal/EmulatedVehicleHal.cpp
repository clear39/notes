//  @/work/workcodes/aosp-p9.x-auto-ga/vendor/nxp-opensource/imx/vehicle/impl/vhal_v2_0/EmulatedVehicleHal.cpp

class EmulatedVehicleHal : public EmulatedVehicleHalIface {}

/**
 * EmulatedVehicleHalIface  @   vendor/nxp-opensource/imx/vehicle/impl/vhal_v2_0/VehicleEmulator.h
 * VehicleHal @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/common/include/vhal_v2_0/VehicleHal.h
 *  */ 
class EmulatedVehicleHalIface : public VehicleHal {}


EmulatedVehicleHal::EmulatedVehicleHal(VehiclePropertyStore* propStore)
    : mPropStore(propStore),
      //  kHvacPowerProperties @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/DefaultConfig.h
      mHvacPowerProps(std::begin(kHvacPowerProperties), std::end(kHvacPowerProperties)),
      mRecurrentTimer(std::bind(&EmulatedVehicleHal::onContinuousPropertyTimer, this, std::placeholders::_1)),
      mLinearFakeValueGenerator(std::make_unique<LinearFakeValueGenerator>(std::bind(&EmulatedVehicleHal::onFakeValueGenerated, this, std::placeholders::_1))),
      mJsonFakeValueGenerator(std::make_unique<JsonFakeValueGenerator>(std::bind(&EmulatedVehicleHal::onFakeValueGenerated, this, std::placeholders::_1))) {
    initStaticConfig();
    for (size_t i = 0; i < arraysize(kVehicleProperties); i++) {
        mPropStore->registerProperty(kVehicleProperties[i].config);
    }
}


void EmulatedVehicleHal::initStaticConfig() {
    for (auto&& it = std::begin(kVehicleProperties); it != std::end(kVehicleProperties); ++it) {
        const auto& cfg = it->config;
        VehiclePropertyStore::TokenFunction tokenFunction = nullptr;

        switch (cfg.prop) {
            case OBD2_FREEZE_FRAME: {
                tokenFunction = [](const VehiclePropValue& propValue) {
                    return propValue.timestamp;
                };
                break;
            }
            default:
                break;
        }

        mPropStore->registerProperty(cfg, tokenFunction);
    }
}


/***
*在 VehicleEmulator 的够着函数中调用 mHal->registerEmulator(this)
*/
void EmulatedVehicleHalIface::registerEmulator(VehicleEmulator* emulator) {
    ALOGI("%s, emulator: %p", __func__, emulator);
    std::lock_guard<std::mutex> g(mEmulatorLock);
    mEmulator = emulator;
}

inline constexpr VehicleArea getPropArea(int32_t prop) {
    return static_cast<VehicleArea>(prop & toInt(VehicleArea::MASK));
}

inline constexpr bool isGlobalProp(int32_t prop) {
    return getPropArea(prop) == VehicleArea::GLOBAL;
}

//  @ /work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/common/include/vhal_v2_0/VehicleHal.h
/***
* 该函数在 VehicleHalManager::init中调用
*/
void VehicleHal::init(
        VehiclePropValuePool* valueObjectPool,
        const HalEventFunction& onHalEvent,
        const HalErrorFunction& onHalError) {
        mValuePool = valueObjectPool;
        mOnHalEvent = onHalEvent;
        mOnHalPropertySetError = onHalError;
        // 这里由于 EmulatedVehicleHal 子类实现了 onCreate 函数 ，所以转到 EmulatedVehicleHal中
        onCreate();
}

static bool isDiagnosticProperty(VehiclePropConfig propConfig) {
    switch (propConfig.prop) {
        case OBD2_LIVE_FRAME:
        case OBD2_FREEZE_FRAME:
        case OBD2_FREEZE_FRAME_CLEAR:
        case OBD2_FREEZE_FRAME_INFO:
            return true;
    }
    return false;
}

// Parse supported properties list and generate vector of property values to hold current values.
void EmulatedVehicleHal::onCreate() {
    static constexpr bool shouldUpdateStatus = true;

    // kVehicleProperties 全局变量定义在 /work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/DefaultConfig.h
    for (auto& it : kVehicleProperties) {
        VehiclePropConfig cfg = it.config;
        int32_t numAreas = cfg.areaConfigs.size();

        if (isDiagnosticProperty(cfg)) {
            // do not write an initial empty value for the diagnostic properties
            // as we will initialize those separately.
            continue;
        }

        // A global property will have only a single area
        if (isGlobalProp(cfg.prop)) {
            numAreas = 1;
        }

        for (int i = 0; i < numAreas; i++) {
            int32_t curArea;

            if (isGlobalProp(cfg.prop)) {
                curArea = 0;
            } else {
                curArea = cfg.areaConfigs[i].areaId;
            }

            // Create a separate instance for each individual zone
            VehiclePropValue prop = {
                .prop = cfg.prop,
                .areaId = curArea,
            };

            if (it.initialAreaValues.size() > 0) {
                auto valueForAreaIt = it.initialAreaValues.find(curArea);
                if (valueForAreaIt != it.initialAreaValues.end()) {
                    prop.value = valueForAreaIt->second;
                } else {
                    ALOGW("%s failed to get default value for prop 0x%x area 0x%x", __func__, cfg.prop, curArea);
                }
            } else {
                prop.value = it.initialValue;
            }
            mPropStore->writeValue(prop, shouldUpdateStatus);
        }
    }
    initObd2LiveFrame(*mPropStore->getConfigOrDie(OBD2_LIVE_FRAME));
    initObd2FreezeFrame(*mPropStore->getConfigOrDie(OBD2_FREEZE_FRAME));
}

void EmulatedVehicleHal::initObd2LiveFrame(const VehiclePropConfig& propConfig) {
    static constexpr bool shouldUpdateStatus = true;

    auto liveObd2Frame = createVehiclePropValue(VehiclePropertyType::MIXED, 0);
    auto sensorStore = fillDefaultObd2Frame(static_cast<size_t>(propConfig.configArray[0]), static_cast<size_t>(propConfig.configArray[1]));
    sensorStore->fillPropValue("", liveObd2Frame.get());
    liveObd2Frame->prop = OBD2_LIVE_FRAME;

    mPropStore->writeValue(*liveObd2Frame, shouldUpdateStatus);
}

void EmulatedVehicleHal::initObd2FreezeFrame(const VehiclePropConfig& propConfig) {
    static constexpr bool shouldUpdateStatus = true;

    auto sensorStore = fillDefaultObd2Frame(static_cast<size_t>(propConfig.configArray[0]),static_cast<size_t>(propConfig.configArray[1]));

    static std::vector<std::string> sampleDtcs = {"P0070",
                                                  "P0102"
                                                  "P0123"};
    for (auto&& dtc : sampleDtcs) {
        auto freezeFrame = createVehiclePropValue(VehiclePropertyType::MIXED, 0);
        sensorStore->fillPropValue(dtc, freezeFrame.get());
        freezeFrame->prop = OBD2_FREEZE_FRAME;

        mPropStore->writeValue(*freezeFrame, shouldUpdateStatus);
    }
}


std::vector<VehiclePropConfig> EmulatedVehicleHal::listProperties()  {
    return mPropStore->getAllConfigs();
}
