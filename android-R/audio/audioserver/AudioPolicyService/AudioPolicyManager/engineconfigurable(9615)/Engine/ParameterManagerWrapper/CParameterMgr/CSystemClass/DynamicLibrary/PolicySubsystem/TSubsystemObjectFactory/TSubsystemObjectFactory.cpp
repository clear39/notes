
class PARAMETER_EXPORT CSubsystemObjectCreator {

};

CSubsystemObjectCreator::CSubsystemObjectCreator(const string &strMappingKey,
                                                 uint32_t uiAncestorIdMask,
                                                 size_t maxConfigurableElementSize)
    : _strMappingKey(strMappingKey), _uiAncestorIdMask(uiAncestorIdMask),
      _maxConfigurableElementSize(maxConfigurableElementSize)
{
}

// Accessors
// mStreamComponentName = "Stream";
// mInputSourceComponentName = "InputSource";
//mProductStrategyComponentName = "ProductStrategy";
const string &CSubsystemObjectCreator::getMappingKey() const
{
    return _strMappingKey;
}

size_t CSubsystemObjectCreator::getMaxConfigurableElementSize() const
{
    return _maxConfigurableElementSize;
}


template <class SubsystemObjectType>
class TSubsystemObjectFactory : public CSubsystemObjectCreator
{
public:
    TSubsystemObjectFactory(const std::string &strMappingKey, uint32_t uiAncestorIdMask,
                            size_t maxConfigurableElementSize = std::numeric_limits<size_t>::max())
        : CSubsystemObjectCreator(strMappingKey, uiAncestorIdMask, maxConfigurableElementSize)
    {
    }

    // Object creation
    virtual CSubsystemObject *objectCreate(
        const std::string &strMappingValue,
        CInstanceConfigurableElement *pInstanceConfigurableElement, const CMappingContext &context,
        core::log::Logger &logger) const
    {	
    	//Stream
    	//InputSource
    	//ProductStrategy
        return new SubsystemObjectType(strMappingValue, pInstanceConfigurableElement, context,
                                       logger);
    }
};
