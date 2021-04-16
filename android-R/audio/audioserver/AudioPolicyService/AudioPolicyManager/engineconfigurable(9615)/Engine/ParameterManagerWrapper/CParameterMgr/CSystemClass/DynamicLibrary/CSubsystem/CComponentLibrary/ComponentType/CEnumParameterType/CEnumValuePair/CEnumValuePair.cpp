
class CEnumValuePair : public CElement {

}
// <ValuePair Literal="voice_call" Numerical="0"/>
// From IXmlSink
bool CEnumValuePair::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Literal
    std::string name;
    xmlElement.getAttribute("Literal", name);
    setName(name);

    // Numerical
    xmlElement.getAttribute("Numerical", _iNumerical);

    // Base
    return base::fromXml(xmlElement, serializingContext);
}
