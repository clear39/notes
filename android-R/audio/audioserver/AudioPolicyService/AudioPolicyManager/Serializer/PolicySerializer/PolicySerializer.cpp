


class PolicySerializer
{
public:
    PolicySerializer() : mVersion{std::to_string(gMajor) + "." + std::to_string(gMinor)}
    {
        ALOGV("%s: Version=%s Root=%s", __func__, mVersion.c_str(), rootName);
    }
    status_t deserialize(const char *configFile, AudioPolicyConfig *config);

private:
    static constexpr const char *rootName = "audioPolicyConfiguration";
    static constexpr const char *versionAttribute = "version";
    static constexpr uint32_t gMajor = 1; /**< the major number of the policy xml format version. */
    static constexpr uint32_t gMinor = 0; /**< the minor number of the policy xml format version. */

    typedef AudioPolicyConfig Element;

    const std::string mVersion;

    // Children: ModulesTraits, VolumeTraits, SurroundSoundTraits (optional)
};


status_t PolicySerializer::deserialize(const char *configFile, AudioPolicyConfig *config)
{
    auto doc = make_xmlUnique(xmlParseFile(configFile));
    if (doc == nullptr) {
        ALOGE("%s: Could not parse %s document.", __func__, configFile);
        return BAD_VALUE;
    }
    xmlNodePtr root = xmlDocGetRootElement(doc.get());
    if (root == NULL) {
        ALOGE("%s: Could not parse %s document: empty.", __func__, configFile);
        return BAD_VALUE;
    }
    if (xmlXIncludeProcess(doc.get()) < 0) {
        ALOGE("%s: libxml failed to resolve XIncludes on %s document.", __func__, configFile);
    }

    if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>(rootName)))  {  //  *rootName = "audioPolicyConfiguration";
        ALOGE("%s: No %s root element found in xml data %s.", __func__, rootName, reinterpret_cast<const char*>(root->name));
        return BAD_VALUE;
    }

    std::string version = getXmlAttribute(root, versionAttribute);  // *versionAttribute = "version";
    if (version.empty()) {
        ALOGE("%s: No version found in root node %s", __func__, rootName);
        return BAD_VALUE;
    }
    if (version != mVersion) {
        ALOGE("%s: Version does not match; expect %s got %s", __func__, mVersion.c_str(), version.c_str());
        return BAD_VALUE;
    }
    // Lets deserialize children
    // Modules
    ModuleTraits::Collection modules;
    status_t status = deserializeCollection<ModuleTraits>(root, &modules, config);
    if (status != NO_ERROR) {
        return status;
    }
    config->setHwModules(modules);

    // Global Configuration
    /**
     * <globalConfiguration speaker_drc_enabled="true"/>
     * 
     * 
    */
    GlobalConfigTraits::deserialize(root, config);

    // Surround configuration
    /**
     * <format name="AUDIO_FORMAT_AAC_LC" subformats="AUDIO_FORMAT_AAC_HE_V1 AUDIO_FORMAT_AAC_HE_V2 AUDIO_FORMAT_AAC_ELD AUDIO_FORMAT_AAC_XHE" />
     * using SurroundFormats = std::unordered_map<audio_format_t, std::unordered_set<audio_format_t>>;
     * 第一个属性 name="AUDIO_FORMAT_AAC_LC" 解析转化为 audio_format_t，作为std::unordered_map 的key 
     * 第二个属性 subformats 解析为 std::unordered_set<audio_format_t>，作为 std::unordered_map 的value
    */
    SurroundSoundTraits::deserialize(root, config);

    return android::OK;
}



status_t SurroundSoundTraits::deserialize(const xmlNode *root, AudioPolicyConfig *config)
{
    /**
     * 这里是初始化 SurroundFormats mSurroundFormats; 成员
    */
    config->setDefaultSurroundFormats();

    for (const xmlNode *cur = root->xmlChildrenNode; cur != NULL; cur = cur->next) {
        if (!xmlStrcmp(cur->name, reinterpret_cast<const xmlChar*>(tag))) {// SurroundSoundTraits::tag = "surroundSound";
            /**
             *  using SurroundFormats = std::unordered_map<audio_format_t, std::unordered_set<audio_format_t>>;
            */
            AudioPolicyConfig::SurroundFormats formats;
            status_t status = deserializeCollection<SurroundSoundFormatTraits>(cur, &formats, nullptr);
            if (status == NO_ERROR) {
                /**
                 * 重新给setDefaultSurroundFormats中的变量（SurroundFormats mSurroundFormats;）赋值为formats
                */
                config->setSurroundFormats(formats);
            }
            return NO_ERROR;
        }
    }
    return NO_ERROR;
}

// Trait 为 SurroundSoundFormatTraits
template <class Trait>
status_t deserializeCollection(const xmlNode *cur,
        typename Trait::Collection *collection,// AudioPolicyConfig::SurroundFormats
        typename Trait::PtrSerializingCtx serializingContext)
{
    for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
        const xmlNode *child = NULL;
        // 
        if (!xmlStrcmp(cur->name, reinterpret_cast<const xmlChar*>(Trait::collectionTag))) {  // SurroundSoundFormatTraits::collectionTag = "formats";
            child = cur->xmlChildrenNode;
        } else if (!xmlStrcmp(cur->name, reinterpret_cast<const xmlChar*>(Trait::tag))) { // SurroundSoundFormatTraits::tag = "format";
            child = cur;
        }
        for (; child != NULL; child = child->next) {
            /**
             * 
            */
            if (!xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>(Trait::tag))) {
                // SurroundSoundTraits::deserialize(child, serializingContext);
                auto element = Trait::deserialize(child, serializingContext);
                if (element.isOk()) {
                    // SurroundSoundFormatTraits::addElementToCollection(element, collection);
                    status_t status = Trait::addElementToCollection(element, collection);
                    if (status != NO_ERROR) {
                        ALOGE("%s: could not add element to %s collection", __func__, Trait::collectionTag);
                        return status;
                    }
                } else {
                    return BAD_VALUE;
                }
            }
        }
        if (!xmlStrcmp(cur->name, reinterpret_cast<const xmlChar*>(Trait::tag))) {
            return NO_ERROR;
        }
    }
    return NO_ERROR;
}



Return<SurroundSoundFormatTraits::Element> SurroundSoundFormatTraits::deserialize(
        const xmlNode *cur, PtrSerializingCtx /*serializingContext*/)
{
    std::string formatLiteral = getXmlAttribute(cur, Attributes::name);
    if (formatLiteral.empty()) {
        ALOGE("%s: No %s found for a surround format", __func__, Attributes::name);
        return Status::fromStatusT(BAD_VALUE);
    }
    audio_format_t format = formatFromString(formatLiteral);
    if (format == AUDIO_FORMAT_DEFAULT) {
        ALOGE("%s: Unrecognized format %s", __func__, formatLiteral.c_str());
        return Status::fromStatusT(BAD_VALUE);
    }
    Element pair = std::make_pair(format, Collection::mapped_type{});

    std::string subformatsLiteral = getXmlAttribute(cur, Attributes::subformats);
    if (subformatsLiteral.empty()) return pair;
    FormatVector subformats = formatsFromString(subformatsLiteral, " ");
    for (const auto& subformat : subformats) {
        auto result = pair.second.insert(subformat);
        if (!result.second) {
            ALOGE("%s: could not add subformat %x to collection", __func__, subformat);
            return Status::fromStatusT(BAD_VALUE);
        }
    }
    return pair;
}