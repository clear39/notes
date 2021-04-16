CSubsystem::CSubsystem(const string &strName, core::log::Logger &logger)
    : base(strName), _pComponentLibrary(new CComponentLibrary),
      _pInstanceDefinition(new CInstanceDefinition), _logger(logger)
{
    // Note: A subsystem contains instance components
    // InstanceDefintion and ComponentLibrary objects are then not chosen to be children
    // They'll be delt with locally
}

// Subsystem context mapping keys publication
void CSubsystem::addContextMappingKey(const string &strMappingKey)
{
    //std::vector<std::string> _contextMappingKeyArray;
    _contextMappingKeyArray.push_back(strMappingKey);
}

// Subsystem object creator publication (strong reference)
void CSubsystem::addSubsystemObjectFactory(CSubsystemObjectCreator *pSubsystemObjectCreator)
{
    _subsystemObjectCreatorArray.push_back(pSubsystemObjectCreator);
}


//解析 /vendor/etc/parameter-framework/Structure/Policy/PolicySubsystem.xml 
bool CSubsystem::structureFromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Subsystem class does not rely on generic fromXml algorithm of Element class.
    // So, setting here the description if found as XML attribute.
    string description;
    //没有该属性 const std::string CElement::gDescriptionPropertyName = "Description";
    xmlElement.getAttribute(gDescriptionPropertyName, description);
    //这里为空
    setDescription(description);

    // 在 CParameterMgr::loadStructure 创建就是 CXmlParameterSerializingContext
    // Context
    CXmlParameterSerializingContext &parameterBuildContext = static_cast<CXmlParameterSerializingContext &>(serializingContext);

    
    // Install temporary component library for further component creation
    // _pComponentLibrary 为 CComponentLibrary
    parameterBuildContext.setComponentLibrary(_pComponentLibrary);

    CXmlElement childElement;

    // Manage mapping attribute
    string rawMapping;
    // 由于标签 Subsystem 中没有 该属性值
    xmlElement.getAttribute("Mapping", rawMapping);
    if (!rawMapping.empty()) {
        std::string error;
        _pMappingData = new CMappingData;
        if (!_pMappingData->init(rawMapping, error)) {
            serializingContext.setError("Invalid Mapping data from XML element '" + xmlElement.getPath() + "': " + error);
            return false;
        }
    }

    // 解析 PolicySubsystem.xml PolicySubsystem-CommonTypes.xml ProductStrategies.xml
    // XML populate ComponentLibrary
    xmlElement.getChildElement("ComponentLibrary", childElement);
    // _pComponentLibrary 为 CComponentLibrary
    // 这里解析 PolicySubsystem.xml PolicySubsystem-CommonTypes.xml ProductStrategies.xml
    // CComponentLibrary 的子成员均为 CComponentType
    // CComponentType 的子成员有   CBitParameterBlockType,CEnumParameterType,CComponentInstance，CStringParameterType
    // CBitParameterBlockType   //有子成员(CBitParameterType)
    // CEnumParameterType       //有子成员(CEnumValuePair)
    // CComponentInstance       //无子成员，这种类型在其fromXml函数中查找已经存在的CComponentType，所以顺序不能乱修改
    // CStringParameterType     //无子成员
    // CComponentInstance 子成员通过其Type属性值到CComponentLibrary查找对应的CComponentType(只是确认是否存在，并且保存CComponentInstance的_pComponentType变量中)
    // 其中有mapping属性的，均创建CMappingData，进行存储
    if (!_pComponentLibrary->fromXml(childElement, serializingContext)) {
        return false;
    }

    //获取标签为InstanceDefintion的根节点
    // XML populate InstanceDefintion
    xmlElement.getChildElement("InstanceDefintion", childElement);
    // _pInstanceDefinition 为 CInstanceDefinition
    /*
   <InstanceDefinition>
        <!--Component每个标签对应 CComponentInstance 类-->
        <!--Type中的值 对应上面 ComponentLibrary 中的 ComponentType类型-->
        <Component Name="streams" Type="Streams"/>
        <Component Name="input_sources" Type="InputSources"/>
        <Component Name="product_strategies" Type="ProductStrategies"/>
    </InstanceDefinition>
    */
    // 由于 CInstanceDefinition 未重载 fromXml，所以调用父类 CTypeElement::fromXml
    //这里CInstanceDefinition得到3个子成员均为CComponentInstance
    //CComponentInstance 子成员通过其Type属性值到CComponentLibrary查找对应的CComponentType(只是确认是否存在，并且保存CComponentInstance的_pComponentType变量中)
    if (!_pInstanceDefinition->fromXml(childElement, serializingContext)) {
        return false;
    }

    // Create components
    // this 为 PolicySubsystem
    // 在 createInstances 中会 PolicySubsystem 添加子成员
    // 创建 CComponent 添加到 PolicySubsystem 中
    // _pInstanceDefinition = CInstanceDefinition
    _pInstanceDefinition->createInstances(this);

    // Execute mapping to create subsystem mapping entities
    string strError;
    // mapSubsystemElements 函数作用:
    // 创建 Stream InputSource ProductStrategy 三种类，均会加入到 _subsystemObjectList
    // 对应的 CComponent 也会存储到 Stream InputSource ProductStrategy 中
    if (!mapSubsystemElements(strError)) {
        serializingContext.setError(strError);
        return false;
    }

    return true;
}

// CSubsystem 先检测CMappingData，由于CSubsystem没有对应的mapping属性，handleMappingContext 可以忽略
// 遍历所有CSubsystem的子成员的map函数，并将CSubsystem传入函数调用接口中；
// CSubsystem 有3个子成员均为CComponent(内部成员指向CComponentInstance)
// CComponent 的map函数中:
// 对应的子成员CComponent的内部成员指向CComponentInstance是否有CMappingData，显然CSubsystem的子成员CComponent的内部成员指向CComponentInstance没有
// 直接遍历 CComponent的子成员;
// 由于其子的成员 CComponent的内部成员指向CComponentInstance 存在CMappingData，所以会调用 CSubsystem 的 mapBegin 函数
// 在 CSubsystem的 mapBegin 函数中
bool CSubsystem::mapSubsystemElements(string &strError)
{
    // Default mapping context
    // _contextMappingKeyArray 是在 PolicySubsystem 构造函数中通过 addContextMappingKey 添加的 6个成员
    CMappingContext context(_contextMappingKeyArray.size());
    // Add Subsystem-level mapping data, which will be propagated to all children
    //  由于 PolicySubsystem 没有 CMappingData 啥事也没有干
    handleMappingContext(this, context, strError);

    _contextStack.push(context);

    // Map all instantiated subelements in subsystem
    size_t nbChildren = getNbChildren();


    for (size_t child = 0; child < nbChildren; child++) {
        // PolicySubsystem 的3个子成员均为CComponent
        // pInstanceConfigurableChildElement = CComponent
        CInstanceConfigurableElement *pInstanceConfigurableChildElement = static_cast<CInstanceConfigurableElement *>(getChild(child));
        //注意这里传入的是 PolicySubsystem
        if (!pInstanceConfigurableChildElement->map(*this, strError)) {
            return false;
        }
    }

    return true;
}


// Mapping generic context handling
// pConfigurableElement = PolicySubsystem(CSubsystem为父类)
bool CSubsystem::handleMappingContext(const CConfigurableElement *pConfigurableElement,
                                      CMappingContext &context, string &strError) const
{
    // Feed context with found mapping data
    /*
    mKeyName = "Name";
    mKeyCategory = "Category";
    mKeyIdentifier = "Identifier";
    mKeyAmend1 = "Amend1";
    mKeyAmend2 = "Amend2";
    mKeyAmend3 = "Amend3";
    */
    for (size_t item = 0; item < _contextMappingKeyArray.size(); item++) {

        const string &strKey = _contextMappingKeyArray[item];
        const string *pStrValue;

        // pConfigurableElement = PolicySubsystem
        // PolicySubsystem 没有 CMappingData
        if (pConfigurableElement->getMappingData(strKey, pStrValue)) {
            // Assign item to context
            // item 对应 _contextMappingKeyArray 的索引值
            if (!context.setItem(item, &strKey, pStrValue)) {

                strError = getMappingError(strKey, "Already set", pConfigurableElement);

                return false;
            }
        }
    }
    return true;
}


bool CSubsystem::getMappingData(const std::string &strKey, const std::string *&pStrValue) const
{
    if (_pMappingData) {
        return _pMappingData->getValue(strKey, pStrValue);
    }
    return false;
}



// From IMapper
// Handle a configurable element mapping
// pInstanceConfigurableElement = CComponent
bool CSubsystem::mapBegin(CInstanceConfigurableElement *pInstanceConfigurableElement,
                          bool &bKeepDiving, string &strError)
{
    // Get current context
    CMappingContext context = _contextStack.top();

    // Add mapping in context
    // pInstanceConfigurableElement = CComponent
    // 
    if (!handleMappingContext(pInstanceConfigurableElement, context, strError)) {
        return false;
    }

    // Push context
    _contextStack.push(context);

    // Assume diving by default
    bKeepDiving = true;

    // Deal with ambiguous usage of parameter blocks
    bool bShouldCreateSubsystemObject = true;

    switch (pInstanceConfigurableElement->getType()) {

    case CInstanceConfigurableElement::EComponent:
    case CInstanceConfigurableElement::EParameterBlock:
        // Subsystem object creation is optional in parameter blocks
        bShouldCreateSubsystemObject = false;
    // No break
    case CInstanceConfigurableElement::EBitParameterBlock:
    case CInstanceConfigurableElement::EParameter:
    case CInstanceConfigurableElement::EStringParameter:

        bool bHasCreatedSubsystemObject;
        // 这里创建 Stream InputSource ProductStrategy 三种类，均会加入到 _subsystemObjectList
        // 对饮的 pInstanceConfigurableElement 也会存储到 Stream InputSource ProductStrategy 中
        if (!handleSubsystemObjectCreation(pInstanceConfigurableElement, context, bHasCreatedSubsystemObject, strError)) {

            return false;
        }
        // Check for creation error
        if (bShouldCreateSubsystemObject && !bHasCreatedSubsystemObject) {

            strError = getMappingError("Not found", "Subsystem object mapping key is missing", pInstanceConfigurableElement);
            return false;
        }
        // Not created and no error, keep diving
        // 如果 创建 Stream InputSource ProductStrategy 三种类，bHasCreatedSubsystemObject为true
        // 则 bKeepDiving 为 false
        bKeepDiving = !bHasCreatedSubsystemObject;

        return true;

    default:
        assert(0);
        return false;
    }
}

void CSubsystem::mapEnd()
{
    // Unstack context
    _contextStack.pop();
}



// Subsystem object creation handling
bool CSubsystem::handleSubsystemObjectCreation(
    CInstanceConfigurableElement *pInstanceConfigurableElement, CMappingContext &context,
    bool &bHasCreatedSubsystemObject, string &strError)
{
    bHasCreatedSubsystemObject = false;

    // _subsystemObjectCreatorArray 在 PolicySubsystem 构造函数中添加的工厂构建类
    // 有以下三种值
    // mStreamComponentName = "Stream";  对应的类为 Stream
    // mInputSourceComponentName = "InputSource";  对应的类为 InputSource
    //mProductStrategyComponentName = "ProductStrategy"; 对应的类为 ProductStrategy
    // pSubsystemObjectCreator = CSubsystemObjectCreator
    for (const auto *pSubsystemObjectCreator : _subsystemObjectCreatorArray) {

        // Mapping key
        // 通过 pSubsystemObjectCreator->getMappingKey() 得到以下三种字符串值
        //  "Stream" "InputSource"  "ProductStrategy"
        string strKey = pSubsystemObjectCreator->getMappingKey();
        // Object id
        const string *pStrValue;

        // 这里获取 CComponent 中的mapping对应的匹配的键值对的值存入pStrValue中
        if (pInstanceConfigurableElement->getMappingData(strKey, pStrValue中)) {

            // First check context consistency
            // (required ancestors must have been set prior to object creation)
            uint32_t uiAncestorMask = pSubsystemObjectCreator->getAncestorMask(); // 都为 1

            for (size_t uiAncestorKey = 0; uiAncestorKey < _contextMappingKeyArray.size();
                 uiAncestorKey++) {

                if (!((1 << uiAncestorKey) & uiAncestorMask)) {
                    // Ancestor not required
                    continue;
                }
                // Check ancestor was provided
                if (!context.iSet(uiAncestorKey)) {

                    strError =
                        getMappingError(strKey, _contextMappingKeyArray[uiAncestorKey] + " not set",
                                        pInstanceConfigurableElement);

                    return false;
                }
            }

            // Then check configurable element size is correct
            // 
            if (pInstanceConfigurableElement->getFootPrint() > pSubsystemObjectCreator->getMaxConfigurableElementSize()) {

                string strSizeError =
                    "Size should not exceed " +
                    std::to_string(pSubsystemObjectCreator->getMaxConfigurableElementSize());

                strError = getMappingError(strKey, strSizeError, pInstanceConfigurableElement);

                return false;
            }

            // Do create object and keep its track
            // 这里创建 Stream InputSource ProductStrategy 三种类，均会加入到 _subsystemObjectList
            // 对饮的 pInstanceConfigurableElement 也会存储到 Stream InputSource ProductStrategy 中
            // *pStrValue 对应mapping键值对的值
            _subsystemObjectList.push_back(pSubsystemObjectCreator->objectCreate(
                *pStrValue, pInstanceConfigurableElement, context, _logger));

            // Indicate subsytem creation to caller
            bHasCreatedSubsystemObject = true;

            // The subsystem Object has been instantiated, no need to continue looking for an
            // instantiation mapping
            break;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

const CElement *CElement::findDescendant(CPathNavigator &pathNavigator) const
{
    // product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
    string *pStrChildName = pathNavigator.next(); // product_strategies

    if (!pStrChildName) {

        return this;
    }

    const CElement *pChild = findChild(*pStrChildName);

    if (!pChild) {

        return nullptr;
    }

    return pChild->findDescendant(pathNavigator);
}