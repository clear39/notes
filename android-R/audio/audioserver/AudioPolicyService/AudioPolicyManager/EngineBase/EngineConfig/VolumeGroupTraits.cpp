
/*
<volumeGroups>
 	<volumeGroup>
        <name>voice_call</name>
        <indexMin>1</indexMin>
        <indexMax>7</indexMax>
        <volume deviceCategory="DEVICE_CATEGORY_HEADSET">
            <point>0,-4200</point>
            <point>33,-2800</point>
            <point>66,-1400</point>
            <point>100,0</point>
        </volume>
        。。。。。。
    </volumeGroup>

    <volumeGroup>
        <name>music</name>
        <indexMin>0</indexMin>
        <indexMax>25</indexMax>
        <volume deviceCategory="DEVICE_CATEGORY_HEADSET" ref="DEFAULT_MEDIA_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_SPEAKER" ref="DEFAULT_DEVICE_CATEGORY_SPEAKER_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_EARPIECE" ref="DEFAULT_MEDIA_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_EXT_MEDIA" ref="DEFAULT_MEDIA_VOLUME_CURVE"/>
        <volume deviceCategory="DEVICE_CATEGORY_HEARING_AID"  ref="DEFAULT_HEARING_AID_VOLUME_CURVE"/>
    </volumeGroup>

    ......

</volumeGroups>



    
<volumes>
    ......
    <reference name="DEFAULT_MEDIA_VOLUME_CURVE">
    <!-- Default Media reference Volume Curve -->
        <point>1,-5800</point>
        <point>20,-4000</point>
        <point>60,-1700</point>
        <point>100,0</point>
    </reference>
</volumes>
*/

using VolumeGroups = std::vector<VolumeGroup>;

struct VolumeGroup {
    std::string name;
    int indexMin;
    int indexMax;
    VolumeCurves volumeCurves;
};

using VolumeCurves = std::vector<VolumeCurve>;

struct VolumeCurve {
    std::string deviceCategory;
    CurvePoints curvePoints;
};

using CurvePoints = std::vector<CurvePoint>;

struct CurvePoint {
    int index;
    int attenuationInMb;
};

// config->volumeGroups 为 using VolumeGroups = std::vector<VolumeGroup>;
deserializeCollection<VolumeGroupTraits>(doc, cur, config->volumeGroups, nbSkippedElements);



 template <class Trait>  // VolumeGroupTraits 
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection, // VolumeGroupTraits::VolumeGroups
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    	// VolumeGroupTraits::collectionTag = "volumeGroups";
    	// VolumeGroupTraits::tag = "volumeGroup";
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        // VolumeGroupTraits::collectionTag = "volumeGroups";
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
        	// VolumeGroupTraits::tag = "volumeGroup";
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
            	// VolumeGroupTraits::deserialize
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


// VolumeGroupTraits::VolumeGroups
status_t VolumeGroupTraits::deserialize(_xmlDoc *doc, const _xmlNode *root, Collection &volumes)
{
    std::string name;
    int indexMin = 0;
    int indexMax = 0;
    // using StreamVector = std::vector<audio_stream_type_t>;
    StreamVector streams = {};
    // using AttributesVector = std::vector<audio_attributes_t>;
    AttributesVector attributesVect = {};

    for (const xmlNode *child = root->xmlChildrenNode; child != NULL; child = child->next) {

    	// Attributes::name = "name"
        if (not xmlStrcmp(child->name, (const xmlChar *)Attributes::name)) { // <name>music</name>
            xmlCharUnique nameXml(xmlNodeListGetString(doc, child->xmlChildrenNode, 1), xmlFree);
            if (nameXml == nullptr) {
                return BAD_VALUE;
            }
            name = reinterpret_cast<const char*>(nameXml.get());
        }
        // Attributes::indexMin = "indexMin"
        if (not xmlStrcmp(child->name, (const xmlChar *)Attributes::indexMin)) {// <indexMin>0</indexMin>
            xmlCharUnique indexMinXml(xmlNodeListGetString(doc, child->xmlChildrenNode, 1), xmlFree);
            if (indexMinXml == nullptr) {
                return BAD_VALUE;
            }
            std::string indexMinLiteral(reinterpret_cast<const char*>(indexMinXml.get()));
            if (!convertTo(indexMinLiteral, indexMin)) {
                return BAD_VALUE;
            }
        }
        // Attributes::indexMax = "indexMax"
        if (not xmlStrcmp(child->name, (const xmlChar *)Attributes::indexMax)) {// <indexMax>25</indexMax>
            xmlCharUnique indexMaxXml(xmlNodeListGetString(doc, child->xmlChildrenNode, 1), xmlFree);
            if (indexMaxXml == nullptr) {
                return BAD_VALUE;
            }
            std::string indexMaxLiteral(reinterpret_cast<const char*>(indexMaxXml.get()));
            if (!convertTo(indexMaxLiteral, indexMax)) {
                return BAD_VALUE;
            }
        }
    }
    //注意这里是root节点
    //从xml配置文件得知，该函数的属性不存在，相当于不执行，这里可以跳过
    deserializeAttributesCollection(doc, root, attributesVect);

    std::string streamNames;
    for (const auto &stream : streams) {
        streamNames += android::toString(stream) + " ";
    }
    std::string attrmNames;
    for (const auto &attr : attributesVect) {
        attrmNames += android::toString(attr) + "\n";
    }
    ALOGV("%s: group=%s indexMin=%d, indexMax=%d streams=%s attributes=%s",
          __func__, name.c_str(), indexMin, indexMax, streamNames.c_str(), attrmNames.c_str( ));

    VolumeCurves groupVolumeCurves;
    size_t skipped = 0;
    // 
    deserializeCollection<VolumeTraits>(doc, root, groupVolumeCurves, skipped);
    volumes.push_back({ name, indexMin, indexMax, groupVolumeCurves });
    return NO_ERROR;
}

//
static status_t deserializeAttributesCollection(_xmlDoc *doc, const _xmlNode *cur, AttributesVector &collection)
{
    status_t ret = BAD_VALUE;
    // Either we do provide only one attributes or a collection of supported attributes
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
        if (not xmlStrcmp(cur->name, (const xmlChar *)("Attributes")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("ContentType")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("Usage")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("Flags")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("Bundle"))) {
            audio_attributes_t attributes = AUDIO_ATTRIBUTES_INITIALIZER;
        	//
            ret = deserializeAttributes(doc, cur, attributes);
            if (ret == NO_ERROR) {
                collection.push_back(attributes);
                // We are done if the "Attributes" balise is omitted, only one Attributes is allowed
                if (xmlStrcmp(cur->name, (const xmlChar *)("Attributes"))) {
                    return ret;
                }
            }
        }
    }
    return ret;
}

static status_t deserializeAttributes(_xmlDoc *doc, const _xmlNode *cur,audio_attributes_t &attributes) {
    // Retrieve content type, usage, flags, and bundle from xml
    for (; cur != NULL; cur = cur->next) {
        if (not xmlStrcmp(cur->name, (const xmlChar *)("Attributes"))) {
            const xmlNode *attrNode = cur;
            std::string attrRef = getXmlAttribute(cur, attributesAttributeRef);
            if (!attrRef.empty()) {
                getReference(xmlDocGetRootElement(doc), attrNode, attrRef, attributesAttributeRef);
                if (attrNode == NULL) {
                    ALOGE("%s: No reference found for %s", __FUNCTION__, attrRef.c_str());
                    return BAD_VALUE;
                }
                return deserializeAttributes(doc, attrNode->xmlChildrenNode, attributes);
            }
            return parseAttributes(attrNode->xmlChildrenNode, attributes);
        }
        if (not xmlStrcmp(cur->name, (const xmlChar *)("ContentType")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("Usage")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("Flags")) ||
                not xmlStrcmp(cur->name, (const xmlChar *)("Bundle"))) {
            return parseAttributes(cur, attributes);
        }
    }
    return BAD_VALUE;
}


template <class Trait> // VolumeTraits
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection,  // VolumeTraits::VolumeCurves
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    	//Trait::collectionTag = "volumes";
    	//Trait::tag = "volume";
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        //Trait::collectionTag = "volumes";
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
        	//Trait::tag = "volume";
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
            	// VolumeTraits::deserialize
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

// VolumeTraits::Collection 为 VolumeCurves
status_t VolumeTraits::deserialize(_xmlDoc *doc, const _xmlNode *root, Collection &volumes)
{
    std::string deviceCategory = getXmlAttribute(root, Attributes::deviceCategory);
    if (deviceCategory.empty()) {
        ALOGW("%s: No %s found", __FUNCTION__, Attributes::deviceCategory);
    }
    std::string referenceName = getXmlAttribute(root, Attributes::reference);
    const _xmlNode *ref = NULL;
    if (!referenceName.empty()) {
        getReference(xmlDocGetRootElement(doc), ref, referenceName, collectionTag);
        if (ref == NULL) {
            ALOGE("%s: No reference Ptr found for %s", __FUNCTION__, referenceName.c_str());
            return BAD_VALUE;
        }
    }
    // Retrieve curve point from reference element if found or directly from current curve
    // using CurvePoints = std::vector<CurvePoint>;
    CurvePoints curvePoints;
    for (const xmlNode *child = referenceName.empty() ?
         root->xmlChildrenNode : ref->xmlChildrenNode; child != NULL; child = child->next) {
        if (!xmlStrcmp(child->name, (const xmlChar *)volumePointTag)) {
            xmlCharUnique pointXml(xmlNodeListGetString(doc, child->xmlChildrenNode, 1), xmlFree);
            if (pointXml == NULL) {
                return BAD_VALUE;
            }
            ALOGV("%s: %s=%s", __func__, tag, reinterpret_cast<const char*>(pointXml.get()));
            std::vector<int> point;
            collectionFromString<DefaultTraits<int>>(
                        reinterpret_cast<const char*>(pointXml.get()), point, ",");
            if (point.size() != 2) {
                ALOGE("%s: Invalid %s: %s", __func__, volumePointTag,
                      reinterpret_cast<const char*>(pointXml.get()));
                return BAD_VALUE;
            }
            curvePoints.push_back({point[0], point[1]});
        }
    }
    volumes.push_back({ deviceCategory, curvePoints });
    return NO_ERROR;
}