
class VehiclePropertyStore {
    using TokenFunction = std::function<int64_t(const VehiclePropValue& value)>;

    struct RecordConfig {
        VehiclePropConfig propConfig;
        TokenFunction tokenFunction;
    };

    struct RecordId {
        int32_t prop;
        int32_t area;
        int64_t token;

        bool operator==(const RecordId& other) const;
        bool operator<(const RecordId& other) const;
    };

    std::unordered_map<int32_t /* VehicleProperty */, RecordConfig> mConfigs;

    using PropertyMap = std::map<RecordId, VehiclePropValue>;
    PropertyMap mPropertyValues;  // Sorted map of RecordId : VehiclePropValue.
}




//  hardware/interfaces/automotive/vehicle/2.0/default/common/src/VehiclePropertyStore.cpp
//  tokenFunc 默认参数为null
void VehiclePropertyStore::registerProperty(const VehiclePropConfig& config,VehiclePropertyStore::TokenFunction tokenFunc) {
    MuxGuard g(mLock);
    //  std::unordered_map<int32_t /* VehicleProperty */, RecordConfig> mConfigs;
    mConfigs.insert({ config.prop, RecordConfig { config, tokenFunc } });
}


bool VehiclePropertyStore::writeValue(const VehiclePropValue& propValue,bool updateStatus) {
    MuxGuard g(mLock);
    if (!mConfigs.count(propValue.prop)) return false;

    RecordId recId = getRecordIdLocked(propValue);
    VehiclePropValue* valueToUpdate = const_cast<VehiclePropValue*>(getValueOrNullLocked(recId));
    if (valueToUpdate == nullptr) {
        mPropertyValues.insert({ recId, propValue });
    } else {
        valueToUpdate->timestamp = propValue.timestamp;
        valueToUpdate->value = propValue.value;
        if (updateStatus) {
            valueToUpdate->status = propValue.status;
        }
    }
    return true;
}

VehiclePropertyStore::RecordId VehiclePropertyStore::getRecordIdLocked(const VehiclePropValue& valuePrototype) const {
    RecordId recId = {
        .prop = valuePrototype.prop,
        .area = isGlobalProp(valuePrototype.prop) ? 0 : valuePrototype.areaId,
        .token = 0
    };

    auto it = mConfigs.find(recId.prop);
    if (it == mConfigs.end()) return {};

    if (it->second.tokenFunction != nullptr) {
        recId.token = it->second.tokenFunction(valuePrototype);
    }
    return recId;
}


const VehiclePropValue* VehiclePropertyStore::getValueOrNullLocked(const VehiclePropertyStore::RecordId& recId) const  {
    auto it = mPropertyValues.find(recId);
    return it == mPropertyValues.end() ? nullptr : &it->second;
}


std::vector<VehiclePropConfig> VehiclePropertyStore::getAllConfigs() const {
    MuxGuard g(mLock);
    std::vector<VehiclePropConfig> configs;
    configs.reserve(mConfigs.size());
    for (auto&& recordConfigIt: mConfigs) {
        configs.push_back(recordConfigIt.second.propConfig);
    }
    return configs;
}