
/*
<criterion_types>
    <criterion_type name="OutputDevicesMaskType" type="inclusive"/>
    <criterion_type name="InputDevicesMaskType" type="inclusive"/>
    <criterion_type name="OutputDevicesAddressesType" type="inclusive">
        <values>
            <!-- legacy remote submix -->
            <value literal="0" numerical="1"/>
        </values>
    </criterion_type>
    <criterion_type name="InputDevicesAddressesType" type="inclusive">
        <values>
            <!-- legacy remote submix -->
            <value literal="0" numerical="1"/>
        </values>
    </criterion_type>
    。。。。。。
</criterion_types>
*/

using ValuePair = std::pair<uint32_t, std::string>;

using ValuePairs = std::vector<ValuePair>;


struct CriterionType
{
    std::string name;
    bool isInclusive;
    ValuePairs valuePairs;
};

using CriterionTypes = std::vector<CriterionType>;


deserializeCollection<CriterionTypeTraits>(doc, cur, config->criterionTypes, nbSkippedElements);

template <class Trait>  // CriterionTypeTraits
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection, // CriterionTypeTraits::CriterionTypes
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    	//CriterionTypeTraits::collectionTag = "criterion_types"
    	//CriterionTypeTraits::tag = "criterion_type"
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        //CriterionTypeTraits::collectionTag = "criterion_types"
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
        	//CriterionTypeTraits::tag = "criterion_type"
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
            	// CriterionTypeTraits::deserialize
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




status_t CriterionTypeTraits::deserialize(_xmlDoc *doc, const _xmlNode *child,
                       Collection &criterionTypes)  // CriterionTypeTraits::CriterionTypes
{
	// Attributes::name = "name"
    std::string name = getXmlAttribute(child, Attributes::name);
    if (name.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::name);
        return BAD_VALUE;
    }
    ALOGV("%s: %s %s = %s", __FUNCTION__, tag, Attributes::name, name.c_str());

    // Attributes::type = "type"
    std::string type = getXmlAttribute(child, Attributes::type);
    if (type.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::type);
        return BAD_VALUE;
    }
    ALOGV("%s: %s %s = %s", __FUNCTION__, tag, Attributes::type, type.c_str());
    bool isInclusive(type == "inclusive");

    //using ValuePair = std::pair<uint32_t, std::string>;
    //using ValuePairs = std::vector<ValuePair>;
    ValuePairs pairs;
    size_t nbSkippedElements = 0;

    //pairs为 ValueTraits::ValuePairs
    deserializeCollection<ValueTraits>(doc, child, pairs, nbSkippedElements);
    criterionTypes.push_back({name, isInclusive, pairs});
    return NO_ERROR;
}




template <class Trait>  // ValueTraits
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection, // ValueTraits::ValuePairs
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    	//ValueTraits::collectionTag = "values"
    	//ValueTraits::tag = "value"
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        //ValueTraits::collectionTag = "values"
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
        	//ValueTraits::tag = "value"
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
            	// ValueTraits::deserialize
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

// Collection = ValueTraits::ValuePairs
status_t ValueTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *child, Collection &values)
{
	//Attributes::literal = "literal";
    std::string literal = getXmlAttribute(child, Attributes::literal);
    if (literal.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::literal);
        return BAD_VALUE;
    }
    uint32_t numerical = 0;
    //Attributes::numerical = "numerical";
    std::string numericalTag = getXmlAttribute(child, Attributes::numerical);
    if (numericalTag.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::literal);
        return BAD_VALUE;
    }
    //
    if (!convertTo(numericalTag, numerical)) {
        ALOGE("%s: : Invalid value(%s)", __FUNCTION__, numericalTag.c_str());
        return BAD_VALUE;
    }
    values.push_back({numerical, literal});
    return NO_ERROR;
}