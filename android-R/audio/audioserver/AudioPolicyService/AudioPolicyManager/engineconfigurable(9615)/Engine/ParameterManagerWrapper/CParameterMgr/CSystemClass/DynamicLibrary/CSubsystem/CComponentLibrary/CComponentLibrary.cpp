

class CComponentLibrary : public CElement {

}

/**
// /vendor/etc/parameter-framework/Structure/Policy/PolicySubsystem.xml 
<ComponentLibrary>
    <!--#################### GLOBAL COMPONENTS BEGIN ####################-->
    <!-- Common Types defintion -->
    <xi:include href="PolicySubsystem-CommonTypes.xml"/>
    <xi:include href="ProductStrategies.xml"/>

    <ComponentType Name="Streams" Description="associated to audio_stream_type_t definition">
        <Component Name="voice_call" Type="Stream" Mapping="Name:AUDIO_STREAM_VOICE_CALL"/>
    </ComponentType>
</ComponentLibrary>
*/

/*
PolicySubsystem-CommonTypes.xml

<ComponentType Name="InputDevicesMask">
    <BitParameterBlock Name="mask" Size="32">
        <BitParameter Name="communication" Size="1" Pos="0"/>
   </BitParameterBlock>
</ComponentType>              

<ComponentType Name="VolumeProfileType">
    <!--EnumParameter对应 CEnumParameterType-->
    <EnumParameter Name="volume_profile" Size="32">
        <!--ValuePair 对应 CEnumValuePair -->
        <ValuePair Literal="voice_call" Numerical="0"/>
    </EnumParameter>
</ComponentType>

<ComponentTypeSet>
    <ComponentType Name="ProductStrategy" Mapping="ProductStrategy">
        <Component Name="selected_output_devices" Type="OutputDevicesMask"/>
        <StringParameter Name="device_address" MaxLength="256" Description="if any, device address associated"/>
    </ComponentType>
</ComponentTypeSet>
*/

/*
//ProductStrategies.xml

<ComponentTypeSet xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
        xsi:noNamespaceSchemaLocation="Schemas/ComponentTypeSet.xsd">
    <!--ComponentType 对应 CComponentType-->
    <ComponentType Name="ProductStrategies" Description="">
        <Component Name="phone" Type="ProductStrategy" Mapping="Name:STRATEGY_PHONE"/>
    </ComponentType>
</ComponentTypeSet>
*/

//这里最子成员为 ComponentType 
bool CComponentLibrary::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    CXmlElement childElement;

    CXmlElement::CChildIterator it(xmlElement);

    // XML populate all component libraries
    while (it.next(childElement)) {

        // Filter component library/type set elements
        if (childElement.getType() == "ComponentLibrary" ||
            childElement.getType() == "ComponentTypeSet") {
            // 对于 PolicySubsystem-CommonTypes.xml 和 ProductStrategies.xml 文件中的内容，再次递归调用
            if (!fromXml(childElement, serializingContext)) {
                return false;
            }
        } else {
            // Regular child creation and populating
            // 这里解析 ComponentType 标签
            // pParameterCreationLibrary->addElementBuilder("ComponentType", new TNamedElementBuilderTemplate<CComponentType>());
            // xmlElement.getNameAttribute() 得到 ComponentType 标签的属性Name值
            // 总共有11个 ComponentType
             // pChild = new CComponentType(xmlElement.getNameAttribute());  
            CElement *pChild = createChild(childElement, serializingContext);

            // CComponentType::fromXml
            if (!pChild || !pChild->fromXml(childElement, serializingContext)) 
                return false;
            }
        }
    }

    return true;
}

CElement *CElement::createChild(const CXmlElement &childElement, CXmlSerializingContext &serializingContext)
{
    // Context
    CXmlElementSerializingContext &elementSerializingContext = static_cast<CXmlElementSerializingContext &>(serializingContext);

    // Child needs creation
    // 这里elementSerializingContext.getElementLibrary()还是返回 CParameterMgr::_pElementLibrarySet中的第二个成员
    // 
    CElement *pChild = elementSerializingContext.getElementLibrary()->createElement(childElement);

    if (!pChild) { 
        elementSerializingContext.setError("Unable to create XML element " + childElement.getPath());
        return nullptr;
    }
    // Store created child!
    addChild(pChild);

    return pChild;
}