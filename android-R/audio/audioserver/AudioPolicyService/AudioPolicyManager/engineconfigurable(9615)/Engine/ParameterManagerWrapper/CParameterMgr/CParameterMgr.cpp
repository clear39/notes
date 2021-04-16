// Remote command parsers array Size
// 
CParameterMgr::CParameterMgr(const string &strConfigurationFilePath, log::ILogger &logger)
    : _pMainParameterBlackboard(new CParameterBlackboard),
      _pElementLibrarySet(new CElementLibrarySet),
      _xmlConfigurationUri(CXmlDocSource::mkUri(strConfigurationFilePath, "")), _logger(logger)
{
    // Deal with children
    // 由于 CElement 是 CParameterMgr的父类， addChild为从父类继承的函数
    // CParameterFrameworkConfiguration CSelectionCriteria CSystemClass CConfigurableDomains 均为 CElement 之类
    addChild(new CParameterFrameworkConfiguration); // 对应枚举EFrameworkConfiguration
    addChild(new CSelectionCriteria);       // 对应枚举 ESelectionCriteria
    addChild(new CSystemClass(_logger)); // 对应枚举 ESystemClass
    addChild(new CConfigurableDomains);// 对应枚举 EConfigurableDomains 
}

//在  CParameterMgrPlatformConnector::setValidateSchemasOnStart 调用，bValidate 为false
void CParameterMgr::setValidateSchemasOnStart(bool bValidate)
{
    _bValidateSchemasOnStart = bValidate;
}


// Selection criteria interface
CSelectionCriterionType *CParameterMgr::createSelectionCriterionType(bool bIsInclusive)
{
    // Propagate
    // getSelectionCriteria从数组中查找到 CSelectionCriteria
    return getSelectionCriteria()->createSelectionCriterionType(bIsInclusive);
}

CSelectionCriterion *CParameterMgr::createSelectionCriterion(
    const string &strName, const CSelectionCriterionType *pSelectionCriterionType)
{
    // Propagate
    // getSelectionCriteria从数组中查找到 CSelectionCriteria
    return getSelectionCriteria()->createSelectionCriterion(strName, pSelectionCriterionType, _logger);
}

CSelectionCriteria *CParameterMgr::getSelectionCriteria()
{
	/*
	ESelectionCriteria 为一个枚举
	和构造函数中addChild调用一一对应
	enum ChildElement
    {
        EFrameworkConfiguration,
        ESelectionCriteria,
        ESystemClass,
        EConfigurableDomains
    };
    这里返回 CSelectionCriteria
	*/
    return static_cast<CSelectionCriteria *>(getChild(ESelectionCriteria));
}




///////////////////////////////////////////////////////////////////////
bool CParameterMgr::load(string &strError)
{
	//日志打印
    LOG_CONTEXT("Loading");

    // 创建三个CElementLibrary并且加入到_pElementLibrarySet中
    feedElementLibraries();

    // Load Framework configuration
    //
    if (!loadFrameworkConfiguration(strError)) {

        return false;
    }

    if (!loadSubsystems(strError)) {
        return false;
    }

    // Load structure
    if (!loadStructure(strError)) {
        return false;
    }

    // Load settings
    if (!loadSettings(strError)) {
        return false;
    }

    // Init flow of element tree
    if (!init(strError)) {
        return false;
    }

    {
        LOG_CONTEXT("Main blackboard back synchronization");

        // Back synchronization for areas in parameter blackboard not covered by any domain
        BackSynchronizer(getConstSystemClass(), _pMainParameterBlackboard).sync();
    }

    // We're done loading the settings and back synchronizing
    CConfigurableDomains *pConfigurableDomains = getConfigurableDomains();

    // We need to ensure all domains are valid
    pConfigurableDomains->validate(_pMainParameterBlackboard);

    // Log selection criterion states
    {
        LOG_CONTEXT("Criterion states");

        const CSelectionCriteria *selectionCriteria = getConstSelectionCriteria();

        list<string> criteria;
        selectionCriteria->listSelectionCriteria(criteria, true, false);

        info() << criteria;
    }

    // Subsystem can not ask for resync as they have not been synced yet
    getSystemClass()->cleanSubsystemsNeedToResync();

    // At initialization, check subsystems that need resync
    doApplyConfigurations(true);

    // Start remote processor server if appropriate
    //handleRemoteProcessingInterface 内部没有任何动作
    return handleRemoteProcessingInterface(strError);
}


// Dynamic creation library feeding
void CParameterMgr::feedElementLibraries()
{
    /**
    * 其中 CElementLibrary.addElementBuilder 内部存储在 typedef std::map<std::string, const CElementBuilder *> ElementBuilderMap;
    */
    // Global Configuration handling
    auto pFrameworkConfigurationLibrary = new CElementLibrary;

    /*
    * TElementBuilderTemplate 模板类中实现 createElement 函数，作用是构造对应的类
    */
    pFrameworkConfigurationLibrary->addElementBuilder(
        "ParameterFrameworkConfiguration",
        new TElementBuilderTemplate<CParameterFrameworkConfiguration>());
    pFrameworkConfigurationLibrary->addElementBuilder(
        "SubsystemPlugins", new TKindElementBuilderTemplate<CSubsystemPlugins>());
    pFrameworkConfigurationLibrary->addElementBuilder(
        "Location", new TKindElementBuilderTemplate<CPluginLocation>());
    pFrameworkConfigurationLibrary->addElementBuilder(
        "StructureDescriptionFileLocation",
        new TKindElementBuilderTemplate<CFrameworkConfigurationLocation>());
    pFrameworkConfigurationLibrary->addElementBuilder(
        "SettingsConfiguration", new TKindElementBuilderTemplate<CFrameworkConfigurationGroup>());
    pFrameworkConfigurationLibrary->addElementBuilder(
        "ConfigurableDomainsFileLocation",
        new TKindElementBuilderTemplate<CFrameworkConfigurationLocation>());

    // CElementLibrarySet *_pElementLibrarySet;
    // CElementLibrarySet.addElementLibrary 内部存储在 std::vector<CElementLibrary *> _elementLibraryArray;
    _pElementLibrarySet->addElementLibrary(pFrameworkConfigurationLibrary);

    // Parameter creation
    auto pParameterCreationLibrary = new CElementLibrary;

    /*
    * TNamedElementBuilderTemplate 模板类中实现 createElement 函数，作用是构造对应的类
    * TElementBuilderTemplate 
    */
    //getSystemClass() 得到 CSystemClass 在构造方法中创建
    pParameterCreationLibrary->addElementBuilder(
        "Subsystem", new CSubsystemElementBuilder(getSystemClass()->getSubsystemLibrary()));

    pParameterCreationLibrary->addElementBuilder(
        "ComponentType", new TNamedElementBuilderTemplate<CComponentType>());
    pParameterCreationLibrary->addElementBuilder(
        "Component", new TNamedElementBuilderTemplate<CComponentInstance>());
    pParameterCreationLibrary->addElementBuilder(
        "BitParameter", new TNamedElementBuilderTemplate<CBitParameterType>());
    pParameterCreationLibrary->addElementBuilder(
        "BitParameterBlock", new TNamedElementBuilderTemplate<CBitParameterBlockType>());
    pParameterCreationLibrary->addElementBuilder(
        "StringParameter", new TNamedElementBuilderTemplate<CStringParameterType>());
    pParameterCreationLibrary->addElementBuilder(
        "ParameterBlock", new TNamedElementBuilderTemplate<CParameterBlockType>());
    pParameterCreationLibrary->addElementBuilder(
        "BooleanParameter", new TNamedElementBuilderTemplate<CBooleanParameterType>());
    pParameterCreationLibrary->addElementBuilder("IntegerParameter", new IntegerParameterBuilder());
    pParameterCreationLibrary->addElementBuilder(
        "LinearAdaptation", new TElementBuilderTemplate<CLinearParameterAdaptation>());
    pParameterCreationLibrary->addElementBuilder(
        "LogarithmicAdaptation", new TElementBuilderTemplate<CLogarithmicParameterAdaptation>());
    pParameterCreationLibrary->addElementBuilder(
        "EnumParameter", new TNamedElementBuilderTemplate<CEnumParameterType>());
    pParameterCreationLibrary->addElementBuilder("ValuePair",
                                                 new TElementBuilderTemplate<CEnumValuePair>());
    pParameterCreationLibrary->addElementBuilder(
        "FixedPointParameter", new TNamedElementBuilderTemplate<CFixedPointParameterType>());
    pParameterCreationLibrary->addElementBuilder(
        "FloatingPointParameter", new TNamedElementBuilderTemplate<CFloatingPointParameterType>);

    pParameterCreationLibrary->addElementBuilder(
        "SubsystemInclude",
        new CFileIncluderElementBuilder(_bValidateSchemasOnStart, getSchemaUri()));

    _pElementLibrarySet->addElementLibrary(pParameterCreationLibrary);



    // Parameter Configuration Domains creation
    auto pParameterConfigurationLibrary = new CElementLibrary;

    pParameterConfigurationLibrary->addElementBuilder(
        "ConfigurableDomain", new TElementBuilderTemplate<CConfigurableDomain>());
    pParameterConfigurationLibrary->addElementBuilder(
        "Configuration", new TNamedElementBuilderTemplate<CDomainConfiguration>());
    pParameterConfigurationLibrary->addElementBuilder("CompoundRule",
                                                      new TElementBuilderTemplate<CCompoundRule>());
    pParameterConfigurationLibrary->addElementBuilder(
        "SelectionCriterionRule", new TElementBuilderTemplate<CSelectionCriterionRule>());

    _pElementLibrarySet->addElementLibrary(pParameterConfigurationLibrary);
}

bool CParameterMgr::loadFrameworkConfiguration(string &strError)
{
    LOG_CONTEXT("Loading framework configuration");

    // Parse Structure XML file
    // CXmlElementSerializingContext 保存解析文件，上下文
    CXmlElementSerializingContext elementSerializingContext(strError);

    // _xmlConfigurationUri 为 /vendor/etc/parameter-framework/ParameterFrameworkConfigurationPolicy.xml
    // 得到 _xmlDoc
    _xmlDoc *doc = CXmlDocSource::mkXmlDoc(_xmlConfigurationUri, true, true, elementSerializingContext);
    if (doc == nullptr) {
        return false;
    }

    // getFrameworkConfiguration() 得到 CParameterFrameworkConfiguration
    // 这个 xmlParse 才是有主要作用：
    // 
    if (!xmlParse(elementSerializingContext, getFrameworkConfiguration(), doc, _xmlConfigurationUri,
                  EFrameworkConfigurationLibrary)) {

        return false;
    }

    // getConstFrameworkConfiguration()->getSystemClassName() 得到 SystemClassName="Policy"
    // Set class name to system class and configurable domains
    // getSystemClass() 得到 CSystemClass
    //getFrameworkConfiguration() 得到 CParameterFrameworkConfiguration
    getSystemClass()->setName(getConstFrameworkConfiguration()->getSystemClassName());
    // getConfigurableDomains() 得到 CConfigurableDomains
    getConfigurableDomains()->setName(getConstFrameworkConfiguration()->getSystemClassName());

    /**
     <SubsystemPlugins>
        <Location Folder="">
            <Plugin Name="libpolicy-subsystem.so"/>
        </Location>
    </SubsystemPlugins>
    */
    // Get subsystem plugins elements
    _pSubsystemPlugins = static_cast<const CSubsystemPlugins *>(
        getConstFrameworkConfiguration()->findChild("SubsystemPlugins"));

    if (!_pSubsystemPlugins) {

        strError = "Parameter Framework Configuration: couldn't find SubsystemPlugins element";

        return false;
    }

    // getConstFrameworkConfiguration()->isTuningAllowed() 得到 TuningAllowed="false"
    // Log tuning availability
    info() << "Tuning "
           << (getConstFrameworkConfiguration()->isTuningAllowed() ? "allowed" : "prohibited");

    return true;
}

// pRootElement 为 CParameterFrameworkConfiguration
// EFrameworkConfigurationLibrary 值为0
//xmlParse(elementSerializingContext, getFrameworkConfiguration(), doc, _xmlConfigurationUri,EFrameworkConfigurationLibrary)
// XML parsing
bool CParameterMgr::xmlParse(CXmlElementSerializingContext &elementSerializingContext,
                             CElement *pRootElement, _xmlDoc *doc, const string &baseUri,
                             CParameterMgr::ElementLibrary eElementLibrary, bool replace /*= true*/,
                             const string &strNameAttributeName /* = "Name"*/)
{
    // Init serializing context
    // eElementLibrary 值为 0
    // _pElementLibrarySet->getElementLibrary(eElementLibrary) 得到 
    // 在feedElementLibraries()中得到第一个ElementLibrary（pFrameworkConfigurationLibrary）
    // 这里注意 设置到elementSerializingContext中的ElementLibrary(用于解析baseUri文件)
    /*
    baseUri:/vendor/etc/parameter-framework/ParameterFrameworkConfigurationPolicy.xml 
    pRootElement->getXmlElementName() 为 "ParameterFrameworkConfiguration"
    pRootElement->getName() 为空
    strNameAttributeName 为 "Name"
    */
    elementSerializingContext.set(_pElementLibrarySet->getElementLibrary(eElementLibrary), baseUri);

    // pRootElement 为 CParameterFrameworkConfiguration
    // _bValidateSchemasOnStart 为 false
    // pRootElement->getXmlElementName() 内部调用getKind()返回"ParameterFrameworkConfiguration"
    // pRootElement->getName() 为空
    // strNameAttributeName 为 "Name"
    CXmlDocSource docSource(doc, _bValidateSchemasOnStart, pRootElement->getXmlElementName(),
                            pRootElement->getName(), strNameAttributeName);

    //getSchemaUri()返回 _schemaUri(std::string _schemaUri;),这里得到的为空字符
    docSource.setSchemaBaseUri(getSchemaUri());

    // Start clean
    auto clean = [replace, &pRootElement] {
        if (replace) {
            pRootElement->clean();
        }
    };
    // pRootElement 为 CParameterFrameworkConfiguration
    // 这里调用 CParameterFrameworkConfiguration.clean()
    clean();

    //CXmlMemoryDocSink memorySink(CParameterFrameworkConfiguration);
    CXmlMemoryDocSink memorySink(pRootElement);

    // docSource 为 CXmlDocSource 
    if (!memorySink.process(docSource, elementSerializingContext)) {
        clean();
        return false;
    }

    return true;
}

/**
* 加载 libpolicy-subsystem.so 动态库
*/
bool CParameterMgr::loadSubsystems(std::string &error)
{
    LOG_CONTEXT("Loading subsystem plugins");

    // getSystemClass() 得到 CSystemClass 类
    // _bFailOnMissingSubsystem 为 1
    // Load subsystems
    // 其中 loadSubsystems 中  CSystemClass 加载俩个子成员 
    // addElementBuilder("Virtual", new VirtualSubsystemBuilder(_logger));
    // addElementBuilder("Policy",  new TLoggingElementBuilderTemplate<PolicySubsystem>(logger));
    // _pSubsystemPlugins 参数 方便查找加载libpolicy-subsystem.so (PolicySubsystem)
    bool isSuccess =
        getSystemClass()->loadSubsystems(error, _pSubsystemPlugins, !_bFailOnMissingSubsystem);

    if (isSuccess) {
        info() << "All subsystem plugins successfully loaded";

        if (!error.empty()) {
            // Log missing subsystems as info
            info() << error;
        }
    } else {
        warning() << error;
    }
    return isSuccess;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/**
* 这里主要解析 /vendor/etc/parameter-framework/Structure/Policy/PolicyClass.xml 文件
* 并且将添加到 CSystemClass 中
*/
bool CParameterMgr::loadStructure(string &strError)
{
    // Retrieve system to load structure to
    CSystemClass *pSystemClass = getSystemClass();

    // pSystemClass->getName() 为 policy
    // 在 CParameterMgr::loadFrameworkConfiguration 中通过setName函数设置
    LOG_CONTEXT("Loading " + pSystemClass->getName() + " system class structure");

    // pStructureDescriptionFileLocation 得到在 CParameterMgr::loadFrameworkConfiguration 中创建的  
    // Get structure description element
    const CFrameworkConfigurationLocation *pStructureDescriptionFileLocation =
        static_cast<const CFrameworkConfigurationLocation *>(
            getConstFrameworkConfiguration()->findChildOfKind("StructureDescriptionFileLocation"));

    if (!pStructureDescriptionFileLocation) {
        strError = "No StructureDescriptionFileLocation element found for SystemClass " +
                   pSystemClass->getName();
        return false;
    }

    /**
    * 
    */
    // Parse Structure XML file
    CParameterAccessContext accessContext(strError);
    CXmlParameterSerializingContext parameterBuildContext(accessContext, strError);

    {
        // _xmlConfigurationUri 为 /vendor/etc/parameter-framework/ParameterFrameworkConfigurationPolicy.xml
        // getUri 得到 Structure/Policy/PolicyClass.xml
        // Get structure URI
        string structureUri = CXmlDocSource::mkUri(_xmlConfigurationUri, pStructureDescriptionFileLocation->getUri());

        LOG_CONTEXT("Importing system structure from file " + structureUri);

        // structureUri = /vendor/etc/parameter-framework/Structure/Policy/PolicyClass.xml
        _xmlDoc *doc = CXmlDocSource::mkXmlDoc(structureUri, true, true, parameterBuildContext);
        if (doc == nullptr) {
            return false;
        }

        // EParameterCreationLibrary 为 1
        // pSystemClass 为 CSystemClass
        // structureUri = /vendor/etc/parameter-framework/Structure/Policy/PolicyClass.xml
        if (!xmlParse(parameterBuildContext, pSystemClass, doc, structureUri, EParameterCreationLibrary)) {
            return false;
        }
    }

    // Initialize offsets
    // 作用 ？
    pSystemClass->setOffset(0);

    // Initialize main blackboard's size
    // 作用 ？
    _pMainParameterBlackboard->setSize(pSystemClass->getFootPrint());

    return true;
}

// pRootElement 为 SystemClass
// baseUri 为 /vendor/etc/parameter-framework/Structure/Policy/PolicyClass.xml
// eElementLibrary 为 1
// XML parsing
bool CParameterMgr::xmlParse(CXmlElementSerializingContext &elementSerializingContext,
                             CElement *pRootElement, _xmlDoc *doc, const string &baseUri,
                             CParameterMgr::ElementLibrary eElementLibrary, bool replace,
                             const string &strNameAttributeName)
{
    /*
    baseUri:/vendor/etc/parameter-framework/Structure/Policy/PolicyClass.xml 
    getXmlElementName:SystemClass 
    getName:Policy 
    strNameAttributeName:Name
    */
    //_pElementLibrarySet->getElementLibrary(eElementLibrary) 得到 feedElementLibraries 中的第二个成员 CElementLibrary
    // Init serializing context
    elementSerializingContext.set(_pElementLibrarySet->getElementLibrary(eElementLibrary), baseUri);

    // pRootElement 为 SystemClass
    // _bValidateSchemasOnStart 为 false
    // getXmlElementName 中会得到 CSystemClass::getKind() 值为 "SystemClass"
    // pRootElement->getName() 为 "Policy"
    // strNameAttributeName 为 "Name"
    CXmlDocSource docSource(doc, _bValidateSchemasOnStart, pRootElement->getXmlElementName(),
                            pRootElement->getName(), strNameAttributeName);

    // getSchemaUri() 得到为 null
    docSource.setSchemaBaseUri(getSchemaUri());

    // Start clean
    auto clean = [replace, &pRootElement] {
        if (replace) {
            pRootElement->clean();
        }
    };
    clean();
    
  
    // pRootElement 为 SystemClass
    CXmlMemoryDocSink memorySink(pRootElement);
    // /vendor/etc/parameter-framework/Structure/Policy/PolicyClass.xml
    if (!memorySink.process(docSource, elementSerializingContext)) {
        clean();
        return false;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
bool CParameterMgr::loadSettings(string &strError)
{
    string strLoadError;
    bool success = loadSettingsFromConfigFile(strLoadError);

    // _bFailOnFailedSettingsLoad 为 true
    if (!success && !_bFailOnFailedSettingsLoad) {
        // Load can not fail, ie continue but log the load errors
        warning() << strLoadError;
        warning() << "Failed to load settings, continue without domains.";
        success = true;
    }

    if (!success) {
        // Propagate the litteral error only if the function fails
        strError = strLoadError;
        return false;
    }

    return true;
}

bool CParameterMgr::loadSettingsFromConfigFile(string &strError)
{
    LOG_CONTEXT("Loading settings");

    /*
    <!--对应创建 CFrameworkConfigurationGroup -->
    <SettingsConfiguration>
        <!--对应创建 ConfigurableDomainsFileLocation -->
        <ConfigurableDomainsFileLocation Path="Settings/Policy/PolicyConfigurableDomains.xml"/>
    </SettingsConfiguration>
    */
    // Get settings configuration element
    //pFrameworkConfigurationLibrary->addElementBuilder("SettingsConfiguration", new TKindElementBuilderTemplate<CFrameworkConfigurationGroup>());
    const CFrameworkConfigurationGroup *pParameterConfigurationGroup =
        static_cast<const CFrameworkConfigurationGroup *>(
            getConstFrameworkConfiguration()->findChildOfKind("SettingsConfiguration"));

    if (!pParameterConfigurationGroup) {

        // No settings to load

        return true;
    }

    /*
        <!--对应创建 ConfigurableDomainsFileLocation -->
        <ConfigurableDomainsFileLocation Path="Settings/Policy/PolicyConfigurableDomains.xml"/>
    */
    // Get configurable domains element
    const CFrameworkConfigurationLocation *pConfigurableDomainsFileLocation =
        static_cast<const CFrameworkConfigurationLocation *>(
            pParameterConfigurationGroup->findChildOfKind("ConfigurableDomainsFileLocation"));

    if (!pConfigurableDomainsFileLocation) {
        strError = "No ConfigurableDomainsFileLocation element found for SystemClass " + getSystemClass()->getName();

        return false;
    }

    //这里 pConfigurableDomains 为 CConfigurableDomains 
    // Get destination root element
    CConfigurableDomains *pConfigurableDomains = getConfigurableDomains();


    // configurationDomainsUri 为 /vendor/etc/parameter-framework/Settings/Policy/PolicyConfigurableDomains.xml
    // Get Xml configuration domains URI
    string configurationDomainsUri =
        CXmlDocSource::mkUri(_xmlConfigurationUri, pConfigurableDomainsFileLocation->getUri());

    // Parse configuration domains XML file
    // getSystemClass() 得到 CSystemClass
    CXmlDomainImportContext xmlDomainImportContext(strError, true, *getSystemClass());

    // Selection criteria definition for rule creation
    xmlDomainImportContext.setSelectionCriteriaDefinition(
        getConstSelectionCriteria()->getSelectionCriteriaDefinition());

    // Auto validation of configurations
    xmlDomainImportContext.setAutoValidationRequired(true);

    info() << "Importing configurable domains from file " << configurationDomainsUri
           << " with settings";

    _xmlDoc *doc =
        CXmlDocSource::mkXmlDoc(configurationDomainsUri, true, true, xmlDomainImportContext);
    if (doc == nullptr) {
        return false;
    }

    return xmlParse(xmlDomainImportContext, pConfigurableDomains, doc, _xmlConfigurationUri,
                    EParameterConfigurationLibrary, true, "SystemClassName");
}


bool CParameterMgr::xmlParse(CXmlElementSerializingContext &elementSerializingContext,
                             CElement *pRootElement, _xmlDoc *doc, const string &baseUri,
                             CParameterMgr::ElementLibrary eElementLibrary, bool replace,
                             const string &strNameAttributeName)
{
    /*
    pRootElement 为 CConfigurableDomains
    baseUri:/vendor/etc/parameter-framework/ParameterFrameworkConfigurationPolicy.xml 
    pRootElement->getXmlElementName():ConfigurableDomains 
    pRootElement->getName():Policy 
    strNameAttributeName:SystemClassName
    */
    // Init serializing context
    elementSerializingContext.set(_pElementLibrarySet->getElementLibrary(eElementLibrary), baseUri);

    CXmlDocSource docSource(doc, _bValidateSchemasOnStart, pRootElement->getXmlElementName(),
                            pRootElement->getName(), strNameAttributeName);

    // getSchemaUri() 为 null
    docSource.setSchemaBaseUri(getSchemaUri());

    // Start clean
    auto clean = [replace, &pRootElement] {
        if (replace) {// true
            pRootElement->clean();
        }
    };
    clean();

    // pRootElement = CConfigurableDomains
    CXmlMemoryDocSink memorySink(pRootElement);

    if (!memorySink.process(docSource, elementSerializingContext)) {
        clean();
        return false;
    }
  
    return true;
}


// Init
bool CParameterMgr::init(string &strError)
{
    // CElement::init(strError);
    return base::init(strError);
}

bool CElement::init(string &strError)
{
    /*
    * 这里会调用
    */
    for (CElement *child : _childArray) {
        if (!child->init(strError)) {
            return false;
        }
    }

    return true;
}


// Remote Processor Server connection handling
bool CParameterMgr::handleRemoteProcessingInterface(string &strError)
{
    LOG_CONTEXT("Handling remote processing interface");

    // 由于 isRemoteInterfaceRequired() 返回 false，所以没有任何处理
    if (not isRemoteInterfaceRequired()) {
        return true;
    }
    。。。。。。
}




/////////////////////////////////////////////////////////////////////////////////

// Configuration application
void CParameterMgr::applyConfigurations()
{
    LOG_CONTEXT("Configuration application request");

    // Lock state
    lock_guard<mutex> autoLock(getBlackboardMutex());

    if (!_bTuningModeIsOn) {

        // Apply configuration(s)
        doApplyConfigurations(false);
    } else {

        warning() << "Configurations were not applied because the TuningMode is on";
    }
}

// Apply configurations
void CParameterMgr::doApplyConfigurations(bool bForce)
{
    LOG_CONTEXT("Applying configurations");

    CSyncerSet syncerSet;

    core::Results infos;
    // Check subsystems that need resync
    // 这里getSystemClass 得到 CSystemClass,
    // 在 CParameterMgr 构造函数中调用 addChild(new CSystemClass(_logger))
    // checkForSubsystemsToResync 内部条件直接返回，相当于没有做任何处理 
    getSystemClass()->checkForSubsystemsToResync(syncerSet, infos);

    // Ensure application of currently selected configurations
    // getConfigurableDomains 得到 CConfigurableDomains
    // 在 CParameterMgr 构造函数中调用 addChild(new CConfigurableDomains);
    // _pMainParameterBlackboard 为 CParameterBlackboard (构造函数中初始化)
    getConfigurableDomains()->apply(_pMainParameterBlackboard, syncerSet, bForce, infos);

    info() << infos;

    // Reset the modified status of the current criteria to indicate that a new configuration has
    // been applied
    getSelectionCriteria()->resetModifiedStatus();
}