

class CBitParameterType : public CTypeElement {

};


CBitParameterType::CBitParameterType(const string &strName) : base(strName)
{
}


// <BitParameter Name="earpiece" Size="1" Pos="0"/>
// From IXmlSink
bool CBitParameterType::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Pos
    xmlElement.getAttribute("Pos", _bitPos);

    // Size
    xmlElement.getAttribute("Size", _uiBitSize);

    // Validate bit pos and size still fit into parent type
    // 这里得到的父类为CBitParameterBlockType
    const CBitParameterBlockType *pBitParameterBlockType = static_cast<const CBitParameterBlockType *>(getParent());

    // 4 * 8  = 32
    size_t uiParentBlockBitSize = pBitParameterBlockType->getSize() * 8;

    if (_bitPos + _uiBitSize > uiParentBlockBitSize) {

        // Range exceeded
        std::ostringstream strStream;

        strStream << "Pos and Size attributes inconsistent with maximum container element size ("
                  << uiParentBlockBitSize << " bits) for " + getKind();

        serializingContext.setError(strStream.str());

        return false;
    }

    // Max
    _uiMax = getMaxEncodableValue();
    if (xmlElement.getAttribute("Max", _uiMax) && (_uiMax > getMaxEncodableValue())) {

        // Max value exceeded
        std::ostringstream strStream;

        strStream << "Max attribute inconsistent with maximum encodable size ("
                  << getMaxEncodableValue() << ") for " + getKind();

        serializingContext.setError(strStream.str());

        return false;
    }

    // Base //没有其他属性了
    return base::fromXml(xmlElement, serializingContext);
}


// Max value
uint64_t CBitParameterType::getMaxEncodableValue() const
{
    return (uint64_t)-1L >> (8 * sizeof(uint64_t) - _uiBitSize);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CInstanceConfigurableElement *CTypeElement::instantiate() const
{
    CInstanceConfigurableElement *pInstanceConfigurableElement = doInstantiate();

    // Populate
    populate(pInstanceConfigurableElement);

    return pInstanceConfigurableElement;
}


CInstanceConfigurableElement *CBitParameterType::doInstantiate() const
{
    return new CBitParameter(getName(), this);
}

