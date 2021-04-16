
class PolicySubsystem : public CSubsystem {

}

// @ frameworks/av/services/audiopolicy/engineconfigurable/parameter-framework/plugin/PolicySubsystem.cpp
// 在 CXmlFileIncluderElement::fromXml 中构建 并且添加 CSystemClass中
// name 为 Name="policy"
PolicySubsystem::PolicySubsystem(const std::string &name, core::log::Logger &logger)
    : CSubsystem(name, logger),
      mPluginInterface(NULL)
{
    // Try to connect a Plugin Interface from Audio Policy Engine
    EngineInstance *engineInstance = EngineInstance::getInstance();

    ALOG_ASSERT(engineInstance != NULL, "NULL Plugin Interface");

    // Retrieve the Route Interface
    mPluginInterface = engineInstance->queryInterface<android::AudioPolicyPluginInterface>();
    ALOG_ASSERT(mPluginInterface != NULL, "NULL Plugin Interface");

    // addContextMappingKey 是在父类 CSubsystem 中实现
    // Provide mapping keys to the core, necessary when parsing the XML Structure files.
    addContextMappingKey(mKeyName);  // mKeyName = "Name";
    addContextMappingKey(mKeyCategory); // mKeyCategory = "Category";
    addContextMappingKey(mKeyIdentifier);  // mKeyIdentifier = "Identifier";
    addContextMappingKey(mKeyAmend1); // mKeyAmend1 = "Amend1";
    addContextMappingKey(mKeyAmend2); // mKeyAmend2 = "Amend2";
    addContextMappingKey(mKeyAmend3); // mKeyAmend3 = "Amend3";

    // addSubsystemObjectFactory 是在父类 CSubsystem 中实现
    // Provide creators to upper layer
    // mStreamComponentName = "Stream";
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<Stream>(
            mStreamComponentName,
            (1 << MappingKeyName))      // MappingKeyName = 0
        );
    // mInputSourceComponentName = "InputSource";
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<InputSource>(
            mInputSourceComponentName,
            (1 << MappingKeyName)) // MappingKeyName = 0
        );

    //mProductStrategyComponentName = "ProductStrategy";
    addSubsystemObjectFactory(
        new TSubsystemObjectFactory<ProductStrategy>(
            mProductStrategyComponentName, (1 << MappingKeyName)) // MappingKeyName = 0
        );
}


//主要解析 /vendor/etc/parameter-framework/Structure/Policy/PolicySubsystem.xml 
/*
CXmlFileIncluderElement::fromXml
CXmlMemoryDocSink::doProcess
CConfigurableElement::fromXml // PolicySubsystem 和 CSubsystem都没有实现 fromXml 该方法
*/
bool CConfigurableElement::fromXml(const CXmlElement &xmlElement,
                                   CXmlSerializingContext &serializingContext)
{
    auto &context = static_cast<CXmlParameterSerializingContext &>(serializingContext);
    auto &accessContext = context.getAccessContext();

    // false
    if (accessContext.serializeSettings()) {
        // As serialization and deserialisation are handled through the *same* function
        // the (de)serialize object can not be const in `serializeXmlSettings` signature.
        // As a result a const_cast is unavoidable :(.
        // Fixme: split serializeXmlSettings in two functions (in and out) to avoid the `const_cast`
        return serializeXmlSettings(const_cast<CXmlElement &>(xmlElement),
                                    static_cast<CConfigurationAccessContext &>(accessContext));
    }

    // 由于之类 CSubsystem::structureFromXml 实现 
    return structureFromXml(xmlElement, serializingContext);
}



////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

const CElement *CElement::findDescendant(CPathNavigator &pathNavigator) const
{
    // product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
    string *pStrChildName = pathNavigator.next(); // pStrChildName = "product_strategies"

    if (!pStrChildName) {

        return this;
    }
    /*
    *有2个成员为 CComponent 
    */
    //有三个孩子成员
    /*
    <Component Name="streams" Type="Streams"/>  （CComponentInstance）
    <Component Name="input_sources" Type="InputSources"/> （CComponentInstance）
    <Component Name="product_strategies" Type="ProductStrategies"/> （CComponentInstance）
    */
    const CElement *pChild = findChild(*pStrChildName);

    if (!pChild) {

        return nullptr;
    }

    // 
    return pChild->findDescendant(pathNavigator);
}


