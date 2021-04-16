class CComponentType : public CTypeElement {

}

//对应标签 <ComponentType Name="VolumeProfileType"> 
CComponentType::CComponentType(const std::string &strName) : base(strName)
{
}


/*
*
*/
bool CComponentType::fromXml(const CXmlElement &xmlElement,CXmlSerializingContext &serializingContext)
{
    // Context
    CXmlParameterSerializingContext &parameterBuildContext = lParameterSerializingContext &>(serializingContext);

    // pComponentLibrary 只有一个
    const CComponentLibrary *pComponentLibrary = parameterBuildContext.getComponentLibrary();

    // Populate children
    // CTypeElement::fromXml
    if (!base::fromXml(xmlElement, serializingContext)) {
        return false;
    }

    // Check for Extends attribute (extensions will be populated after and not before)
    // xml文件中 均没有 "Extends" 属性
    if (xmlElement.hasAttribute("Extends")) {

        std::string strExtendsType;
        xmlElement.getAttribute("Extends", strExtendsType);

        _pExtendsComponentType = pComponentLibrary->getComponentType(strExtendsType);

        if (!_pExtendsComponentType) {

            serializingContext.setError("ComponentType " + strExtendsType + " referred to by " +
                                        xmlElement.getPath() + " not found!");

            return false;
        }

        if (_pExtendsComponentType == this) {

            serializingContext.setError("Recursive ComponentType definition of " +
                                        xmlElement.getPath());

            return false;
        }
    }

    return true;
}


bool CTypeElement::fromXml(const CXmlElement &xmlElement,CXmlSerializingContext &serializingContext)
{
    // Array Length attribute
    // ComponentType 中没有"ArrayLength"属性
    xmlElement.getAttribute("ArrayLength", _arrayLength);
    // Manage mapping attribute
    std::string rawMapping;
    // ComponentType 只有俩个有 "Mapping" 属性会创建
    // 有"Mapping" 属性会创建CMappingData用于存储键值对(这里只有key没有值)
    // 只有 PolicySubsystem-CommonTypes.xml 中的一下俩个有Mapping
    /*
   <!--ComponentType 对应 CComponentType-->
    <ComponentType Name="Stream" Mapping="Stream">
        <!--Component 对应 CComponentInstance-->
        <Component Name="applicable_volume_profile" Type="VolumeProfileType" Description="Volume profile followed by a given stream type."/>
    </ComponentType>

    <ComponentType Name="InputSource">
        <Component Name="applicable_input_device" Type="InputDevicesMask" Mapping="InputSource" Description="Selected Input device"/>
    </ComponentType>

    <ComponentType Name="ProductStrategy" Mapping="ProductStrategy">
        <Component Name="selected_output_devices" Type="OutputDevicesMask"/>
        <!--Component 对应 CStringParameterType -->
        <StringParameter Name="device_address" MaxLength="256" Description="if any, device address associated"/>
    </ComponentType>
    */
    if (xmlElement.getAttribute("Mapping", rawMapping) && !rawMapping.empty()) {

        std::string error;
        //CMappingData 只有一个 key 没有，对应的value为空
        // getMappingData 判断_pMappingData是否为空，为空进行初始化创建
        // init函数 解析rawMapping(可能有":")，并加入到CMappingData的map列表中
        if (!getMappingData()->init(rawMapping, error)) {
            serializingContext.setError("Invalid Mapping data from XML element '" +
                                        xmlElement.getPath() + "': " + error);
            return false;
        }
    }

    return base::fromXml(xmlElement, serializingContext);
}

CMappingData *CTypeElement::getMappingData()
{
    if (!_pMappingData) {

        _pMappingData = new CMappingData;
    }
    return _pMappingData;
}


// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {
	// 部分 CComponentType 有该属性
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        // 由于 CComponentType 实现了 childrenAreDynamic 方法，返回 true
        if (!childrenAreDynamic()) {
            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {
                serializingContext.setError("Unable to handle XML element: " + childElement.getPath());
                return false;
            }

        } else {
        	//执行这里
            // Child needs creation
             //有4种类型，注意在解析是有顺序的，在CComponentInstance::fromXml中会查询根据Type属性对应的CComponentType，否则报错
            // pParameterCreationLibrary->addElementBuilder("BitParameterBlock", new TNamedElementBuilderTemplate<CBitParameterBlockType>());
            // pParameterCreationLibrary->addElementBuilder("EnumParameter", new TNamedElementBuilderTemplate<CEnumParameterType>());
            // pParameterCreationLibrary->addElementBuilder("Component", new TNamedElementBuilderTemplate<CComponentInstance>());
            // pParameterCreationLibrary->addElementBuilder("StringParameter", new TNamedElementBuilderTemplate<CStringParameterType>());
            //通过上面4中构建器，得到一下始终类型：
            // CBitParameterBlockType   //有子成员(CBitParameterType)
            // CEnumParameterType       //有子成员(CEnumValuePair)
            // CComponentInstance       //无子成员，这种类型在其fromXml函数中查找已经存在的CComponentType，所以顺序不能乱修改
            // CStringParameterType     //无子成员
            pChild = createChild(childElement, serializingContext);
            if (!pChild) {

                return false;
            }
        }

        // Dig
        //有4种类型
        // CComponentInstance::fromXml
        // CBitParameterBlockType::fromXml
        // CEnumParameterType::fromXml 
        // CStringParameterType::fromXml 
        if (!pChild->fromXml(childElement, serializingContext)) {
            return false;
        }
    }

    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}




/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
// 在CComponentInstance::populate中调用
// pElement = CComponent
void CComponentType::populate(CElement *pElement) const
{
    // Populate children
    base::populate(pElement);

    // Manage extended type
    // 由于 没有对应 Extends 属性，所以这里不执行
    if (_pExtendsComponentType) {

        // Populate from extended type
        _pExtendsComponentType->populate(pElement);
    }
}

//由于这里只会有以下三种CComponentType的子成员均为 会调用populate函数
/*
<ComponentType Name="Streams" Description="associated to audio_stream_type_t definition">
<ComponentType Name="InputSources" Description="associated to audio_source_t definition, identifier mapping must match the value of the enum">
<ComponentType Name="ProductStrategies" Description="">
以上三种对应的CComponentType的子成员均为 CComponentInstance
在CComponentInstance的instantiate()方法中再次创建CComponent实例，同时还会调用其属性Type对应的CComponentType，再次调用populate函数；
其中 CComponentType 没有创建任何类;
每一个CComponentInstance(标签Component) 均有Type属性，最终递归代用到一下三种类型截止
CBitParameterBlockType
CEnumParameterType
CStringParameterType
再次分析以上三个类的instantiate()函数；

对于CBitParameterBlockType如下:
在CBitParameterBlockType的instantiate()函数中创建的是CBitParameterBlock实例(不是CComponent),
在CBitParameterBlockType的子成员CBitParameterType::doInstantiate()函数中创建的是CBitParameter实例(不是CComponent);

对于CEnumParameterType如下:
在CEnumParameterType的instantiate()函数中创建的是CParameter实例(不是CComponent),
虽然CEnumParameterType有子成员(CEnumValuePair)，但是CEnumParameterType的父类CParameterType的populate空实现，到此结束了

对于CStringParameterType如下:
在CStringParameterType的instantiate()函数中创建的是CStringParameter实例(不是CComponent),CStringParameterType无子成员

*/
// pElement = CComponent
void CTypeElement::populate(CElement *pElement) const
{
    // Populate children
    size_t uiChild;
    size_t uiNbChildren = getNbChildren();
    /*
    子类型有三种
    CComponentInstance (对应标签Component)
    但是这里只会CComponentInstance会调用
    */
    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

        // 每个孩子成员为 CComponentInstance
        const CTypeElement *pChildTypeElement =
            static_cast<const CTypeElement *>(getChild(uiChild));

        // 根据type的类型不同，子成员有三种类型
        // pInstanceConfigurableChildElement = new CComponent(getName(), this /*CComponentInstance*/);
        // pInstanceConfigurableChildElement = new CParameter(getName(), this /*CEnumParameterType*/);
        // pInstanceConfigurableChildElement = new CBitParameterBlock(getName(), this /*CBitParameterBlockType*/);  
        CInstanceConfigurableElement *pInstanceConfigurableChildElement =
            pChildTypeElement->instantiate();

        // Affiliate
        // 将 CComponent 添加 CComponent
        pElement->addChild(pInstanceConfigurableChildElement);
    }
}



