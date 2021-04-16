


void ProductStrategyMap::initialize()
{
    mDefaultStrategy = getDefault();
    ALOG_ASSERT(mDefaultStrategy != PRODUCT_STRATEGY_NONE, "No default product strategy found");
}

bool ProductStrategy::isDefault() const
{
	//frameworks/av/services/audiopolicy/engine/common/src/VolumeCurve.cpp:117:        
	// std::string attStr = attributes == defaultAttr ? "{ Any }" : android::toString(attributes);
    return std::find_if(begin(mAttributesVector), end(mAttributesVector), [](const auto &attr) {
        return attr.mAttributes == defaultAttr; }) != end(mAttributesVector);
}

product_strategy_t ProductStrategyMap::getDefault() const
{
    if (mDefaultStrategy != PRODUCT_STRATEGY_NONE) {
        return mDefaultStrategy;
    }
    for (const auto &iter : *this) {
        if (iter.second->isDefault()) {
            ALOGV("%s: using default %s", __FUNCTION__, iter.second->getName().c_str());
            return iter.second->getId();
        }
    }
    ALOGE("%s: No default product strategy defined", __FUNCTION__);
    return PRODUCT_STRATEGY_NONE;
}


ProductStrategy::ProductStrategy(const std::string &name) :
    mName(name),
    mId(static_cast<product_strategy_t>(HandleGenerator<uint32_t>::getNextHandle()))
{
	
}