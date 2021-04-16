
/*
ParameterManagerWrapper作用为：

*/

//  @   frameworks\av\services\audiopolicy\engineconfigurable\wrapper\ParameterManagerWrapper.cpp
ParameterManagerWrapper::ParameterManagerWrapper(bool enableSchemaVerification = false,
                                                 const std::string &schemaUri = {})
    : mPfwConnectorLogger(new ParameterMgrPlatformConnectorLogger)
{
    // Connector
    // const char *const ParameterManagerWrapper::mPolicyPfwVendorConfFileName =
    // "/vendor/etc/parameter-framework/ParameterFrameworkConfigurationPolicy.xml";
    if (access(mPolicyPfwVendorConfFileName, R_OK) == 0) {
        //执行这里，
        // 第1，构建CParameterMgrPlatformConnector
        mPfwConnector = new CParameterMgrPlatformConnector(mPolicyPfwVendorConfFileName);
    } else {
        mPfwConnector = new CParameterMgrPlatformConnector(mPolicyPfwDefaultConfFileName);
    }

    // Logger
    mPfwConnector->setLogger(mPfwConnectorLogger);

    // Schema validation
    std::string error;
    // 第2、setValidateSchemasOnStart调用 enableSchemaVerification为false
    bool ret = mPfwConnector->setValidateSchemasOnStart(enableSchemaVerification, error);
    ALOGE_IF(!ret, "Failed to activate schema validation: %s", error.c_str());


    // 由于 enableSchemaVerification 使用默认参数为false，所以setSchemaUri不调用
    if (enableSchemaVerification && ret && !schemaUri.empty()) {
        ALOGE("Schema verification activated with schema URI: %s", schemaUri.c_str());
        mPfwConnector->setSchemaUri(schemaUri); //这里不执行
    }
}

// Criterion (评判或作决定的)标准，准则，原则
// using ValuePair = std::pair<uint32_t, std::string>;
// using ValuePairs = std::vector<ValuePair>;
// ParameterManagerWrapper::addCriterion 在 Engine::loadAudioPolicyEngineConfig() 中调用
status_t ParameterManagerWrapper::addCriterion(const std::string &name, bool isInclusive,
                                               ValuePairs pairs, const std::string &defaultValue)
{
    ALOG_ASSERT(not isStarted(), "Cannot add a criterion if PFW is already started");
    //这里返回的criterionType 为ISelectionCriterionTypeInterface类型
    auto criterionType = mPfwConnector->createSelectionCriterionType(isInclusive);

    for (auto pair : pairs) {
        std::string error;
        ALOGV("%s: Adding pair %d,%s for criterionType %s", __FUNCTION__, pair.first,
              pair.second.c_str(), name.c_str());
        criterionType->addValuePair(pair.first, pair.second, error);
    }
    ALOG_ASSERT(mPolicyCriteria.find(name) == mPolicyCriteria.end(),
                "%s: Criterion %s already added", __FUNCTION__, name.c_str());

    // 这里返回的criterion为ISelectionCriterionInterface 类型
    auto criterion = mPfwConnector->createSelectionCriterion(name, criterionType);
    mPolicyCriteria[name] = criterion;

    if (not defaultValue.empty()) {
        int numericalValue = 0;
        if (not criterionType->getNumericalValue(defaultValue.c_str(), numericalValue)) {
            ALOGE("%s; trying to apply invalid default literal value (%s)", __FUNCTION__,
                  defaultValue.c_str());
        }
        criterion->setCriterionState(numericalValue);
    }
    return NO_ERROR;
}


// 在 status_t Engine::initCheck() 中调用
status_t ParameterManagerWrapper::start(std::string &error)
{
    ALOGD("%s: in", __FUNCTION__);
    /// Start PFW
    if (!mPfwConnector->start(error)) {
        ALOGE("%s: Policy PFW start error: %s", __FUNCTION__, error.c_str());
        return NO_INIT;
    }
    ALOGD("%s: Policy PFW successfully started!", __FUNCTION__);
    return NO_ERROR;
}

status_t ParameterManagerWrapper::setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config)
{
    // @todo: return an error on a unsupported value
    if (usage > AUDIO_POLICY_FORCE_USE_CNT) {
        return BAD_VALUE;
    }

    /**
    * mPolicyCriteria 在 ParameterManagerWrapper::addCriterion 中初始化
    * getElement<ISelectionCriterionInterface> 的作用是：
    * 获取gForceUseCriterionTag[usage]中的字符串作为key，到mPolicyCriteria(map)中查找对应的value
    */
    ISelectionCriterionInterface *criterion =
            getElement<ISelectionCriterionInterface>(gForceUseCriterionTag[usage], mPolicyCriteria);
    if (criterion == NULL) {
        ALOGE("%s: no criterion found for %s", __FUNCTION__, gForceUseCriterionTag[usage]);
        return BAD_VALUE;
    }
    if (!isValueValidForCriterion(criterion, static_cast<int>(config))) {
        return BAD_VALUE;
    }
    criterion->setCriterionState((int)config);
    applyPlatformConfiguration();
    return NO_ERROR;
}

audio_policy_forced_cfg_t ParameterManagerWrapper::getForceUse(audio_policy_force_use_t usage) const
{
    // @todo: return an error on a unsupported value
    if (usage > AUDIO_POLICY_FORCE_USE_CNT) {
        return AUDIO_POLICY_FORCE_NONE;
    }
    /**
    * mPolicyCriteria 在 ParameterManagerWrapper::addCriterion 中初始化
    * getElement<ISelectionCriterionInterface> 的作用是：
    * 获取gForceUseCriterionTag[usage]中的字符串作为key，到mPolicyCriteria(map)中查找对应的value
    */
    const ISelectionCriterionInterface *criterion =
            getElement<ISelectionCriterionInterface>(gForceUseCriterionTag[usage], mPolicyCriteria);
    if (criterion == NULL) {
        ALOGE("%s: no criterion found for %s", __FUNCTION__, gForceUseCriterionTag[usage]);
        return AUDIO_POLICY_FORCE_NONE;
    }
    return static_cast<audio_policy_forced_cfg_t>(criterion->getCriterionState());
}







/////////////////////////////////////////////////////////////////////////////////////////
/*
<criterion_type name="OutputDevicesAddressesType" type="inclusive">
    <values>
        <value literal="0" numerical="1"/>
    </values>
</criterion_type>
*/
status_t ParameterManagerWrapper::setDeviceConnectionState(
        audio_devices_t type, const std::string address, audio_policy_dev_state_t state)
{
    // static const char *const gOutputDeviceAddressCriterionName = "AvailableOutputDevicesAddresses";
    // static const char *const gInputDeviceAddressCriterionName = "AvailableInputDevicesAddresses";
    std::string criterionName = audio_is_output_device(type) ?
                gOutputDeviceAddressCriterionName : gInputDeviceAddressCriterionName;

    ALOGV("%s: device with address %s %s", __FUNCTION__, address.c_str(),
          state != AUDIO_POLICY_DEVICE_STATE_AVAILABLE? "disconnected" : "connected");
    
    /**
    * mPolicyCriteria 在 ParameterManagerWrapper::addCriterion 中初始化
    * getElement<ISelectionCriterionInterface> 的作用是：
    * 获取gForceUseCriterionTag[usage]中的字符串作为key，到mPolicyCriteria(map)中查找对应的value
    */
    ISelectionCriterionInterface *criterion = getElement<ISelectionCriterionInterface>(criterionName, mPolicyCriteria);

    if (criterion == NULL) {
        ALOGE("%s: no criterion found for %s", __FUNCTION__, criterionName.c_str());
        return DEAD_OBJECT;
    }

    //
    auto criterionType = criterion->getCriterionType();
    int deviceAddressId;
    //由于 address 为空，在 getNumericalValue中 deviceAddressId 被设置为 0
    if (not criterionType->getNumericalValue(address.c_str(), deviceAddressId)) {
        ALOGW("%s: unknown device address reported (%s) for criterion %s", __FUNCTION__,
              address.c_str(), criterionName.c_str());
        return BAD_TYPE;
    }
    //
    int currentValueMask = criterion->getCriterionState();
    if (state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
        currentValueMask |= deviceAddressId;
    } else {
        currentValueMask &= ~deviceAddressId;
    }
    criterion->setCriterionState(currentValueMask);
    return NO_ERROR;
}


/////////////////////////////////////////////////////////////////////////////////////////
status_t ParameterManagerWrapper::setAvailableOutputDevices(audio_devices_t outputDevices)
{
    ALOGV("%s 0x%#8x",__func__, outputDevices);
    // gOutputDeviceCriterionName = "AvailableOutputDevices" 
    ISelectionCriterionInterface *criterion = getElement<ISelectionCriterionInterface>(gOutputDeviceCriterionName, mPolicyCriteria);
    if (criterion == NULL) {
        ALOGE("%s: no criterion found for %s", __FUNCTION__, gOutputDeviceCriterionName);
        return DEAD_OBJECT;
    }
    criterion->setCriterionState(outputDevices); // Speaker|WiredHeadphone|HdmiArc
    applyPlatformConfiguration();
    return NO_ERROR;
}



void ParameterManagerWrapper::applyPlatformConfiguration()
{
    mPfwConnector->applyConfigurations();
}

