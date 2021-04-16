

//  @   frameworks\av\services\audiopolicy\engine\config\src\EngineConfig.cpp
// constexpr char DEFAULT_PATH[] = "/vendor/etc/audio_policy_engine_configuration.xml";

ParsingResult parse(const char* path /*= DEFAULT_PATH*/) {
    XmlErrorHandler errorHandler;
    xmlDocPtr doc;
    doc = xmlParseFile(path);
    if (doc == NULL) {
        ALOGE("%s: Could not parse document %s", __FUNCTION__, path);
        return {nullptr, 0};
    }
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        ALOGE("%s: Could not parse: empty document %s", __FUNCTION__, path);
        xmlFreeDoc(doc);
        return {nullptr, 0};
    }
    if (xmlXIncludeProcess(doc) < 0) {
        ALOGE("%s: libxml failed to resolve XIncludes on document %s", __FUNCTION__, path);
        return {nullptr, 0};
    }
    std::string version = getXmlAttribute(cur, gVersionAttribute);
    if (version.empty()) {
        ALOGE("%s: No version found", __func__);
        return {nullptr, 0};
    }
    size_t nbSkippedElements = 0;
    auto config = std::make_unique<Config>();
    // <configuration version="1.0" xmlns:xi="http://www.w3.org/2001/XInclude">
    config->version = std::stof(version); //浮点数 1.0


    // 具体分析在 ProductStrategyTraits.cpp中
    deserializeCollection<ProductStrategyTraits>(doc, cur, config->productStrategies, nbSkippedElements);

    //具体分析在 CriterionTraits.cpp中
    deserializeCollection<CriterionTraits>(doc, cur, config->criteria, nbSkippedElements);

    //具体分析在CriterionTypeTraits.cpp中
    deserializeCollection<CriterionTypeTraits>(doc, cur, config->criterionTypes, nbSkippedElements);


    //具体分析在VolumeGroupTraits.cpp中
    deserializeCollection<VolumeGroupTraits>(doc, cur, config->volumeGroups, nbSkippedElements);

    return {std::move(config), nbSkippedElements};
}