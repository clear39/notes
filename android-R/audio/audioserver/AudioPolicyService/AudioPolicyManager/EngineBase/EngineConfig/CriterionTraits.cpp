
/*
<criteria>
    <criterion name="AvailableInputDevices" type="InputDevicesMaskType" default="none"/>
    <criterion name="AvailableOutputDevices" type="OutputDevicesMaskType" default="none"/>
    <criterion name="AvailableOutputDevicesAddresses" type="OutputDevicesAddressesType" default="none"/>
    <criterion name="AvailableInputDevicesAddresses" type="InputDevicesAddressesType" default="none"/>
    <criterion name="TelephonyMode" type="AndroidModeType" default="Normal"/>
    <criterion name="ForceUseForCommunication" type="ForceUseForCommunicationType" default="ForceNone"/>
    <criterion name="ForceUseForMedia" type="ForceUseForMediaType" default="ForceNone"/>
    <criterion name="ForceUseForRecord" type="ForceUseForRecordType" default="ForceNone"/>
    <criterion name="ForceUseForDock" type="ForceUseForDockType" default="ForceNone"/>
    <criterion name="ForceUseForSystem" type="ForceUseForSystemType" default="ForceNone"/>
    <criterion name="ForceUseForHdmiSystemAudio" type="ForceUseForHdmiSystemAudioType" default="ForceNone"/>
    <criterion name="ForceUseForEncodedSurround" type="ForceUseForEncodedSurroundType" default="ForceNone"/>
    <criterion name="ForceUseForVibrateRinging" type="ForceUseForVibrateRingingType" default="ForceNone"/>
</criteria>
*/
using Criteria = std::vector<Criterion>;

struct Criterion
{
    std::string name;
    std::string typeName;
    std::string defaultLiteralValue;
};

// config->criteria 的类型为 Criteria
deserializeCollection<CriterionTraits>(doc, cur, config->criteria, nbSkippedElements);

template <class Trait> // Trait = CriterionTraits
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection,  // CriterionTraits::Criteria
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    	// CriterionTraits::collectionTag = "criteria"
    	// CriterionTraits::tag = "criterion"
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        // CriterionTraits::collectionTag = "criteria"
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
        	//转换成子节点
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
        	// CriterionTraits::tag = "criterion"
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
            	// CriterionTraits::deserialize
                status_t status = Trait::deserialize(doc, child, collection);
                if (status != NO_ERROR) {
                    nbSkippedElement += 1;
                }
            }
        }
        if (!xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
    }
    return NO_ERROR;
}





status_t CriterionTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *child,
                                      Collection &criteria)  // CriterionTraits::Criteria
{
	// Attributes::name = "name"
    std::string name = getXmlAttribute(child, Attributes::name);// "name"
    if (name.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::name);
        return BAD_VALUE;
    }
    ALOGV("%s: %s = %s", __FUNCTION__, Attributes::name, name.c_str());

    //Attributes::defaultVal = "default"
    std::string defaultValue = getXmlAttribute(child, Attributes::defaultVal); //
    if (defaultValue.empty()) {
        // Not mandatory to provide a default value for a criterion, even it is recommanded...
        ALOGV("%s: No attribute %s found (but recommanded)", __FUNCTION__, Attributes::defaultVal);
    }
    ALOGV("%s: %s = %s", __FUNCTION__, Attributes::defaultVal, defaultValue.c_str());

    // Attributes::type = "type"
    std::string typeName = getXmlAttribute(child, Attributes::type);
    if (typeName.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::name);
        return BAD_VALUE;
    }
    ALOGV("%s: %s = %s", __FUNCTION__, Attributes::type, typeName.c_str());

    criteria.push_back({name, typeName, defaultValue});
    return NO_ERROR;
}