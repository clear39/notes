/*
<EnumParameter Name="volume_profile" Size="32">
	<ValuePair Literal="voice_call" Numerical="0"/>
	<ValuePair Literal="system" Numerical="1"/>
</EnumParameter>
*/


class CEnumParameterType : public CParameterType{

}


CEnumParameterType::CEnumParameterType(const string &strName) : base(strName)
{
}

bool CEnumParameterType::childrenAreDynamic() const
{
    return true;
}

bool CEnumParameterType::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Size in bits
    size_t sizeInBits = 0;
    // sizeInBits = 32
    if (not xmlElement.getAttribute("Size", sizeInBits)) {
        return false;
    }

    // Size
    setSize(sizeInBits / 8); // 4

    // Base
    return base::fromXml(xmlElement, serializingContext);
}

// Size
void CParameterType::setSize(size_t size)
{
    _size = size;
}


// From IXmlSink
bool CParameterType::fromXml(const CXmlElement &xmlElement,CXmlSerializingContext &serializingContext)
{
	// const std::string CParameterType::gUnitPropertyName = "Unit";
    // 在CEnumParameterType中没有该属性
    xmlElement.getAttribute(gUnitPropertyName, _strUnit);

    return base::fromXml(xmlElement, serializingContext);
}


bool CTypeElement::fromXml(const CXmlElement &xmlElement,CXmlSerializingContext &serializingContext)
{
    // Array Length attribute
    // 在CEnumParameterType中没有该属性
    xmlElement.getAttribute("ArrayLength", _arrayLength);
    // Manage mapping attribute
    std::string rawMapping;
    // 在CEnumParameterType中没有该属性
    if (xmlElement.getAttribute("Mapping", rawMapping) && !rawMapping.empty()) {

        std::string error;
        if (!getMappingData()->init(rawMapping, error)) {
            serializingContext.setError("Invalid Mapping data from XML element '" + xmlElement.getPath() + "': " + error);
            return false;
        }
    }
    return base::fromXml(xmlElement, serializingContext);
}


// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement,
                       CXmlSerializingContext &serializingContext) try {
    // 在CEnumParameterType中没有该属性
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        // CEnumParameterType::childrenAreDynamic() 返回 true
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {

                serializingContext.setError("Unable to handle XML element: " +
                                            childElement.getPath());

                return false;
            }

        } else {
        	// <ValuePair Literal="voice_call" Numerical="0"/>
        	//pParameterCreationLibrary->addElementBuilder("ValuePair",new TElementBuilderTemplate<CEnumValuePair>());
            // Child needs creation
            // pChild = CEnumValuePair
            pChild = createChild(childElement, serializingContext);
            if (!pChild) {

                return false;
            }
        }

        // Dig
        if (!pChild->fromXml(childElement, serializingContext)) {

            return false;
        }
    }

    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
CInstanceConfigurableElement *CTypeElement::instantiate() const
{   
    // pInstanceConfigurableElement = CParameter
    CInstanceConfigurableElement *pInstanceConfigurableElement = doInstantiate();

    // Populate
    populate(pInstanceConfigurableElement);

    return pInstanceConfigurableElement;
}


// Parameter instantiation
CInstanceConfigurableElement *CParameterType::doInstantiate() const
{
    if (isScalar()) {//true
        // Scalar parameter
        return new CParameter(getName(), this);
    } else {
        // Array Parameter
        return new CArrayParameter(getName(), this);
    }
}

bool CTypeElement::isScalar() const
{
    return !_arrayLength;
}


// Object creation
void CParameterType::populate(CElement * /*elem*/) const
{
    // Prevent further digging for instantiaton since we're leaf on the strcture tree
}
