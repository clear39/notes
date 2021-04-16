

class CAreaConfiguration {
	  // Configurable element settings
    CParameterBlackboard _blackboard;

    const CSyncerSet *_pSyncerSet;
};


CAreaConfiguration::CAreaConfiguration(const CConfigurableElement *pConfigurableElement,
                                       const CSyncerSet *pSyncerSet)
    : _pConfigurableElement(pConfigurableElement), _pSyncerSet(pSyncerSet)
{
    // Size blackboard
    _blackboard.setSize(_pConfigurableElement->getFootPrint());
}


// XML configuration settings parsing
bool CAreaConfiguration::serializeXmlSettings(
    CXmlElement &xmlConfigurableElementSettingsElementContent,
    CConfigurationAccessContext &configurationAccessContext)
{
    // Assign blackboard to configuration context
    configurationAccessContext.setParameterBlackboard(&_blackboard);

    // Assign base offset to configuration context
    configurationAccessContext.setBaseOffset(_pConfigurableElement->getOffset());

    // Parse configuration settings (element contents)
    if (_pConfigurableElement->serializeXmlSettings(xmlConfigurableElementSettingsElementContent, configurationAccessContext)) {

        if (!configurationAccessContext.serializeOut()) {

            // Serialized-in areas are valid
            _bValid = true;
        }
        return true;
    }
    return false;
}




// Apply data to current
bool CAreaConfiguration::restore(CParameterBlackboard *pMainBlackboard, bool bSync /*=false*/, core::Results *errors) const
{
    assert(_bValid);

    copyTo(pMainBlackboard, _pConfigurableElement->getOffset());

    // Synchronize if required
    // 由于bSync为false, 一直返回true
    return !bSync || _pSyncerSet->sync(*pMainBlackboard, false, errors);
}

// Blackboard copies
void CAreaConfiguration::copyTo(CParameterBlackboard *pToBlackboard, size_t offset) const
{
	//将_blackboard(本类中私有变量)赋值给全局 pToBlackboard
    pToBlackboard->restoreFrom(&_blackboard, offset);
}