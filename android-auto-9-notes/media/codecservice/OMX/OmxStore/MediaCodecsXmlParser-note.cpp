//  @frameworks/av/media/libstagefright/xmlparser/MediaCodecsXmlParser.cpp
class MediaCodecsXmlParser {
    // Treblized media codec list will be located in /odm/etc or /vendor/etc.
    static constexpr char const* defaultSearchDirs[] = {"/odm/etc", "/vendor/etc", "/etc", nullptr};
    static constexpr char const* defaultMainXmlName = "media_codecs.xml";
    static constexpr char const* defaultPerformanceXmlName = "media_codecs_performance.xml";
    static constexpr char const* defaultProfilingResultsXmlPath = "/data/misc/media/media_codecs_profiling_results.xml";//该文件存在
}

MediaCodecsXmlParser::MediaCodecsXmlParser(
        const char* const* searchDirs,
        const char* mainXmlName,
        const char* performanceXmlName,
        const char* profilingResultsXmlPath) :
    mParsingStatus(NO_INIT),
    mUpdate(false),
    mCodecCounter(0) {
    std::string path;
    /**
     * # find etc/ vendor/etc/ odm/ -type f -name media_codecs.xml                                                                                                                                  
     * vendor/etc/media_codecs.xml
     */
    if (findFileInDirs(searchDirs, mainXmlName, &path)) {
        parseTopLevelXMLFile(path.c_str(), false);
    } else {
        ALOGE("Cannot find %s", mainXmlName);
        mParsingStatus = NAME_NOT_FOUND;
    }
    if (findFileInDirs(searchDirs, performanceXmlName, &path)) {
        parseTopLevelXMLFile(path.c_str(), true);
    }
    if (profilingResultsXmlPath != nullptr) {
        parseTopLevelXMLFile(profilingResultsXmlPath, true);
    }
}


void MediaCodecsXmlParser::parseXMLFile(const char *path) {
    FILE *file = fopen(path, "r");//打开文件

    if (file == nullptr) {
        ALOGW("unable to open media codecs configuration xml file: %s", path);
        mParsingStatus = NAME_NOT_FOUND;
        return;
    }

    XML_Parser parser = ::XML_ParserCreate(nullptr);
    LOG_FATAL_IF(parser == nullptr, "XML_MediaCodecsXmlParserCreate() failed.");

    //设置回到指针 this
    ::XML_SetUserData(parser, this);

    //设置 解析开始回调函数StartElementHandlerWrapper，以及解析结束函数EndElementHandlerWrapper
    ::XML_SetElementHandler(parser, StartElementHandlerWrapper, EndElementHandlerWrapper);

    static constexpr int BUFF_SIZE = 512;
    while (mParsingStatus == OK) {
        void *buff = ::XML_GetBuffer(parser, BUFF_SIZE);
        if (buff == nullptr) {
            ALOGE("failed in call to XML_GetBuffer()");
            mParsingStatus = UNKNOWN_ERROR;
            break;
        }

        int bytes_read = ::fread(buff, 1, BUFF_SIZE, file);
        if (bytes_read < 0) {
            ALOGE("failed in call to read");
            mParsingStatus = ERROR_IO;
            break;
        }

        XML_Status status = ::XML_ParseBuffer(parser, bytes_read, bytes_read == 0);
        if (status != XML_STATUS_OK) {
            ALOGE("malformed (%s)", ::XML_ErrorString(::XML_GetErrorCode(parser)));
            mParsingStatus = ERROR_MALFORMED;
            break;
        }

        if (bytes_read == 0) {
            break;
        }
    }

    ::XML_ParserFree(parser);

    fclose(file);
    file = nullptr;
}


// static
void MediaCodecsXmlParser::StartElementHandlerWrapper(void *me, const char *name, const char **attrs) {
    // 将转到MediaCodecsXmlParser内部处理
    static_cast<MediaCodecsXmlParser*>(me)->startElementHandler(name, attrs);
}

// static
void MediaCodecsXmlParser::EndElementHandlerWrapper(void *me, const char *name) {
    // 将转到MediaCodecsXmlParser内部处理
    static_cast<MediaCodecsXmlParser*>(me)->endElementHandler(name);
}

typedef std::pair<std::string, std::string> Attribute;
typedef std::map<std::string, std::string> AttributeMap;

typedef std::pair<std::string, AttributeMap> Type;
typedef std::map<std::string, AttributeMap> TypeMap;

typedef std::set<std::string> QuirkSet;

struct CodecProperties {
    bool isEncoder;    ///< Whether this codec is an encoder or a decoder (区分编码还是解码组件)
    size_t order;      ///< Order of appearance in the file (starting from 0)   //这里是由添加时自动累加的索引值，每次添加之后 自动累加一
    QuirkSet quirkSet; ///< Set of quirks requested by this codec
    TypeMap typeMap;   ///< Map of types supported by this codec
};



typedef std::map<std::string, CodecProperties> CodecMap;
                                    |
                                    |
                                struct CodecProperties {
                                    QuirkSet quirkSet;-----------------------------------
                                    TypeMap typeMap; ---------                          |
                                };                           |       typedef std::set<std::string> QuirkSet
                                                             |                          
                                                             |                          
                                                             |
                                   typedef std::map<std::string, AttributeMap> TypeMap；
                                                                        |
                                                                        |
                                                                        |
                                        typedef std::map<std::string, std::string> AttributeMap;








/**
<MediaCodecs>
	<Include href="media_codecs_google_telephony.xml" />
    <Settings>
        <!--Setting 主要有是三个属性 name、value、update-->
        <!--属性保存在 AttributeMap mServiceAttributeMap; 其中保存name和value的键值对，
        而update只是用来检测错误（如果mServiceAttributeMap中已经有值则只是更新（update必须为true);如果mServiceAttributeMap中不存在则是添加（update必须为false);  ）  -->
        <!--typedef std::pair<std::string, std::string> Attribute;-->
        <!--typedef std::map<std::string, std::string> AttributeMap;-->
        <Setting name="max-video-encoder-input-buffers" value="8" />
    </Settings>



    
    <Decoders>
        <!-- MediaCodec 有三个属性 name、type、update （update的作用和Setting一样） -->
        <!-- typedef std::pair<std::string, CodecProperties> Codec; -->
        <!-- typedef std::map<std::string, CodecProperties> CodecMap;-->
        <!-- CodecMap mCodecMap;-->
        <MediaCodec name="OMX.Freescale.std.video_decoder.wmv.sw-based" type="video/x-wmv" >
            <Quirk name="requires-allocate-on-output-ports" />          <!--Quirk 对应存储在 CodecProperties 的 QuirkSet 集合中 (此集合只存储名称字符串)-->
            <Limit name="size" min="64x64" max="1920x1088" />
            <Limit name="alignment" value="2x2" />
            <Limit name="block-size" value="16x16" />
            <Limit name="blocks-per-second" min="1" max="244800" />
            <Limit name="bitrate" range="1-20000000"/>
            <Limit name="concurrent-instances" max="32" />
        </MediaCodec>
    </Decoders>
    <Encoders>
        <MediaCodec name="OMX.google.h263.encoder" type="video/3gpp">
            <!-- profiles and levels:  ProfileBaseline : Level45 -->
            <Limit name="size" min="176x144" max="176x144" />
            <Limit name="alignment" value="16x16" />
            <Limit name="bitrate" range="1-128000" />
            <!--Feature 属性有 name 、optional 、required 、value-->
            <Feature name="adaptive-playback" />   
        </MediaCodec>
    </Encoders>
</MediaCodecs>   
*/

//解析文件开始
void MediaCodecsXmlParser::startElementHandler(const char *name, const char **attrs) {
    if (mParsingStatus != OK) {
        return;
    }

    bool inType = true;

    if (strEq(name, "Include")) {
        mParsingStatus = includeXMLFile(attrs);
        if (mParsingStatus == OK) {
            mSectionStack.push_back(mCurrentSection);
            mCurrentSection = SECTION_INCLUDE;
        }
        return;
    }

    switch (mCurrentSection) {
        case SECTION_TOPLEVEL:
        {
            if (strEq(name, "Decoders")) {
                mCurrentSection = SECTION_DECODERS;
            } else if (strEq(name, "Encoders")) {
                mCurrentSection = SECTION_ENCODERS;
            } else if (strEq(name, "Settings")) {
                mCurrentSection = SECTION_SETTINGS;
            }
            break;
        }

        case SECTION_SETTINGS:
        {
            if (strEq(name, "Setting")) {
                mParsingStatus = addSettingFromAttributes(attrs);
            }
            break;
        }

        case SECTION_DECODERS:
        {
            if (strEq(name, "MediaCodec")) {
                mParsingStatus =
                    addMediaCodecFromAttributes(false /* encoder */, attrs);

                mCurrentSection = SECTION_DECODER;
            }
            break;
        }

        case SECTION_ENCODERS:
        {
            if (strEq(name, "MediaCodec")) {
                mParsingStatus =
                    addMediaCodecFromAttributes(true /* encoder */, attrs);

                mCurrentSection = SECTION_ENCODER;
            }
            break;
        }

        case SECTION_DECODER:
        case SECTION_ENCODER:
        {
            if (strEq(name, "Quirk")) {
                mParsingStatus = addQuirk(attrs);
            } else if (strEq(name, "Type")) {
                mParsingStatus = addTypeFromAttributes(attrs,
                        (mCurrentSection == SECTION_ENCODER));
                mCurrentSection =
                        (mCurrentSection == SECTION_DECODER ?
                        SECTION_DECODER_TYPE : SECTION_ENCODER_TYPE);
            }
        }
        inType = false;     //  这里特别注意没有break跳出
        // fall through

        case SECTION_DECODER_TYPE:
        case SECTION_ENCODER_TYPE:
        {
            // ignore limits and features specified outside of type
            bool outside = !inType && mCurrentType == mCurrentCodec->second.typeMap.end();
            if (outside && (strEq(name, "Limit") || strEq(name, "Feature"))) {
                ALOGW("ignoring %s specified outside of a Type", name);
            } else if (strEq(name, "Limit")) {
                mParsingStatus = addLimit(attrs);
            } else if (strEq(name, "Feature")) {
                mParsingStatus = addFeature(attrs);
            }
            break;
        }

        default:
            break;
    }

}


void MediaCodecsXmlParser::endElementHandler(const char *name) {
    if (mParsingStatus != OK) {
        return;
    }

    switch (mCurrentSection) {
        case SECTION_SETTINGS:
        {
            if (strEq(name, "Settings")) {
                mCurrentSection = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_DECODERS:
        {
            if (strEq(name, "Decoders")) {
                mCurrentSection = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_ENCODERS:
        {
            if (strEq(name, "Encoders")) {
                mCurrentSection = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_DECODER_TYPE:
        case SECTION_ENCODER_TYPE:
        {
            if (strEq(name, "Type")) {
                mCurrentSection =
                        (mCurrentSection == SECTION_DECODER_TYPE ?
                        SECTION_DECODER : SECTION_ENCODER);

                mCurrentType = mCurrentCodec->second.typeMap.end();
            }
            break;
        }

        case SECTION_DECODER:
        {
            if (strEq(name, "MediaCodec")) {
                mCurrentSection = SECTION_DECODERS;
                mCurrentName.clear();
            }
            break;
        }

        case SECTION_ENCODER:
        {
            if (strEq(name, "MediaCodec")) {
                mCurrentSection = SECTION_ENCODERS;
                mCurrentName.clear();
            }
            break;
        }

        case SECTION_INCLUDE:
        {
            if (strEq(name, "Include") && (mSectionStack.size() > 0)) {
                mCurrentSection = mSectionStack.back();
                mSectionStack.pop_back();
            }
            break;
        }

        default:
            break;
    }

}



//  生成RoleMap分析
const MediaCodecsXmlParser::RoleMap& MediaCodecsXmlParser::getRoleMap() const {
    if (mRoleMap.empty()) {
        generateRoleMap();
    }
    return mRoleMap;
}


struct NodeInfo {
    std::string name;
    std::vector<Attribute> attributeList;
};


struct RoleProperties {
    std::string type;
    bool isEncoder;
    std::multimap<size_t, NodeInfo> nodeList;
};

void MediaCodecsXmlParser::generateRoleMap() const {
    for (const auto& codec : mCodecMap) {
        const auto& codecName = codec.first;       // <MediaCodec name="OMX.Freescale.std.video_decoder.avc.v3.hw-based" type="..." >
        bool isEncoder = codec.second.isEncoder;
        size_t order = codec.second.order;  //这里 order 是加载 xml 文件 ，累加的id索引

        const auto& typeMap = codec.second.typeMap; //  typedef std::map<std::string, AttributeMap> TypeMap；
        for (const auto& type : typeMap) {
            const auto& typeName = type.first;     // <MediaCodec name="..." type="video/avc" >
            const char* roleName = GetComponentRole(isEncoder, typeName.data());
            if (roleName == nullptr) {
                ALOGE("Cannot find the role for %s of type %s", isEncoder ? "an encoder" : "a decoder", typeName.data());
                continue;
            }
            const auto& typeAttributeMap = type.second;

            auto roleIterator = mRoleMap.find(roleName);
            std::multimap<size_t, NodeInfo>* nodeList;
            if (roleIterator == mRoleMap.end()) {
                //  @frameworks/av/media/libstagefright/xmlparser/include/media/stagefright/xmlparser/MediaCodecsXmlParser.h

                RoleProperties roleProperties;
                roleProperties.type = typeName; // <MediaCodec name="..." type="video/avc" >
                roleProperties.isEncoder = isEncoder;
                //  roleName 是由 查询得到 
                auto insertResult = mRoleMap.insert(std::make_pair(roleName, roleProperties));
                if (!insertResult.second) {
                    ALOGE("Cannot add role %s", roleName);
                    continue;
                }
                nodeList = &insertResult.first->second.nodeList;
            } else {
                if (roleIterator->second.type != typeName) {
                    ALOGE("Role %s has mismatching types: %s and %s",
                            roleName,roleIterator->second.type.data(),typeName.data());
                    continue;
                }
                if (roleIterator->second.isEncoder != isEncoder) {
                    ALOGE("Role %s cannot be both an encoder and a decoder",
                            roleName);
                    continue;
                }
                nodeList = &roleIterator->second.nodeList;
            }

            NodeInfo nodeInfo;
            nodeInfo.name = codecName;     // <MediaCodec name="OMX.Freescale.std.video_decoder.avc.v3.hw-based" type="..." >       
            nodeInfo.attributeList.reserve(typeAttributeMap.size());
            for (const auto& attribute : typeAttributeMap) {
                nodeInfo.attributeList.push_back(Attribute{attribute.first, attribute.second});
            }
            nodeList->insert(std::make_pair(std::move(order), std::move(nodeInfo)));
        }
    }
}




//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/media/libstagefright/omx/OMXUtils.cpp
const char *GetComponentRole(bool isEncoder, const char *mime) {
    struct MimeToRole {
        const char *mime;
        const char *decoderRole;
        const char *encoderRole;
    };
    //  下面mime属性  @frameworks/av/media/libstagefright/foundation/MediaDefs.cpp
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
        { MEDIA_MIMETYPE_IMAGE_ANDROID_HEIC,
            "image_decoder.heic", "image_encoder.heic" },
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

    static const size_t kNumMimeToRole =
        sizeof(kMimeToRole) / sizeof(kMimeToRole[0]);

    size_t i;
    for (i = 0; i < kNumMimeToRole; ++i) {
        if (!strcasecmp(mime, kMimeToRole[i].mime)) {
            break;
        }
    }

    if (i == kNumMimeToRole) {
        return NULL;
    }

    return isEncoder ? kMimeToRole[i].encoderRole
                  : kMimeToRole[i].decoderRole;
}


//  生成RoleMap分析
const char* MediaCodecsXmlParser::getCommonPrefix() const {
    if (mCommonPrefix.empty()) {
        generateCommonPrefix();
    }
    return mCommonPrefix.data();
}


 // Computed longest common prefix
mutable std::string mCommonPrefix;


//得到字符串相同的部分
void MediaCodecsXmlParser::generateCommonPrefix() const {
    if (mCodecMap.empty()) {
        return;
    }
    auto i = mCodecMap.cbegin();    
    auto first = i->first.cbegin(); // <MediaCodec name="OMX.Freescale.std.video_decoder.avc.v3.hw-based" type="..." >
    auto last = i->first.cend();
    for (++i; i != mCodecMap.cend(); ++i) {
        last = std::mismatch(first, last, i->first.cbegin(), i->first.cend()).first;
    }
    mCommonPrefix.insert(mCommonPrefix.begin(), first, last);
}