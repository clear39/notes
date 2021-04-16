

class CStringParameterType : public CTypeElement {

}

CStringParameterType::CStringParameterType(const string &strName) : base(strName)
{
}

// 在 PolicySubsystem-CommonTypes.xml 中 只有一项
/**
<ComponentType Name="ProductStrategy" Mapping="ProductStrategy">
    <Component Name="selected_output_devices" Type="OutputDevicesMask"/>
    <StringParameter Name="device_address" MaxLength="256" Description="if any, device address associated"/>
</ComponentType>
*/
// From IXmlSink
bool CStringParameterType::fromXml(const CXmlElement &xmlElement,CXmlSerializingContext &serializingContext)
{
    // MaxLength
    xmlElement.getAttribute("MaxLength", _maxLength); //256

    // Base
    return base::fromXml(xmlElement, serializingContext);
}



////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
CInstanceConfigurableElement *CTypeElement::instantiate() const
{
    CInstanceConfigurableElement *pInstanceConfigurableElement = doInstantiate();

    // Populate
    populate(pInstanceConfigurableElement);

    return pInstanceConfigurableElement;
}


CInstanceConfigurableElement *CStringParameterType::doInstantiate() const
{
    return new CStringParameter(getName(), this);
}
