
//  @   frameworks/av/services/audiopolicy/engineconfigurable/src/Engine.cpp
// 构建 ParameterManagerWrapper
// 加载并解析配置文件
Engine::Engine() : mPolicyParameterMgr(new ParameterManagerWrapper())
{
    status_t loadResult = loadAudioPolicyEngineConfig();
    if (loadResult < 0) {
        ALOGE("Policy Engine configuration is invalid.");
    }
}

status_t Engine::loadAudioPolicyEngineConfig()
{
	//这里请转 EngineBase 中查看分析
    auto result = EngineBase::loadAudioPolicyEngineConfig();

    // Custom XML Parsing
    auto loadCriteria= [this](const auto& configCriteria, const auto& configCriterionTypes) {
        //遍历所有的Criteria，得到其criterionType，
        //然后通过criterionType在CriterionTypes集合中查找匹配对应上的criterionType
        for (auto& criterion : configCriteria) {
            engineConfig::CriterionType criterionType;
            //
            for (auto &configCriterionType : configCriterionTypes) {
                if (configCriterionType.name == criterion.typeName) {
                    criterionType = configCriterionType;
                    break;
                }
            }
            ALOG_ASSERT(not criterionType.name.empty(), "Invalid criterion type for %s",
                        criterion.name.c_str());
            //
            mPolicyParameterMgr->addCriterion(criterion.name, criterionType.isInclusive,
                                              criterionType.valuePairs,
                                              criterion.defaultLiteralValue);
        }
    };

    loadCriteria(result.parsedConfig->criteria, result.parsedConfig->criterionTypes);
    return result.nbSkippedElement == 0? NO_ERROR : BAD_VALUE;
}



status_t Engine::initCheck()
{
    std::string error;
    // 
    if (mPolicyParameterMgr == nullptr 
    	|| mPolicyParameterMgr->start(error) != NO_ERROR) {
        ALOGE("%s: could not start Policy PFW: %s", __FUNCTION__, error.c_str());
        return NO_INIT;
    }
    return EngineBase::initCheck();
}



//////////////////////////////////////////////////////////////////////
status_t Engine::setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config)
{
    status_t status = mPolicyParameterMgr->setForceUse(usage, config);
    if (status != NO_ERROR) {
        return status;
    }
    return EngineBase::setForceUse(usage, config);
}

audio_policy_forced_cfg_t Engine::getForceUse(audio_policy_force_use_t usage) const
{
    return mPolicyParameterMgr->getForceUse(usage);
}

///////////////////////////////////////////////////////////////////////


DeviceVector Engine::getOutputDevicesForAttributes(const audio_attributes_t &attributes,
                                                   const sp<DeviceDescriptor> &preferredDevice,
                                                   bool fromCache) const
{
    // First check for explict routing device
    if (preferredDevice != nullptr) {
        ALOGV("%s explicit Routing on device %s", __func__, preferredDevice->toString().c_str());
        return DeviceVector(preferredDevice);
    }   
    product_strategy_t strategy = getProductStrategyForAttributes(attributes);
    const DeviceVector availableOutputDevices = getApmObserver()->getAvailableOutputDevices();
    const SwAudioOutputCollection &outputs = getApmObserver()->getOutputs();
    //  
    // @TODO: what is the priority of explicit routing? Shall it be considered first as it used to
    // be by APM?
    //  
    // Honor explicit routing requests only if all active clients have a preferred route in which
    // case the last active client route is used
    sp<DeviceDescriptor> device = findPreferredDevice(outputs, strategy, availableOutputDevices);
    if (device != nullptr) {
        return DeviceVector(device);
    }   

    return fromCache? mDevicesForStrategies.at(strategy) : getDevicesForProductStrategy(strategy);
}


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

////////////////////////////////////////////////////////////////////////

status_t AudioPolicyManager::dump(int fd)
{
    String8 result;
    dump(&result);
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

void AudioPolicyManager::dump(String8 *dst) const
{
    dst->appendFormat("\nAudioPolicyManager Dump: %p\n", this);
    dst->appendFormat(" Primary Output: %d\n",
             hasPrimaryOutput() ? mPrimaryOutput->mIoHandle : AUDIO_IO_HANDLE_NONE);
    std::string stateLiteral;
    AudioModeConverter::toString(mEngine->getPhoneState(), stateLiteral);
    dst->appendFormat(" Phone state: %s\n", stateLiteral.c_str());
    const char* forceUses[AUDIO_POLICY_FORCE_USE_CNT] = {
        "communications", "media", "record", "dock", "system",
        "HDMI system audio", "encoded surround output", "vibrate ringing" };
    for (audio_policy_force_use_t i = AUDIO_POLICY_FORCE_FOR_COMMUNICATION;
         i < AUDIO_POLICY_FORCE_USE_CNT; i = (audio_policy_force_use_t)((int)i + 1)) {
        audio_policy_forced_cfg_t forceUseValue = mEngine->getForceUse(i);
        dst->appendFormat(" Force use for %s: %d", forceUses[i], forceUseValue);
        if (i == AUDIO_POLICY_FORCE_FOR_ENCODED_SURROUND &&
                forceUseValue == AUDIO_POLICY_FORCE_ENCODED_SURROUND_MANUAL) {
            dst->append(" (MANUAL: ");
            dumpManualSurroundFormats(dst);
            dst->append(")");
        }
        dst->append("\n");
    }
    dst->appendFormat(" TTS output %savailable\n", mTtsOutputAvailable ? "" : "not ");
    dst->appendFormat(" Master mono: %s\n", mMasterMono ? "on" : "off");
    dst->appendFormat(" Config source: %s\n", mConfig.getSource().c_str()); // getConfig not const
    mAvailableOutputDevices.dump(dst, String8("Available output"));
    mAvailableInputDevices.dump(dst, String8("Available input"));
    mHwModulesAll.dump(dst);
    mOutputs.dump(dst);
    mInputs.dump(dst);
    mEffects.dump(dst);
    mAudioPatches.dump(dst);
    mPolicyMixes.dump(dst);
    mAudioSources.dump(dst);

    dst->appendFormat(" AllowedCapturePolicies:\n");
    for (auto& policy : mAllowedCapturePolicies) {
        dst->appendFormat("   - uid=%d flag_mask=%#x\n", policy.first, policy.second);
    }

    dst->appendFormat("\nPolicy Engine dump:\n");
    mEngine->dump(dst);
}





////////////////////////////////////////////////////////////////////////
/**
* 由AudioPolicyManager中调用
*/
status_t Engine::setDeviceConnectionState(const sp<DeviceDescriptor> device,audio_policy_dev_state_t state)
{
    mPolicyParameterMgr->setDeviceConnectionState(device->type(), device->address().c_str(), state);
    if (audio_is_output_device(device->type())) {
        // FIXME: Use DeviceTypeSet when the interface is ready
        return mPolicyParameterMgr->setAvailableOutputDevices(deviceTypesToBitMask(getApmObserver()->getAvailableOutputDevices().types()));
    } else if (audio_is_input_device(device->type())) {
        // FIXME: Use DeviceTypeSet when the interface is ready
        return mPolicyParameterMgr->setAvailableInputDevices(deviceTypesToBitMask(getApmObserver()->getAvailableInputDevices().types()));
    }
    return EngineBase::setDeviceConnectionState(device, state);
}




