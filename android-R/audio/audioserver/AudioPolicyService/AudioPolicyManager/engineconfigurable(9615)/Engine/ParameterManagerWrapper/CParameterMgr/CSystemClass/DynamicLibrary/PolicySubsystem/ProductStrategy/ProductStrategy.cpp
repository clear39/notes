class PARAMETER_EXPORT CFormattedSubsystemObject : public CSubsystemObject{};

class ProductStrategy : public CFormattedSubsystemObject {};

// <Component Name="phone" Type="ProductStrategy" Mapping="Name:STRATEGY_PHONE"/>
// mappingValue 为 STRATEGY_PHONE
// instanceConfigurableElement = CComponent
ProductStrategy::ProductStrategy(const string &mappingValue,
                   CInstanceConfigurableElement *instanceConfigurableElement,
                   const CMappingContext &context,
                   core::log::Logger& logger)
    : CFormattedSubsystemObject(instanceConfigurableElement,
                                logger,
                                mappingValue,
                                MappingKeyAmend1,
                                (MappingKeyAmendEnd - MappingKeyAmend1 + 1),
                                context)
{
    std::string name(context.getItem(MappingKeyName));

    ALOG_ASSERT(instanceConfigurableElement != nullptr, "Invalid Configurable Element");
    mPolicySubsystem = static_cast<const PolicySubsystem *>(
                instanceConfigurableElement->getBelongingSubsystem());
    ALOG_ASSERT(mPolicySubsystem != nullptr, "Invalid Policy Subsystem");

    mPolicyPluginInterface = mPolicySubsystem->getPolicyPluginInterface();
    ALOG_ASSERT(mPolicyPluginInterface != nullptr, "Invalid Policy Plugin Interface");

    mId = mPolicyPluginInterface->getProductStrategyByName(name);

    ALOG_ASSERT(mId != PRODUCT_STRATEGY_INVALID, "Product Strategy %s not found", name.c_str());

    ALOGE("Product Strategy %s added", name.c_str());
}


CSubsystemObject::CSubsystemObject(CInstanceConfigurableElement *pInstanceConfigurableElement,
                                   core::log::Logger &logger)
    : _logger(logger), _pInstanceConfigurableElement(pInstanceConfigurableElement),
      _dataSize(pInstanceConfigurableElement->getFootPrint())
{
    // Syncer
    //这里很重要
    _pInstanceConfigurableElement->setSyncer(this);
}



bool ProductStrategy::sendToHW(string & /*error*/)
{
    Device deviceParams;
    blackboardRead(&deviceParams, sizeof(deviceParams));

    mPolicyPluginInterface->setDeviceTypesForProductStrategy(mId, deviceParams.applicableDevice);
    mPolicyPluginInterface->setDeviceAddressForProductStrategy(mId, deviceParams.deviceAddress);
    return true;
}

