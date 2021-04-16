class CDomainConfiguration : public CElement {

};

/*
<Configuration Name="Selected">
	<CompoundRule Type="All">
	  	<SelectionCriterionRule SelectionCriterion="AvailableOutputDevices" MatchesWhen="Includes" Value="Speaker"/>
	</CompoundRule>

</Configuration>
*/

CDomainConfiguration::CDomainConfiguration(const string &strName) : base(strName)
{
}

// From IXmlSink
bool CElement::fromXml(const CXmlElement &xmlElement, CXmlSerializingContext &serializingContext) try {
    // CDomainConfiguration 没有该属性
    xmlElement.getAttribute(gDescriptionPropertyName, _strDescription);

    // Propagate through children
    CXmlElement::CChildIterator childIterator(xmlElement);

    CXmlElement childElement;

    while (childIterator.next(childElement)) {

        CElement *pChild;

        serializingContext.appendToError("CElement::fromXml childElement:" + childElement.getType() + "\n");

        //true
        if (!childrenAreDynamic()) {

            pChild = findChildOfKind(childElement.getType());

            if (!pChild) {

                serializingContext.setError("Unable to handle XML element: " +
                                            childElement.getPath());

                return false;
            }

        } else {
            // Child needs creation
            // pParameterConfigurationLibrary->addElementBuilder("CompoundRule",new TElementBuilderTemplate<CCompoundRule>());
    		// pChild = CCompoundRule 
            pChild = createChild(childElement, serializingContext);
            if (!pChild) {
                return false;
            }
        }

        // Dig
        // pChild = CCompoundRule 
        if (!pChild->fromXml(childElement, serializingContext)) {

            return false;
        }
    }

    return true;
} catch (const PfError &e) {
    serializingContext.appendLineToError(e.what());
    return false;
}


void CDomainConfiguration::addConfigurableElement(const CConfigurableElement *configurableElement, const CSyncerSet *syncerSet)
{
    //
    mAreaConfigurationList.emplace_back(configurableElement->createAreaConfiguration(syncerSet));
}




/*
<ConfigurableElement Path="/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker">
  <BitParameter Name="speaker">0</BitParameter>
</ConfigurableElement>
*/
// XML configuration settings parsing
bool CDomainConfiguration::parseSettings(CXmlElement &xmlConfigurationSettingsElement, CXmlDomainImportContext &context)
{
    // Parse configurable element's configuration settings
    CXmlElement::CChildIterator it(xmlConfigurationSettingsElement);

    CXmlElement xmlConfigurableElementSettingsElement;
    auto insertLocation = begin(mAreaConfigurationList);

    while (it.next(xmlConfigurableElementSettingsElement)) {

        // Retrieve area configuration
        string configurableElementPath;
        xmlConfigurableElementSettingsElement.getAttribute("Path", configurableElementPath);

        // Path="/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker"
        auto areaConfiguration = findAreaConfigurationByPath(configurableElementPath);
        if (areaConfiguration == end(mAreaConfigurationList)) {

            context.setError("Configurable Element " + configurableElementPath +
                             " referred to by Configuration " + getPath() +
                             " not associated to Domain");

            return false;
        }
        // Parse
        if (!importOneConfigurableElementSettings(areaConfiguration->get(),xmlConfigurableElementSettingsElement, context)) {

            return false;
        }
        // Take into account the new configuration order by moving the configuration associated to
        // the element to the n-th position of the configuration list.
        // It will result in prepending to the configuration list wit the configuration of all
        // elements found in XML, keeping the order of the processing of the XML file.
        mAreaConfigurationList.splice(insertLocation, mAreaConfigurationList, areaConfiguration);
        // areaConfiguration is still valid, but now refer to the reorderer list
        insertLocation = std::next(areaConfiguration);
    }
    return true;
}

// "/Policy/policy/product_strategies/transmitted_through_speaker/selected_output_devices/mask/speaker"
CDomainConfiguration::AreaConfigurations::iterator CDomainConfiguration::
    findAreaConfigurationByPath(const std::string &configurableElementPath)
{
    // 
    auto areaConfiguration =
        find_if(begin(mAreaConfigurationList), end(mAreaConfigurationList),
                [&](const AreaConfiguration &conf) {
                    return conf->getConfigurableElement()->getPath() == configurableElementPath;
                });
    return areaConfiguration;
}


// Serialize one configuration for one configurable element
bool CDomainConfiguration::importOneConfigurableElementSettings(
    CAreaConfiguration *areaConfiguration, CXmlElement &xmlConfigurableElementSettingsElement,
    CXmlDomainImportContext &context)
{
    const CConfigurableElement *destination = areaConfiguration->getConfigurableElement();

    // Check structure
    if (xmlConfigurableElementSettingsElement.getNbChildElements() != 1) {

        // Structure error
        context.setError("Struture error encountered while parsing settings of " +
                         destination->getKind() + " " + destination->getName() +
                         " in Configuration " + getPath());

        return false;
    }

    // Element content
    CXmlElement xmlConfigurableElementSettingsElementContent;
    // Check name and kind
    if (!xmlConfigurableElementSettingsElement.getChildElement(
            destination->getXmlElementName(), destination->getName(),
            xmlConfigurableElementSettingsElementContent)) {

        // "Component" tag has been renamed to "ParameterBlock", but retro-compatibility shall
        // be ensured.
        //
        // So checking if this case occurs, i.e. element name is "ParameterBlock"
        // but found xml setting name is "Component".
        bool compatibilityCase =
            (destination->getXmlElementName() == "ParameterBlock") &&
            xmlConfigurableElementSettingsElement.getChildElement(
                "Component", destination->getName(), xmlConfigurableElementSettingsElementContent);

        // Error if the compatibility case does not occur.
        if (!compatibilityCase) {
            context.setError("Couldn't find settings for " + destination->getXmlElementName() +
                             " " + destination->getName() + " for Configuration " + getPath());

            return false;
        }
    }

    // Create configuration access context
    string error;
    CConfigurationAccessContext configurationAccessContext(error, false);

    // Have domain configuration parse settings for configurable element
    bool success = areaConfiguration->serializeXmlSettings(xmlConfigurableElementSettingsElementContent, configurationAccessContext);

    context.appendToError(error);
    return success;
}




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Dynamic data application
bool CDomainConfiguration::isApplicable() const
{
    const CCompoundRule *pRule = getRule();

    return pRule && pRule->matches();
}

enum ChildElementType
{
    ECompoundRule
};

// Rule
const CCompoundRule *CDomainConfiguration::getRule() const
{
    if (getNbChildren()) {
        // Rule created
        // 也就是这里只有一个
        return static_cast<const CCompoundRule *>(getChild(ECompoundRule));
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Apply data to current
bool CDomainConfiguration::restore(CParameterBlackboard *pMainBlackboard, bool bSync,
                                   core::Results *errors) const
{
    // using AreaConfiguration = std::unique_ptr<CAreaConfiguration>;
    // using AreaConfigurations = std::list<AreaConfiguration>;
    // AreaConfigurations mAreaConfigurationList;
    return std::accumulate(begin(mAreaConfigurationList), end(mAreaConfigurationList), true,
                           [&](bool accumulator, const AreaConfiguration &conf) {
                               return conf->restore(pMainBlackboard, bSync, errors) && accumulator;
                           });
}