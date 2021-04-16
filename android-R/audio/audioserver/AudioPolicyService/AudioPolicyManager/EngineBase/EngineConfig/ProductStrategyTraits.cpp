
/*
<ProductStrategys>
.......
    <!--所有ProductStrategy存储到ProductStrategies中-->
    <ProductStrategy name="STRATEGY_MEDIA">
        
        <!--所有AttributesGroup存储到AttributesGroups中-->
        <AttributesGroup streamType="AUDIO_STREAM_MUSIC" volumeGroup="music">
            <!--所有Attributes存储到AttributesVector中-->
            <!--每一个Attributes对应一个audio_attributes_t-->
            <Attributes> <Usage value="AUDIO_USAGE_MEDIA"/> </Attributes> 
            <Attributes> <Usage value="AUDIO_USAGE_GAME"/> </Attributes>
            <Attributes> <Usage value="AUDIO_USAGE_ASSISTANT"/> </Attributes>
            <Attributes> <Usage value="AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE"/> </Attributes>
            <Attributes></Attributes>
        </AttributesGroup>

        <AttributesGroup streamType="AUDIO_STREAM_ASSISTANT" volumeGroup="assistant">
            <Attributes>
                <ContentType value="AUDIO_CONTENT_TYPE_SPEECH"/>
                <Usage value="AUDIO_USAGE_ASSISTANT"/>
            </Attributes>
        </AttributesGroup>

        <AttributesGroup streamType="AUDIO_STREAM_SYSTEM" volumeGroup="system">
            <Attributes> <Usage value="AUDIO_USAGE_ASSISTANCE_SONIFICATION"/> </Attributes>
        </AttributesGroup>

    </ProductStrategy>
......
</ProductStrategys>
*/

//
using ProductStrategies = std::vector<ProductStrategy>;

struct ProductStrategy {
    std::string name;  //  <ProductStrategy name="STRATEGY_MEDIA">
    AttributesGroups attributesGroups;
};

using AttributesGroups = std::vector<AttributesGroup>;

struct AttributesGroup {
    std::string name;
    audio_stream_type_t stream;
    std::string volumeGroup;
    AttributesVector attributesVect;
};


using AttributesVector = std::vector<audio_attributes_t>;


// system/media/audio/include/system/audio.h
typedef struct {
    audio_content_type_t content_type;
    audio_usage_t        usage;
    audio_source_t       source;
    audio_flags_mask_t   flags;    // typedef uint32_t audio_flags_mask_t;
    char                 tags[AUDIO_ATTRIBUTES_TAGS_MAX_SIZE]; /* UTF8 */
} __attribute__((packed)) audio_attributes_t; // sent through Binder;                                                                                                                                  

static const audio_attributes_t AUDIO_ATTRIBUTES_INITIALIZER = {
    /* .content_type = */ AUDIO_CONTENT_TYPE_UNKNOWN,
    /* .usage = */ AUDIO_USAGE_UNKNOWN,
    /* .source = */ AUDIO_SOURCE_DEFAULT,
    /* .flags = */ AUDIO_FLAG_NONE,
    /* .tags = */ ""
};



// config->productStrategies 为 ProductStrategies
deserializeCollection<ProductStrategyTraits>(doc, cur, config->productStrategies, nbSkippedElements);

template <class Trait> // ProductStrategyTraits
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection,
                                      size_t &nbSkippedElement)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
        //  Trait::collectionTag = ProductStrategyTraits::collectionTag  //collectionTag = "ProductStrategies";
        //  Trait::tag = ProductStrategyTraits::tag  //  tag = "ProductStrategy";
        if (xmlStrcmp(cur->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(cur->name, (const xmlChar *)Trait::tag)) {
            continue;
        }
        const xmlNode *child = cur;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        for (; child != NULL; child = child->next) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
                // ProductStrategyTraits::deserialize
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



// Collection 为 ProductStrategies
status_t ProductStrategyTraits::deserialize(_xmlDoc *doc, const _xmlNode *child, Collection &strategies)
{
    // Attributes::name = "name"
    std::string name = getXmlAttribute(child, Attributes::name); // "STRATEGY_MEDIA"
    if (name.empty()) {
        ALOGE("ProductStrategyTraits No attribute %s found", Attributes::name);
        return BAD_VALUE;
    }
    ALOGV("%s: %s = %s", __FUNCTION__, Attributes::name, name.c_str());

    size_t skipped = 0;
    AttributesGroups attrGroups;  // using AttributesGroups = std::vector<AttributesGroup>;
    deserializeCollection<AttributesGroupTraits>(doc, child, attrGroups, skipped);

    //strategies 为 using ProductStrategies = std::vector<ProductStrategy>;
    strategies.push_back({name, attrGroups});
    return NO_ERROR;
}

// 
status_t AttributesGroupTraits::deserialize(_xmlDoc *doc, const _xmlNode *child, Collection &attributesGroup)
{
    // Attributes::name = "name";
    std::string name = getXmlAttribute(child, Attributes::name);// "STRATEGY_MEDIA"
    if (name.empty()) {
        ALOGV("AttributesGroupTraits No attribute %s found", Attributes::name);
    }
    ALOGV("%s: %s = %s", __FUNCTION__, Attributes::name, name.c_str());

    // Attributes::volumeGroup = "volumeGroup";
    std::string volumeGroup = getXmlAttribute(child, Attributes::volumeGroup);// "music"
    if (volumeGroup.empty()) {
        ALOGE("%s: No attribute %s found", __FUNCTION__, Attributes::volumeGroup);
    }
    ALOGV("%s: %s = %s", __FUNCTION__, Attributes::volumeGroup, volumeGroup.c_str());

    audio_stream_type_t streamType = AUDIO_STREAM_DEFAULT;
    // Attributes::streamType = "streamType";
    std::string streamTypeXml = getXmlAttribute(child, Attributes::streamType); // "AUDIO_STREAM_MUSIC"
    if (streamTypeXml.empty()) {
        ALOGV("%s: No attribute %s found", __FUNCTION__, Attributes::streamType);
    } else {
        ALOGV("%s: %s = %s", __FUNCTION__, Attributes::streamType, streamTypeXml.c_str());
        if (not StreamTypeConverter::fromString(streamTypeXml.c_str(), streamType)) {
            ALOGE("Invalid stream type %s", streamTypeXml.c_str());
            return BAD_VALUE;
        }
    }
     // using AttributesVector = std::vector<audio_attributes_t>;
    AttributesVector attributesVect;  
    deserializeAttributesCollection(doc, child, attributesVect);

    attributesGroup.push_back({name, streamType, volumeGroup, attributesVect});
    return NO_ERROR;
}


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



static status_t deserializeAttributes(_xmlDoc *doc, const _xmlNode *cur, audio_attributes_t &attributes) {
    // Retrieve content type, usage, flags, and bundle from xml
    for (; cur != NULL; cur = cur->next) {
        if (not xmlStrcmp(cur->name, (const xmlChar *)("Attributes"))) {
            const xmlNode *attrNode = cur;
            // static constexpr const char *attributesAttributeRef = "attributesRef"; /**< for factorization. */
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


static status_t parseAttributes(const _xmlNode *cur, audio_attributes_t &attributes)
{
    for (; cur != NULL; cur = cur->next) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)("ContentType"))) {
            std::string contentTypeXml = getXmlAttribute(cur, "value");
            audio_content_type_t contentType;
            if (not AudioContentTypeConverter::fromString(contentTypeXml.c_str(), contentType)) {
                ALOGE("Invalid content type %s", contentTypeXml.c_str());
                return BAD_VALUE;
            }
            attributes.content_type = contentType;
            ALOGV("%s content type %s",  __FUNCTION__, contentTypeXml.c_str());
        }
        if (!xmlStrcmp(cur->name, (const xmlChar *)("Usage"))) {
            std::string usageXml = getXmlAttribute(cur, "value");
            audio_usage_t usage;
            if (not UsageTypeConverter::fromString(usageXml.c_str(), usage)) {
                ALOGE("Invalid usage %s", usageXml.c_str());
                return BAD_VALUE;
            }
            attributes.usage = usage;
            ALOGV("%s usage %s",  __FUNCTION__, usageXml.c_str());
        }
        if (!xmlStrcmp(cur->name, (const xmlChar *)("Flags"))) {
            std::string flags = getXmlAttribute(cur, "value");

            ALOGV("%s flags %s",  __FUNCTION__, flags.c_str());
            attributes.flags = AudioFlagConverter::maskFromString(flags, " ");
        }
        if (!xmlStrcmp(cur->name, (const xmlChar *)("Bundle"))) {
            std::string bundleKey = getXmlAttribute(cur, "key");
            std::string bundleValue = getXmlAttribute(cur, "value");

            ALOGV("%s Bundle %s %s",  __FUNCTION__, bundleKey.c_str(), bundleValue.c_str());

            std::string tags(bundleKey + "=" + bundleValue);
            std::strncpy(attributes.tags, tags.c_str(), AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - 1);
        }
    }
    return NO_ERROR;
}




















