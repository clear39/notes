
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

    //对应settings子模块
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



const char* MediaCodecsXmlParser::getCommonPrefix() const {
    if (mCommonPrefix.empty()) {//  mutable std::string mCommonPrefix;
        generateCommonPrefix();
    }
    return mCommonPrefix.data();
}


void MediaCodecsXmlParser::generateCommonPrefix() const {
    if (mCodecMap.empty()) {
        return;
    }
    auto i = mCodecMap.cbegin();
    auto first = i->first.cbegin();
    auto last = i->first.cend();
    for (++i; i != mCodecMap.cend(); ++i) {
        last = std::mismatch(
                first, last, i->first.cbegin(), i->first.cend()).first;
    }
    mCommonPrefix.insert(mCommonPrefix.begin(), first, last);
}




Return<void> OmxStore::listServiceAttributes(listServiceAttributes_cb _hidl_cb) {
    if (mParsingStatus == Status::NO_ERROR) {
        _hidl_cb(Status::NO_ERROR, mServiceAttributeList);
    } else {
        _hidl_cb(mParsingStatus, hidl_vec<ServiceAttribute>());
    }
    return Void();
}

Return<void> OmxStore::getNodePrefix(getNodePrefix_cb _hidl_cb) {
    _hidl_cb(mPrefix);
    return Void();
}

Return<void> OmxStore::listRoles(listRoles_cb _hidl_cb) {
    _hidl_cb(mRoleList);
    return Void();
}

Return<sp<IOmx>> OmxStore::getOmx(hidl_string const& omxName) {
    return IOmx::tryGetService(omxName);
}



































typedef std::pair<std::string, std::string> Attribute;
/**
 * Properties of a node (for IOmxStore)
 */
struct NodeInfo {
    std::string name;
    std::vector<Attribute> attributeList;
};

/**
 * Properties of a role (for IOmxStore)
 */
struct RoleProperties {
    std::string type;
    bool isEncoder;
    std::multimap<size_t, NodeInfo> nodeList;
};

const MediaCodecsXmlParser::RoleMap& MediaCodecsXmlParser::getRoleMap() const {
    //  typedef std::map<std::string, RoleProperties> RoleMap;
    if (mRoleMap.empty()) { //  mutable RoleMap mRoleMap;
        generateRoleMap();
    }
    return mRoleMap;
}


void MediaCodecsXmlParser::generateRoleMap() const {
    for (const auto& codec : mCodecMap) {
        const auto& codecName = codec.first;
        bool isEncoder = codec.second.isEncoder;
        size_t order = codec.second.order;
        const auto& typeMap = codec.second.typeMap; //  对应type
        for (const auto& type : typeMap) {
            const auto& typeName = type.first;
            const char* roleName = GetComponentRole(isEncoder, typeName.data());//  @frameworks/av/media/libstagefright/omx/OMXUtils.cpp:113
            if (roleName == nullptr) {
                ALOGE("Cannot find the role for %s of type %s", isEncoder ? "an encoder" : "a decoder", typeName.data());
                continue;
            }
            const auto& typeAttributeMap = type.second;

            auto roleIterator = mRoleMap.find(roleName);
            std::multimap<size_t, NodeInfo>* nodeList;
            if (roleIterator == mRoleMap.end()) {
                RoleProperties roleProperties;
                roleProperties.type = typeName;
                roleProperties.isEncoder = isEncoder;
                auto insertResult = mRoleMap.insert(std::make_pair(roleName, roleProperties));
                if (!insertResult.second) {
                    ALOGE("Cannot add role %s", roleName);
                    continue;
                }
                nodeList = &insertResult.first->second.nodeList;
            } else {
                if (roleIterator->second.type != typeName) {
                    ALOGE("Role %s has mismatching types: %s and %s",roleName,roleIterator->second.type.data(),typeName.data());
                    continue;
                }
                if (roleIterator->second.isEncoder != isEncoder) {
                    ALOGE("Role %s cannot be both an encoder and a decoder", roleName);
                    continue;
                }
                nodeList = &roleIterator->second.nodeList;
            }

            NodeInfo nodeInfo;
            nodeInfo.name = codecName;
            nodeInfo.attributeList.reserve(typeAttributeMap.size());
            for (const auto& attribute : typeAttributeMap) {
                nodeInfo.attributeList.push_back(Attribute{attribute.first, attribute.second});
            }
            nodeList->insert(std::make_pair(std::move(order), std::move(nodeInfo)));
        }
    }
}



//  @frameworks/av/media/libstagefright/omx/OMXUtils.cpp:113
const char *GetComponentRole(bool isEncoder, const char *mime) {
    struct MimeToRole {
        const char *mime;
        const char *decoderRole;
        const char *encoderRole;
    };

    static const MimeToRole kMimeToRole[] = {
        { MEDIA_MIMETYPE_AUDIO_MPEG,
            "audio_decoder.mp3", "audio_encoder.mp3" },
        { MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_I,
            "audio_decoder.mp1", "audio_encoder.mp1" },
        { MEDIA_MIMETYPE_AUDIO_MPEG_LAYER_II,
            "audio_decoder.mp2", "audio_encoder.mp2" },
        { MEDIA_MIMETYPE_AUDIO_AMR_NB,
            "audio_decoder.amrnb", "audio_encoder.amrnb" },
        { MEDIA_MIMETYPE_AUDIO_AMR_WB,
            "audio_decoder.amrwb", "audio_encoder.amrwb" },
        { MEDIA_MIMETYPE_AUDIO_AAC,
            "audio_decoder.aac", "audio_encoder.aac" },
        { MEDIA_MIMETYPE_AUDIO_VORBIS,
            "audio_decoder.vorbis", "audio_encoder.vorbis" },
        { MEDIA_MIMETYPE_AUDIO_OPUS,
            "audio_decoder.opus", "audio_encoder.opus" },
        { MEDIA_MIMETYPE_AUDIO_G711_MLAW,
            "audio_decoder.g711mlaw", "audio_encoder.g711mlaw" },
        { MEDIA_MIMETYPE_AUDIO_G711_ALAW,
            "audio_decoder.g711alaw", "audio_encoder.g711alaw" },
        { MEDIA_MIMETYPE_VIDEO_AVC,
            "video_decoder.avc", "video_encoder.avc" },
        { MEDIA_MIMETYPE_VIDEO_HEVC,
            "video_decoder.hevc", "video_encoder.hevc" },
        { MEDIA_MIMETYPE_VIDEO_MPEG4,
            "video_decoder.mpeg4", "video_encoder.mpeg4" },
        { MEDIA_MIMETYPE_VIDEO_H263,
            "video_decoder.h263", "video_encoder.h263" },
        { MEDIA_MIMETYPE_VIDEO_VP8,
            "video_decoder.vp8", "video_encoder.vp8" },
        { MEDIA_MIMETYPE_VIDEO_VP9,
            "video_decoder.vp9", "video_encoder.vp9" },
        { MEDIA_MIMETYPE_AUDIO_RAW,
            "audio_decoder.raw", "audio_encoder.raw" },
        { MEDIA_MIMETYPE_VIDEO_DOLBY_VISION,
            "video_decoder.dolby-vision", "video_encoder.dolby-vision" },
        { MEDIA_MIMETYPE_AUDIO_FLAC,
            "audio_decoder.flac", "audio_encoder.flac" },
        { MEDIA_MIMETYPE_AUDIO_MSGSM,
            "audio_decoder.gsm", "audio_encoder.gsm" },
        { MEDIA_MIMETYPE_VIDEO_MPEG2,
            "video_decoder.mpeg2", "video_encoder.mpeg2" },
        { MEDIA_MIMETYPE_AUDIO_AC3,
            "audio_decoder.ac3", "audio_encoder.ac3" },
        { MEDIA_MIMETYPE_AUDIO_EAC3,
            "audio_decoder.eac3", "audio_encoder.eac3" },
        //nxp added
        { MEDIA_MIMETYPE_VIDEO_WMV9,
            "video_decoder.wmv9", "video_encoder.wmv9" },
        { MEDIA_MIMETYPE_VIDEO_WMV,
            "video_decoder.wmv", "video_encoder.wmv" },
        { MEDIA_MIMETYPE_VIDEO_REAL,
            "video_decoder.rv", "video_encoder.rv" },
        { MEDIA_MIMETYPE_VIDEO_SORENSON,
            "video_decoder.sorenson", "video_encoder.sorenson" },
        { MEDIA_MIMETYPE_VIDEO_MJPEG,
            "video_decoder.mjpeg", "video_encoder.mjpeg" },
        { MEDIA_MIMETYPE_VIDEO_DIVX,
            "video_decoder.divx", "video_encoder.divx" },
        { MEDIA_MIMETYPE_VIDEO_DIV4,
            "video_decoder.div4", "video_encoder.div4" },
        { MEDIA_MIMETYPE_VIDEO_DIV3,
            "video_decoder.div3", "video_encoder.div3" },
        { MEDIA_MIMETYPE_VIDEO_XVID,
            "video_decoder.xvid", "video_encoder.xvid" },
        { MEDIA_MIMETYPE_AUDIO_WMA,
            "audio_decoder.wma", "audio_encoder.wma" },
        { MEDIA_MIMETYPE_AUDIO_APE,
            "audio_decoder.ape", "audio_encoder.ape" },
        { MEDIA_MIMETYPE_AUDIO_REAL,
            "audio_decoder.ra", "audio_encoder.ra" },
        { MEDIA_MIMETYPE_AUDIO_AAC_FSL,
            "audio_decoder.aac-fsl", "audio_encoder.aac" },
        { MEDIA_MIMETYPE_AUDIO_BSAC,
            "audio_decoder.bsac", "audio_encoder.bsac" },
    };

    static const size_t kNumMimeToRole = sizeof(kMimeToRole) / sizeof(kMimeToRole[0]);

    size_t i;
    for (i = 0; i < kNumMimeToRole; ++i) {
        if (!strcasecmp(mime, kMimeToRole[i].mime)) {
            break;
        }
    }

    if (i == kNumMimeToRole) {
        return NULL;
    }

    return isEncoder ? kMimeToRole[i].encoderRole : kMimeToRole[i].decoderRole;
}