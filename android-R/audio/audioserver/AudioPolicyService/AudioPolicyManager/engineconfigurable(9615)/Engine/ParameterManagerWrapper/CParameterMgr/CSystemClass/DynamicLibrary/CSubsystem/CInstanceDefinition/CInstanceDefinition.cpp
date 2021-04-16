

class CInstanceDefinition : public CTypeElement
{
public:
    void createInstances(CElement *pFatherElement);

    std::string getKind() const override;

private:
    bool childrenAreDynamic() const override;
    CInstanceConfigurableElement *doInstantiate() const override;
};




/*
<InstanceDefinition>
    <Component Name="streams" Type="Streams"/>
    <Component Name="input_sources" Type="InputSources"/>
    <Component Name="product_strategies" Type="ProductStrategies"/>
</InstanceDefinition>
*/
bool CTypeElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Array Length attribute
    xmlElement.getAttribute("ArrayLength", _arrayLength);
    // Manage mapping attribute
    std::string rawMapping;
    if (xmlElement.getAttribute("Mapping", rawMapping) && !rawMapping.empty()) {

        std::string error;
        if (!getMappingData()->init(rawMapping, error)) {
            serializingContext.setError("Invalid Mapping data from XML element '" + xmlElement.getPath() + "': " + error);
            return false;
        }
    }
    // CElement::fromXml(xmlElement, serializingContext);
    return base::fromXml(xmlElement, serializingContext);
}


// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {
	//CInstanceDefinition 不存在该属性
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    /*
    <Component Name="streams" Type="Streams"/>
    <Component Name="input_sources" Type="InputSources"/>
    <Component Name="product_strategies" Type="ProductStrategies"/>
    */

    while (childIterator.next(childElement)) {

        CElement *pChild;
        // CInstanceDefinition 实现 childrenAreDynamic 方法，返回 true
        if (!childrenAreDynamic()) {
            pChild = findChildOfKind(childElement.getType());
            if (!pChild) {
                serializingContext.setError("Unable to handle XML element: " + childElement.getPath());
                return false;
            }

        } else {
        	//执行这里
            // Child needs creation
             // pChild 为 CComponentInstance
            pChild = createChild(childElement, serializingContext);

            if (!pChild) {

                return false;
            }
        }

        // Dig
        // pChild 为 CComponentInstance
        if (!pChild->fromXml(childElement, serializingContext)) {
            return false;
        }
    }
    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}

CElement *CElement::createChild(const CXmlElement &childElement, CXmlSerializingContext &serializingContext)
{
    // Context
    CXmlElementSerializingContext &elementSerializingContext = static_cast<CXmlElementSerializingContext &>(serializingContext);

    // Child needs creation
    // 这里elementSerializingContext.getElementLibrary()还是返回 CParameterMgr::_pElementLibrarySet中的第二个成员
    // pParameterCreationLibrary->addElementBuilder("Component", new TNamedElementBuilderTemplate<CComponentInstance>());
    // 会调用 TNamedElementBuilderTemplate->createElement(childElement);
    // 再 new CComponentInstance(xmlElement.getNameAttribute());  // xmlElement.getNameAttribute() 获取 Name的属性值
    // pChild 为 CComponentInstance
    CElement *pChild = elementSerializingContext.getElementLibrary()->createElement(childElement);

    if (!pChild) {
        elementSerializingContext.setError("Unable to create XML element " + childElement.getPath());

        return nullptr;
    }
    // Store created child!
    addChild(pChild);

    return pChild;
}






/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// 在 CSubsystem::structureFromXml 中调用
// 这里 pFatherElement 为 PolicySubsystem (CSubsystem子类)
void CInstanceDefinition::createInstances(CElement *pFatherElement)
{
    populate(pFatherElement);
}

void CTypeElement::populate(CElement *pElement) const
{
    // 3个成员 
    /*
    <Component Name="streams" Type="Streams"/>  （CComponentInstance）
    <Component Name="input_sources" Type="InputSources"/> （CComponentInstance）
    <Component Name="product_strategies" Type="ProductStrategies"/> （CComponentInstance）
    */
    // Populate children
    size_t uiChild;
    // CInstanceDefinition通过forXml解析得到3个子成员CComponentInstance
    size_t uiNbChildren = getNbChildren();

    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

         // 每个子成员为 CComponentInstance 
        const CTypeElement *pChildTypeElement =
            static_cast<const CTypeElement *>(getChild(uiChild));

        // pChildTypeElement = CComponentInstance
        // CComponentInstance 没有实现 instantiate 方法,在CTypeElement::instantiate()中实现
        // 每个孩子成员为 pInstanceConfigurableChildElement = new CComponent(getName(), this /*CComponentInstance*/);
        CInstanceConfigurableElement *pInstanceConfigurableChildElement = pChildTypeElement->instantiate();

        // Affiliate
        // 将 CComponent 添加到 pElement(PolicySubsystem) 中
        // PolicySubsystem 有三个成员一下的 CComponent(第一个参数为Name对应的熟悉值)
        // Streams 用于在ComponentLibrary查找对应对应的CComponentType将其的CComponentInstance(对应标签Component) 转成CComponent
        // 添加到通过下面三个创建的CComponent中(作为其孩子成员)
        /*
        <Component Name="streams" Type="Streams"/>  （CComponentInstance）
        <Component Name="input_sources" Type="InputSources"/> （CComponentInstance）
        <Component Name="product_strategies" Type="ProductStrategies"/> （CComponentInstance）
        */
        pElement->addChild(pInstanceConfigurableChildElement);
    }
}
