
class CComponentInstance : public CTypeElement {

}

// 
CComponentInstance::CComponentInstance(const std::string &strName) : base(strName)
{
}

//<Component Name="applicable_volume_profile" Type="VolumeProfileType" Description="Volume profile followed by a given stream type."/>
bool CComponentInstance::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Context
    CXmlParameterSerializingContext &parameterBuildContext =
        static_cast<CXmlParameterSerializingContext &>(serializingContext);

    //
    const CComponentLibrary *pComponentLibrary = parameterBuildContext.getComponentLibrary();

    std::string strComponentType;
    xmlElement.getAttribute("Type", strComponentType);

    //这里回去查找是从对应的CComponentLibrary中去找Type
    // _pComponentType 得到 （标签ComponentType)
    _pComponentType = pComponentLibrary->getComponentType(strComponentType);

    if (!_pComponentType) {
        serializingContext.setError("Unable to create Component " + xmlElement.getPath() +
                                    ". ComponentType " + strComponentType + " not found!");

        return false;
    }
    if (_pComponentType == getParent()) {
        serializingContext.setError("Recursive definition of " + _pComponentType->getName() +
                                    " due to " + xmlElement.getPath() +
                                    " referring to one of its own type.");

        return false;
    }

    return base::fromXml(xmlElement, serializingContext);
}


bool CTypeElement::fromXml(const CXmlElement &xmlElement,
                           CXmlSerializingContext &serializingContext)
{
    // Array Length attribute
    // 在 CComponentInstance 中没有该属性
    xmlElement.getAttribute("ArrayLength", _arrayLength);
    // Manage mapping attribute
    std::string rawMapping;
    // 绝大部分CComponentInstance 中存在该属性，如下：
    /*
    <ComponentType Name="InputSource">
        <Component Name="applicable_input_device" Type="InputDevicesMask" Mapping="InputSource" Description="Selected Input device"/>
    </ComponentType>
    */
    if (xmlElement.getAttribute("Mapping", rawMapping) && !rawMapping.empty()) {

        std::string error;
        //创建CMappingData，并且将值传递给CMappingData进行存储
        if (!getMappingData()->init(rawMapping, error)) {

            serializingContext.setError("Invalid Mapping data from XML element '" +
                                        xmlElement.getPath() + "': " + error);
            return false;
        }
    }
    //到此 CComponentInstance 无子成员
    return base::fromXml(xmlElement, serializingContext);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//CSubsystem::structureFromXml
//CInstanceDefinition::createInstances  
//CTypeElement::populate
// 这里只有 CInstanceDefinition 中的 CComponentInstance(对应Component) 才会执行这里
CInstanceConfigurableElement *CTypeElement::instantiate() const
{
    // doInstantiate 创建一个 CComponent 实例
    // pInstanceConfigurableElement = CComponent 
    CInstanceConfigurableElement *pInstanceConfigurableElement = doInstantiate();

    // Populate
    // pInstanceConfigurableElement = CComponent
    populate(pInstanceConfigurableElement);

    return pInstanceConfigurableElement;
}

// 
CInstanceConfigurableElement *CComponentInstance::doInstantiate() const
{
    // isScalar 返回 true
    if (isScalar()) {
        /*
        <Component Name="streams" Type="Streams"/>  （CComponentInstance）
        <Component Name="input_sources" Type="InputSources"/> （CComponentInstance）
        <Component Name="product_strategies" Type="ProductStrategies"/> （CComponentInstance）
        */
        // getName()得到上面对应的 Name 属性值
        // this 为上面对应的三个 CComponentInstance 
        return new CComponent(getName(), this);
    } else {
        return new CParameterBlock(getName(), this);
    }
}

bool CTypeElement::isScalar() const
{
    return !_arrayLength;//均为0
}

size_t CTypeElement::getArrayLength() const
{
    return _arrayLength;
}

//pElement = CComponent
void CComponentInstance::populate(CElement *pElement) const
{
    size_t arrayLength = getArrayLength();

    if (arrayLength != 0) {

        // Create child elements
        for (size_t child = 0; child < arrayLength; child++) {

            CComponent *pChildComponent = new CComponent(std::to_string(child), this);

            pElement->addChild(pChildComponent);

            base::populate(pChildComponent);

            // 得到对应的 CComponentType(标签ComponentType)
            _pComponentType->populate(pChildComponent);
        }
    } else {
        // CTypeElement::populate
        //可以忽略，由于 InstanceDefinition 标签的子标签 Component(CComponentInstance) 没有 孩子成员，这里相当于没有任何处理
        base::populate(pElement);  // pElement = CComponent

        /*
        <Component Name="streams" Type="Streams"/>  （CComponentInstance）
        <Component Name="input_sources" Type="InputSources"/> （CComponentInstance）
        <Component Name="product_strategies" Type="ProductStrategies"/> （CComponentInstance）
        */
        // _pComponentType = CComponentType 是根据Type属性值在 CComponentInstance::fromXml 赋值
        // 根据"Type"属性值在 CComponentLibrary(标签ComponentLibrary) 中查找对应的成员得到 _pComponentType
        // 这里最终会触发三种 CComponentType 调用 populate 函数
        _pComponentType->populate(static_cast<CComponent *>(pElement)); // pElement = CComponent
    }
}

// pElement = CComponent
// 可以忽略，由于 InstanceDefinition 标签的子标签 Component 没有 孩子成员，这里相当于没有任何处理
void CTypeElement::populate(CElement *pElement) const
{
    // Populate children
    size_t uiChild;
    size_t uiNbChildren = getNbChildren();

    // uiNbChildren 为 0
    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

        const CTypeElement *pChildTypeElement =
            static_cast<const CTypeElement *>(getChild(uiChild));

        // 
        CInstanceConfigurableElement *pInstanceConfigurableChildElement =
            pChildTypeElement->instantiate();

        // Affiliate
        // 
        pElement->addChild(pInstanceConfigurableChildElement);
    }
}