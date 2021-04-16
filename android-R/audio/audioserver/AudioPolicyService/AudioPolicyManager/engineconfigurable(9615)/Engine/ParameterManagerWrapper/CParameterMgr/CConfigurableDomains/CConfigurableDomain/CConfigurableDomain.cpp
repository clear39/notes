
class CConfigurableDomain : public CElement{

};

/*
总共 32 个
<ConfigurableDomain Name="DeviceForInputSource.Calibration" SequenceAware="false">
*/
CConfigurableDomain::CConfigurableDomain(const string &strName) : base(strName)
{
}


bool CConfigurableDomain::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext)
{
    // Context
    CXmlDomainImportContext &xmlDomainImportContext = static_cast<CXmlDomainImportContext &>(serializingContext);

    // Sequence awareness (optional)
    // Sequence(序列)   Aware(意识到)
    // _bSequenceAware 均为 false
    xmlElement.getAttribute("SequenceAware", _bSequenceAware);

    std::string name;
    xmlElement.getAttribute("Name", name);
    setName(name);

    // Local parsing. Do not dig
    if (!parseDomainConfigurations(xmlElement, xmlDomainImportContext) ||
        !parseConfigurableElements(xmlElement, xmlDomainImportContext) ||
        !parseSettings(xmlElement, xmlDomainImportContext)) {
        return false;
    }

    // All provided configurations are parsed
    // Attempt validation on areas of non provided configurations for all configurable elements if
    // required
    // xmlDomainImportContext.autoValidationRequired() 为 true
    if (xmlDomainImportContext.autoValidationRequired()) {
        autoValidateAll();
    }

    return true;
}

/*
<Configurations>
	<Configuration Name="Selected">

		<CompoundRule Type="All">
		  	<SelectionCriterionRule SelectionCriterion="AvailableOutputDevices" MatchesWhen="Includes" Value="Speaker"/>
		</CompoundRule>

	</Configuration>

	<Configuration Name="NotSelected">
		<CompoundRule Type="All"/>
	</Configuration>

</Configurations>
*/
//这里解析的CDomainConfiguration(对应子标签Configuration)，作为CConfigurableDomain的子成员
// XML parsing
bool CConfigurableDomain::parseDomainConfigurations(const CXmlElement &xmlElement,CXmlDomainImportContext &serializingContext)
{
    // We're supposedly clean
    assert(_configurableElementList.empty());

    // Get Configurations element
    CXmlElement xmlConfigurationsElement;

    xmlElement.getChildElement("Configurations", xmlConfigurationsElement);

    // Parse it and create domain configuration objects
    // pParameterConfigurationLibrary->addElementBuilder("Configuration", new TNamedElementBuilderTemplate<CDomainConfiguration>());
    // CDomainConfiguration
    
    // CElement::fromXml
    return base::fromXml(xmlConfigurationsElement, serializingContext);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/*
<Configurations>
<ConfigurableElements>
  <ConfigurableElement Path="/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker"/>
</ConfigurableElements>
</Configurations>
*/
// 这里解析的 CConfigurableElement(对应子标签ConfigurableElement)，作为CConfigurableDomain的子成员
// Parse configurable elements
bool CConfigurableDomain::parseConfigurableElements(const CXmlElement &xmlElement, CXmlDomainImportContext &serializingContext)
{
    //
    CSystemClass &systemClass = serializingContext.getSystemClass();

    // Get ConfigurableElements element
    CXmlElement xmlConfigurableElementsElement;
    xmlElement.getChildElement("ConfigurableElements", xmlConfigurableElementsElement);

    // Parse it and associate found configurable elements to it
    CXmlElement::CChildIterator it(xmlConfigurableElementsElement);

    CXmlElement xmlConfigurableElementElement;

    while (it.next(xmlConfigurableElementElement)) {

        // Locate configurable element
        string strConfigurableElementPath;
        // 得到Path属性的值 /Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
        xmlConfigurableElementElement.getAttribute("Path", strConfigurableElementPath);

        // 构建CPathNavigator，构造函数内部 对/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
        // 通过 / 进行分割
        CPathNavigator pathNavigator(strConfigurableElementPath);
        string strError;

        // Is there an element and does it match system class name?
        // systemClass.getName() 为 Policy
        // navigateThrough 中 对systemClass.getName()和pathNavigator的一个字符串匹配（也就是Policy）
        if (!pathNavigator.navigateThrough(systemClass.getName(), strError)) {

            serializingContext.setError(
                "Could not find configurable element of path " + strConfigurableElementPath +
                " from ConfigurableDomain description " + getName() + " (" + strError + ")");

            return false;
        }

        // policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker
        // Browse system class for configurable element
        // 
        CConfigurableElement *pConfigurableElement =
            static_cast<CConfigurableElement *>(systemClass.findDescendant(pathNavigator));

        if (!pConfigurableElement) {
            serializingContext.setError("Could not find configurable element of path " +
                                        strConfigurableElementPath +
                                        " from ConfigurableDomain description " + getName());
            return false;
        }
        // Add found element to domain
        core::Results infos;
        //
        if (!addConfigurableElement(pConfigurableElement, nullptr, infos)) {
            strError = utility::asString(infos);
            serializingContext.setError(strError);

            return false;
        }
    }

    return true;
}

// Configurable elements association
bool CConfigurableDomain::addConfigurableElement(CConfigurableElement *pConfigurableElement,
                                                 const CParameterBlackboard *pMainBlackboard,
                                                 core::Results &infos)
{
    // Already associated?
    // 判断pConfigurableElement在 _configurableElementList 列表中是否存在
    if (containsConfigurableElement(pConfigurableElement)) {

        infos.push_back("Configurable element " + pConfigurableElement->getPath() +
                        " already associated to configuration domain " + getName());

        return false;
    }

    // Already owned?
    if (pConfigurableElement->belongsTo(this)) {

        infos.push_back("Configurable element " + pConfigurableElement->getPath() +
                        " already owned by configuration domain " + getName());

        return false;
    }

    // Do add
    doAddConfigurableElement(pConfigurableElement, infos, pMainBlackboard);

    return true;
}

// Configurable elements association
void CConfigurableDomain::doAddConfigurableElement(CConfigurableElement *pConfigurableElement,
                                                   core::Results &infos,
                                                   const CParameterBlackboard *pMainBlackboard)
{
    // Inform configurable element
    // addAttachedConfigurableDomain该方法只用在CConfigurableElement实现
    // 将CConfigurableDomain 添加到其列表中
    pConfigurableElement->addAttachedConfigurableDomain(this);

    // Create associated syncer set
    auto pSyncerSet = new CSyncerSet;

    // Add to sync set the configurable element one
    // fillSyncerSet 该方法只用在CConfigurableElement实现
    pConfigurableElement->fillSyncerSet(*pSyncerSet);

    // Store it
    _configurableElementToSyncerSetMap[pConfigurableElement] = pSyncerSet;

    // Add it to global one
    _syncerSet += *pSyncerSet;

    // Inform configurations
    size_t uiNbConfigurations = getNbChildren();

    for (size_t uiChild = 0; uiChild < uiNbConfigurations; uiChild++) {

        CDomainConfiguration *pDomainConfiguration =
            static_cast<CDomainConfiguration *>(getChild(uiChild));

        pDomainConfiguration->addConfigurableElement(pConfigurableElement, pSyncerSet);
    }

    // Ensure area validity for that configurable element (if main blackboard provided)
    if (pMainBlackboard) {

        infos.push_back("Validating domain '" + getName() +
                        "' against main blackboard for configurable element '" +
                        pConfigurableElement->getPath() + "'");
        // Need to validate against main blackboard
        validateAreas(pConfigurableElement, pMainBlackboard);
    }

    // Already associated descendend configurable elements need a merge of their configuration data
    mergeAlreadyAssociatedDescendantConfigurableElements(pConfigurableElement, infos);

    // Add to list
    _configurableElementList.push_back(pConfigurableElement);
}



/*
<Configurations>
<Settings>

  <Configuration Name="Selected">
    <ConfigurableElement Path="/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker">
      <BitParameter Name="speaker">1</BitParameter>
    </ConfigurableElement>
  </Configuration>

  <Configuration Name="NotSelected">
    <ConfigurableElement Path="/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker">
      <BitParameter Name="speaker">0</BitParameter>
    </ConfigurableElement>
  </Configuration>

</Settings>
</Configurations>
*/
// Parse settings
bool CConfigurableDomain::parseSettings(const CXmlElement &xmlElement,
                                        CXmlDomainImportContext &serializingContext)
{
    // Check we actually need to parse configuration settings
    if (!serializingContext.withSettings()) {

        // No parsing required
        return true;
    }

    // Get Settings element
    CXmlElement xmlSettingsElement;
    if (!xmlElement.getChildElement("Settings", xmlSettingsElement)) {

        // No settings, bail out successfully
        return true;
    }

    // Parse configuration settings
    CXmlElement::CChildIterator it(xmlSettingsElement);

    CXmlElement xmlConfigurationSettingsElement;

    while (it.next(xmlConfigurationSettingsElement)) {
    	// 去当前列表中查找对应的 CDomainConfiguration
        // Get domain configuration
        CDomainConfiguration *pDomainConfiguration = static_cast<CDomainConfiguration *>( findChild(xmlConfigurationSettingsElement.getNameAttribute()));

        if (!pDomainConfiguration) {

            serializingContext.setError("Could not find domain configuration referred to by"
                                        " configurable domain \"" +
                                        getName() + "\".");

            return false;
        }

        // Have domain configuration parse settings for all configurable elements
        // pDomainConfiguration = CDomainConfiguration
        if (!pDomainConfiguration->parseSettings(xmlConfigurationSettingsElement,serializingContext)) {

            return false;
        }
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// Configuration application if required
void CConfigurableDomain::apply(CParameterBlackboard *pParameterBlackboard, CSyncerSet *pSyncerSet,
                                bool bForce, std::string &strInfo) const
{
    // Apply configuration only if the blackboard will
    // be synchronized either now or by syncerSet.
    if (!pSyncerSet ^ _bSequenceAware) {  //_bSequenceAware 均为 false
        // The configuration can not be syncronised
        return;
    }

    if (bForce) {  // false
        // Force a configuration restore by forgetting about last applied configuration
        _pLastAppliedConfiguration = nullptr;
    }
    const CDomainConfiguration *pApplicableDomainConfiguration =
        findApplicableDomainConfiguration();

    if (pApplicableDomainConfiguration) {

        // Check not the last one before applying
        if (!_pLastAppliedConfiguration ||
            _pLastAppliedConfiguration != pApplicableDomainConfiguration) {

            strInfo = "Applying configuration '" + pApplicableDomainConfiguration->getName() +
                      "' from domain '" + getName() + "'";

            // Check if we need to synchronize during restore
            bool bSync = !pSyncerSet && _bSequenceAware;  // false

            //进行值的写入
            // Do the restore
            pApplicableDomainConfiguration->restore(pParameterBlackboard, bSync, nullptr);

            // Record last applied configuration
            _pLastAppliedConfiguration = pApplicableDomainConfiguration;

            // Check we need to provide syncer set to caller
            if (pSyncerSet && !_bSequenceAware) {

                // Since we applied changes, add our own sync set to the given one
                *pSyncerSet += _syncerSet;
            }
        }
    }
}


// Search for an applicable configuration
const CDomainConfiguration *CConfigurableDomain::findApplicableDomainConfiguration() const
{
    size_t uiNbConfigurations = getNbChildren();
    
    for (size_t uiChild = 0; uiChild < uiNbConfigurations; uiChild++) {

        const CDomainConfiguration *pDomainConfiguration =
            static_cast<const CDomainConfiguration *>(getChild(uiChild));

        // 调用  
        if (pDomainConfiguration->isApplicable()) {
            return pDomainConfiguration;
        }
    }
    return nullptr;

}