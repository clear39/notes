
class CBitParameterBlockType : public CTypeElement {};

/*
 <BitParameterBlock Name="mask" Size="32">
    <BitParameter Name="earpiece" Size="1" Pos="0"/>
    <BitParameter Name="speaker" Size="1" Pos="1"/>
 </BitParameterBlock>
*/
// 解析 BitParameterBlock 标签
CBitParameterBlockType::CBitParameterBlockType(const string &strName) : base(strName)
{
}

/*
 <BitParameterBlock Name="mask" Size="32">
    <BitParameter Name="earpiece" Size="1" Pos="0"/>
    <BitParameter Name="speaker" Size="1" Pos="1"/>
 </BitParameterBlock>
*/
// From IXmlSink
bool CBitParameterBlockType::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Size
    xmlElement.getAttribute("Size", _size);
    _size /= 8;  // 4

    // Base
    // CTypeElement::fromXml
    return base::fromXml(xmlElement, serializingContext);
}


bool CTypeElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Array Length attribute
    // 在 CBitParameterBlockType 中没有该属性
    xmlElement.getAttribute("ArrayLength", _arrayLength);
    // Manage mapping attribute
    std::string rawMapping;
     // 在 CBitParameterBlockType 中没有该属性
    if (xmlElement.getAttribute("Mapping", rawMapping) && !rawMapping.empty()) {

        std::string error;
        if (!getMappingData()->init(rawMapping, error)) {

            serializingContext.setError("Invalid Mapping data from XML element '" +
                                        xmlElement.getPath() + "': " + error);
            return false;
        }
    }
    return base::fromXml(xmlElement, serializingContext);
}



// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        // CBitParameterBlockType::childrenAreDynamic() 返回 true
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {

                serializingContext.setError("Unable to handle XML element: " +
                                            childElement.getPath());

                return false;
            }

        } else {
            // Child needs creation
            // <BitParameter Name="earpiece" Size="1" Pos="0"/>
            // pParameterCreationLibrary->addElementBuilder("BitParameter", new TNamedElementBuilderTemplate<CBitParameterType>());
            // pChild = CBitParameterType
            pChild = createChild(childElement, serializingContext);

            if (!pChild) {
                return false;
            }
        }

        // Dig
        // pChild = CBitParameterType
        if (!pChild->fromXml(childElement, serializingContext)) {

            return false;
        }
    }

    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
CInstanceConfigurableElement *CTypeElement::instantiate() const
{
    CInstanceConfigurableElement *pInstanceConfigurableElement = doInstantiate();

    // Populate
    populate(pInstanceConfigurableElement);

    return pInstanceConfigurableElement;
}



void CTypeElement::populate(CElement *pElement) const
{
    // Populate children
    size_t uiChild;
    size_t uiNbChildren = getNbChildren();

    //子成员为 CBitParameterType 
    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

        const CTypeElement *pChildTypeElement =
            static_cast<const CTypeElement *>(getChild(uiChild));

        CInstanceConfigurableElement *pInstanceConfigurableChildElement =
            pChildTypeElement->instantiate();

        // Affiliate
        pElement->addChild(pInstanceConfigurableChildElement);
    }
}


// Instantiation
CInstanceConfigurableElement *CBitParameterBlockType::doInstantiate() const
{
    return new CBitParameterBlock(getName(), this);
}