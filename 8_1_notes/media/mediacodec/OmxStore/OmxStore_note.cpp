
//	@frameworks/av/media/libstagefright/omx/include/media/stagefright/omx/1.0/OmxStore.h

struct OmxStore : public IOmxStore {
    OmxStore(
            const char* owner = "default",
            const char* const* searchDirs = MediaCodecsXmlParser::defaultSearchDirs,		//    static constexpr char const* defaultSearchDirs[] = {"/odm/etc", "/vendor/etc", "/etc", nullptr};
            const char* mainXmlName  = MediaCodecsXmlParser::defaultMainXmlName,	//	 static constexpr char const* defaultMainXmlName = "media_codecs.xml";
            const char* performanceXmlName = MediaCodecsXmlParser::defaultPerformanceXmlName,	//	    static constexpr char const* defaultPerformanceXmlName = "media_codecs_performance.xml";
            const char* profilingResultsXmlPath = MediaCodecsXmlParser::defaultProfilingResultsXmlPath);//	    static constexpr char const* defaultProfilingResultsXmlPath = "/data/misc/media/media_codecs_profiling_results.xml";

    virtual ~OmxStore();

    // Methods from IOmxStore
    Return<void> listServiceAttributes(listServiceAttributes_cb) override;
    Return<void> getNodePrefix(getNodePrefix_cb) override;
    Return<void> listRoles(listRoles_cb) override;
    Return<sp<IOmx>> getOmx(hidl_string const&) override;

protected:
    Status mParsingStatus;
    hidl_string mPrefix;
    hidl_vec<ServiceAttribute> mServiceAttributeList;
    hidl_vec<RoleInfo> mRoleList;
};




//	@frameworks/av/media/libstagefright/omx/1.0/OmxStore.cpp
OmxStore::OmxStore(
        const char* owner,
        const char* const* searchDirs,
        const char* mainXmlName,
        const char* performanceXmlName,
        const char* profilingResultsXmlPath) {

    MediaCodecsXmlParser parser(searchDirs,mainXmlName,performanceXmlName,profilingResultsXmlPath);
    mParsingStatus = toStatus(parser.getParsingStatus());

    const auto& serviceAttributeMap = parser.getServiceAttributeMap();
    mServiceAttributeList.resize(serviceAttributeMap.size());
    size_t i = 0;
    for (const auto& attributePair : serviceAttributeMap) {
        ServiceAttribute attribute;
        attribute.key = attributePair.first;
        attribute.value = attributePair.second;
        mServiceAttributeList[i] = std::move(attribute);
        ++i;
    }

    const auto& roleMap = parser.getRoleMap();
    mRoleList.resize(roleMap.size());
    i = 0;
    for (const auto& rolePair : roleMap) {
        RoleInfo role;
        role.role = rolePair.first;
        role.type = rolePair.second.type;
        role.isEncoder = rolePair.second.isEncoder;
        // TODO: Currently, preferPlatformNodes information is not available in
        // the xml file. Once we have a way to provide this information, it
        // should be parsed properly.
        role.preferPlatformNodes = rolePair.first.compare(0, 5, "audio") == 0;
        hidl_vec<NodeInfo>& nodeList = role.nodes;
        nodeList.resize(rolePair.second.nodeList.size());
        size_t j = 0;
        for (const auto& nodePair : rolePair.second.nodeList) {
            NodeInfo node;
            node.name = nodePair.second.name;
            node.owner = owner;
            hidl_vec<NodeAttribute>& attributeList = node.attributes;
            attributeList.resize(nodePair.second.attributeList.size());
            size_t k = 0;
            for (const auto& attributePair : nodePair.second.attributeList) {
                NodeAttribute attribute;
                attribute.key = attributePair.first;
                attribute.value = attributePair.second;
                attributeList[k] = std::move(attribute);
                ++k;
            }
            nodeList[j] = std::move(node);
            ++j;
        }
        mRoleList[i] = std::move(role);
        ++i;
    }

    mPrefix = parser.getCommonPrefix();
}