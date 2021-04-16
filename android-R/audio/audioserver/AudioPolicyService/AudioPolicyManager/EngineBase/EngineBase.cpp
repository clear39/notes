
//  @   frameworks/av/services/audiopolicy/engine/common/src/EngineDefaultConfig.h
/**
 * For Internal use of respectively audio policy and audioflinger
 * For compatibility reason why apm volume config file, volume group name is the stream type.
 */
const engineConfig::ProductStrategies gOrderedSystemStrategies = {
    {"rerouting",
     {
         {"", AUDIO_STREAM_REROUTING, "AUDIO_STREAM_REROUTING",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT, 0, ""}}
         }
     },
    },
    {"patch",
     {
         {"", AUDIO_STREAM_PATCH, "AUDIO_STREAM_PATCH",
          {{AUDIO_CONTENT_TYPE_UNKNOWN, AUDIO_USAGE_UNKNOWN, AUDIO_SOURCE_DEFAULT, 0, ""}}
         }
     },
    }
};

const engineConfig::VolumeGroups gSystemVolumeGroups = {
    {"AUDIO_STREAM_REROUTING", 0, 1,
     {
         {"DEVICE_CATEGORY_SPEAKER", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEADSET", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EARPIECE", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EXT_MEDIA", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEARING_AID", {{0,0}, {100, 0}}},

     }
    },
    {"AUDIO_STREAM_PATCH", 0, 1,
     {
         {"DEVICE_CATEGORY_SPEAKER", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEADSET", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EARPIECE", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_EXT_MEDIA", {{0,0}, {100, 0}}},
         {"DEVICE_CATEGORY_HEARING_AID", {{0,0}, {100, 0}}},

     }
    }
};


//  @   frameworks\av\services\audiopolicy\engine\common\src\EngineBase.cpp
engineConfig::ParsingResult EngineBase::loadAudioPolicyEngineConfig()
{
    auto loadVolumeConfig = [](auto &volumeGroups, auto &volumeConfig) {
        // Ensure name unicity to prevent duplicate
        LOG_ALWAYS_FATAL_IF(std::any_of(std::begin(volumeGroups), std::end(volumeGroups),
                                     [&volumeConfig](const auto &volumeGroup) {
                return volumeConfig.name == volumeGroup.second->getName(); }),
                            "group name %s defined twice, review the configuration",
                            volumeConfig.name.c_str());

        sp<VolumeGroup> volumeGroup = new VolumeGroup(volumeConfig.name, volumeConfig.indexMin,
                                                      volumeConfig.indexMax);
        volumeGroups[volumeGroup->getId()] = volumeGroup;

        for (auto &configCurve : volumeConfig.volumeCurves) {
            device_category deviceCat = DEVICE_CATEGORY_SPEAKER;
            if (!DeviceCategoryConverter::fromString(configCurve.deviceCategory, deviceCat)) {
                ALOGE("%s: Invalid %s", __FUNCTION__, configCurve.deviceCategory.c_str());
                continue;
            }
            sp<VolumeCurve> curve = new VolumeCurve(deviceCat);
            for (auto &point : configCurve.curvePoints) {
                curve->add({point.index, point.attenuationInMb});
            }
            volumeGroup->add(curve);
        }
        return volumeGroup;
    };

    // group 对应 VolumeGroup
    // volumeGroup 对应 
    // strategy 为重构变量
    auto addSupportedAttributesToGroup = [](auto &group, auto &volumeGroup, auto &strategy) {
        for (const auto &attr : group.attributesVect) {
            strategy->addAttributes({group.stream, volumeGroup->getId(), attr});
            volumeGroup->addSupportedAttributes(attr);
        }
    };
    auto checkStreamForGroups = [](auto streamType, const auto &volumeGroups) {
        const auto &iter = std::find_if(std::begin(volumeGroups), std::end(volumeGroups),
                                     [&streamType](const auto &volumeGroup) {
            const auto& streams = volumeGroup.second->getStreamTypes();
            return std::find(std::begin(streams), std::end(streams), streamType) !=
                    std::end(streams);
        });
        return iter != end(volumeGroups);
    };

    //文件解析并加载到 Config
    auto result = engineConfig::parse();

    //result.parsedConfig 为 Config 类型
    if (result.parsedConfig == nullptr) {
        ALOGW("%s: No configuration found, using default matching phone experience.", __FUNCTION__);
        engineConfig::Config config = gDefaultEngineConfig;
        android::status_t ret = engineConfig::parseLegacyVolumes(config.volumeGroups);
        result = {std::make_unique<engineConfig::Config>(config),
                  static_cast<size_t>(ret == NO_ERROR ? 0 : 1)};
    } else {
        // Append for internal use only volume groups (e.g. rerouting/patch)
        //执行这里，定义在frameworks/av/services/audiopolicy/engine/common/src/EngineDefaultConfig.h
        //添加 AUDIO_STREAM_PATCH 和 AUDIO_STREAM_REROUTING
        result.parsedConfig->volumeGroups.insert(
                    std::end(result.parsedConfig->volumeGroups),
                    std::begin(gSystemVolumeGroups), std::end(gSystemVolumeGroups));
    }
    // Append for internal use only strategies (e.g. rerouting/patch)
    result.parsedConfig->productStrategies.insert(
                std::end(result.parsedConfig->productStrategies),
                std::begin(gOrderedSystemStrategies), std::end(gOrderedSystemStrategies));


    ALOGE_IF(result.nbSkippedElement != 0, "skipped %zu elements", result.nbSkippedElement);
    
    // result.parsedConfig->volumeGroups 转存储到 mVolumeGroups
    //注意这里的俩个定义为engineConfig名字空间中的，和下面的mVolumeGroups不要混在一起
    engineConfig::VolumeGroup defaultVolumeConfig;
    engineConfig::VolumeGroup defaultSystemVolumeConfig;
    for (auto &volumeConfig : result.parsedConfig->volumeGroups) {
        // save default volume config for streams not defined in configuration
        if (volumeConfig.name.compare("AUDIO_STREAM_MUSIC") == 0) {
            defaultVolumeConfig = volumeConfig;
        }
        if (volumeConfig.name.compare("AUDIO_STREAM_PATCH") == 0) {
            defaultSystemVolumeConfig = volumeConfig;
        }
        /**
         * VolumeGroupMap mVolumeGroups;
         * 其中 volumeConfig 为 engineConfig 中的定义 VolumeGroup
         * 将 volumeConfig 转存到 mVolumeGroups 中
        */
        loadVolumeConfig(mVolumeGroups, volumeConfig);
    }

    //
    for (auto& strategyConfig : result.parsedConfig->productStrategies) {
        sp<ProductStrategy> strategy = new ProductStrategy(strategyConfig.name);
        /**
         * 遍历AttributesGroup标签成员
        */
        for (const auto &group : strategyConfig.attributesGroups) {

            const auto &iter = std::find_if(begin(mVolumeGroups), end(mVolumeGroups),
                                         [&group](const auto &volumeGroup) {
                    //
                    return group.volumeGroup == volumeGroup.second->getName(); });

            sp<VolumeGroup> volumeGroup = nullptr;
            // If no volume group provided for this strategy, creates a new one using
            // Music Volume Group configuration (considered as the default)
            if (iter == end(mVolumeGroups)) {
                engineConfig::VolumeGroup volumeConfig;
                if (group.stream >= AUDIO_STREAM_PUBLIC_CNT) {
                    volumeConfig = defaultSystemVolumeConfig;
                } else {
                    volumeConfig = defaultVolumeConfig;
                }
                ALOGW("%s: No configuration of %s found, using default volume configuration"
                        , __FUNCTION__, group.volumeGroup.c_str());
                volumeConfig.name = group.volumeGroup;
                /**

                */
                volumeGroup = loadVolumeConfig(mVolumeGroups, volumeConfig);
            } else {
                volumeGroup = iter->second;
            }
            if (group.stream != AUDIO_STREAM_DEFAULT) {
                // A legacy stream can be assigned once to a volume group
                LOG_ALWAYS_FATAL_IF(checkStreamForGroups(group.stream, mVolumeGroups),
                                    "stream %s already assigned to a volume group, "
                                    "review the configuration", toString(group.stream).c_str());
                //
                volumeGroup->addSupportedStream(group.stream);
            }
            /**
             * 
            */
            addSupportedAttributesToGroup(group, volumeGroup, strategy);
        }
        product_strategy_t strategyId = strategy->getId();
        mProductStrategies[strategyId] = strategy;
    }
    // ProductStrategyMap mProductStrategies;
    // 初始化默认的 mDefaultStrategy 成员
    mProductStrategies.initialize();
    return result;
}

















/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
status_t EngineBase::setPreferredDeviceForStrategy(product_strategy_t strategy,
            const AudioDeviceTypeAddr &device)
{
    // verify strategy exists
    if (mProductStrategies.find(strategy) == mProductStrategies.end()) {
        ALOGE("%s invalid strategy %u", __func__, strategy);
        return BAD_VALUE;
    }

    mProductStrategyPreferredDevices[strategy] = device;
    return NO_ERROR;
}


status_t EngineBase::removePreferredDeviceForStrategy(product_strategy_t strategy)
{
    // verify strategy exists
    if (mProductStrategies.find(strategy) == mProductStrategies.end()) {
        ALOGE("%s invalid strategy %u", __func__, strategy);
        return BAD_VALUE;
    }

    if (mProductStrategyPreferredDevices.erase(strategy) == 0) {
        // no preferred device was set
        return NAME_NOT_FOUND;
    }
    return NO_ERROR;
}


status_t EngineBase::getPreferredDeviceForStrategy(product_strategy_t strategy,
            AudioDeviceTypeAddr &device) const
{
    // verify strategy exists
    if (mProductStrategies.find(strategy) == mProductStrategies.end()) {
        ALOGE("%s unknown strategy %u", __func__, strategy);
        return BAD_VALUE;
    }
    // preferred device for this strategy?
    auto devIt = mProductStrategyPreferredDevices.find(strategy);
    if (devIt == mProductStrategyPreferredDevices.end()) {
        ALOGV("%s no preferred device for strategy %u", __func__, strategy);
        return NAME_NOT_FOUND;
    }

    device = devIt->second;
    return NO_ERROR;
}
