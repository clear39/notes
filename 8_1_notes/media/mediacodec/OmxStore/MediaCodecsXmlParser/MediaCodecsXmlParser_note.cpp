
bool findFileInDirs(const char* const* searchDirs, const char *fileName, std::string *outPath) {
    for (; *searchDirs != nullptr; ++searchDirs) {
        *outPath = std::string(*searchDirs) + "/" + fileName;
        struct stat fileStat;
        if (stat(outPath->c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
            return true;
        }
    }
    return false;
}


//	@av/media/libstagefright/xmlparser/MediaCodecsXmlParser.cpp
MediaCodecsXmlParser::MediaCodecsXmlParser(
        const char* const* searchDirs,
        const char* mainXmlName,
        const char* performanceXmlName,
        const char* profilingResultsXmlPath) :
    mParsingStatus(NO_INIT),
    mUpdate(false),
    mCodecCounter(0) {
    std::string path;
    if (findFileInDirs(searchDirs, mainXmlName, &path)) {
        parseTopLevelXMLFile(path.c_str(), false);//			/vendor/etc/media_codecs.xml
    } else {
        ALOGE("Cannot find %s", mainXmlName);
        mParsingStatus = NAME_NOT_FOUND;
    }
    if (findFileInDirs(searchDirs, performanceXmlName, &path)) {//搜索目录下文件名赋值给path
        parseTopLevelXMLFile(path.c_str(), true);//     文件为 "/vendor/etc/media_codecs_performance.xml"      
    }
    if (profilingResultsXmlPath != nullptr) {//	profilingResultsXmlPath = "/data/misc/media/media_codecs_profiling_results.xml"  //文件不存在
        parseTopLevelXMLFile(profilingResultsXmlPath, true);
    }
}



bool MediaCodecsXmlParser::parseTopLevelXMLFile(const char *codecs_xml,bool ignore_errors) {
    // get href_base
    const char *href_base_end = strrchr(codecs_xml, '/');
    if (href_base_end != nullptr) {
        mHrefBase = std::string(codecs_xml, href_base_end - codecs_xml + 1);//获取文件所在目录
    }

    mParsingStatus = OK; // keeping this here for safety
    mCurrentSection = SECTION_TOPLEVEL;

    parseXMLFile(codecs_xml);

    if (mParsingStatus != OK) {
        ALOGW("parseTopLevelXMLFile(%s) failed", codecs_xml);
        if (ignore_errors) {
            mParsingStatus = OK;
            return false;
        }
        mCodecMap.clear();
        return false;
    }
    return true;
}


void MediaCodecsXmlParser::parseXMLFile(const char *path) {
    FILE *file = fopen(path, "r");

    if (file == nullptr) {
        ALOGW("unable to open media codecs configuration xml file: %s", path);
        mParsingStatus = NAME_NOT_FOUND;
        return;
    }

    //  external/expat/lib/expat.h:25:typedef struct XML_ParserStruct *XML_Parser;
    XML_Parser parser = ::XML_ParserCreate(nullptr);//  @external/expat/lib/xmlparse.c
    LOG_FATAL_IF(parser == nullptr, "XML_MediaCodecsXmlParserCreate() failed.");

    ::XML_SetUserData(parser, this);
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




XML_Parser XMLCALL
XML_ParserCreate(const XML_Char *encodingName /*=NULL*/)
{
  return XML_ParserCreate_MM(encodingName, NULL, NULL);
}

XML_Parser XMLCALL
XML_ParserCreate_MM(const XML_Char *encodingName,
                    const XML_Memory_Handling_Suite *memsuite,
                    const XML_Char *nameSep)
{
  return parserCreate(encodingName, memsuite, nameSep, NULL);
}


static XML_Parser
parserCreate(const XML_Char *encodingName /*=NULL*/ ,
             const XML_Memory_Handling_Suite *memsuite /*=NULL*/,
             const XML_Char *nameSep /*=NULL*/,
             DTD *dtd /*=NULL*/)
{
  XML_Parser parser;

  if (memsuite) {
    XML_Memory_Handling_Suite *mtemp;
    parser = (XML_Parser) memsuite->malloc_fcn(sizeof(struct XML_ParserStruct));
    if (parser != NULL) {
      mtemp = (XML_Memory_Handling_Suite *)&(parser->m_mem);
      mtemp->malloc_fcn = memsuite->malloc_fcn;
      mtemp->realloc_fcn = memsuite->realloc_fcn;
      mtemp->free_fcn = memsuite->free_fcn;
    }
  }else {//执行这里
    XML_Memory_Handling_Suite *mtemp;
    parser = (XML_Parser)malloc(sizeof(struct XML_ParserStruct));
    if (parser != NULL) {
      mtemp = (XML_Memory_Handling_Suite *)&(parser->m_mem);
      mtemp->malloc_fcn = malloc;
      mtemp->realloc_fcn = realloc;
      mtemp->free_fcn = free;
    }
  }

  if (!parser)
    return parser;

  buffer = NULL;
  bufferLim = NULL;

  attsSize = INIT_ATTS_SIZE;    //  #define INIT_ATTS_SIZE 16
  atts = (ATTRIBUTE *)MALLOC(attsSize * sizeof(ATTRIBUTE));
  if (atts == NULL) {
    FREE(parser);
    return NULL;
  }
#ifdef XML_ATTR_INFO  //没有定义
  attInfo = (XML_AttrInfo*)MALLOC(attsSize * sizeof(XML_AttrInfo));
  if (attInfo == NULL) {
    FREE(atts);
    FREE(parser);
    return NULL;
  }
#endif
  dataBuf = (XML_Char *)MALLOC(INIT_DATA_BUF_SIZE * sizeof(XML_Char));
  if (dataBuf == NULL) {
    FREE(atts);
#ifdef XML_ATTR_INFO //没有定义
    FREE(attInfo);
#endif
    FREE(parser);
    return NULL;
  }
  dataBufEnd = dataBuf + INIT_DATA_BUF_SIZE;

  if (dtd) // NULL
    _dtd = dtd;
  else {
    _dtd = dtdCreate(&parser->m_mem);
    if (_dtd == NULL) {
      FREE(dataBuf);
      FREE(atts);
#ifdef XML_ATTR_INFO  //没有定义
      FREE(attInfo);
#endif
      FREE(parser);
      return NULL;
    }
  }

  freeBindingList = NULL;
  freeTagList = NULL;
  freeInternalEntities = NULL;

  groupSize = 0;
  groupConnector = NULL;

  unknownEncodingHandler = NULL;
  unknownEncodingHandlerData = NULL;

  namespaceSeparator = ASCII_EXCL;
  ns = XML_FALSE;
  ns_triplets = XML_FALSE;

  nsAtts = NULL;
  nsAttsVersion = 0;
  nsAttsPower = 0;

  poolInit(&tempPool, &(parser->m_mem));
  poolInit(&temp2Pool, &(parser->m_mem));
  parserInit(parser, encodingName);

  if (encodingName && !protocolEncodingName) {
    XML_ParserFree(parser);
    return NULL;
  }

  if (nameSep) { // NULL
    ns = XML_TRUE;
    internalEncoding = XmlGetInternalEncodingNS();
    namespaceSeparator = *nameSep;
  }else {
    internalEncoding = XmlGetInternalEncoding();
  }

  return parser;
}



static DTD *
dtdCreate(const XML_Memory_Handling_Suite *ms)
{
  DTD *p = (DTD *)ms->malloc_fcn(sizeof(DTD));
  if (p == NULL)
    return p;
  poolInit(&(p->pool), ms);
  poolInit(&(p->entityValuePool), ms);
  hashTableInit(&(p->generalEntities), ms);
  hashTableInit(&(p->elementTypes), ms);
  hashTableInit(&(p->attributeIds), ms);
  hashTableInit(&(p->prefixes), ms);
#ifdef XML_DTD
  p->paramEntityRead = XML_FALSE;
  hashTableInit(&(p->paramEntities), ms);
#endif /* XML_DTD */
  p->defaultPrefix.name = NULL;
  p->defaultPrefix.binding = NULL;

  p->in_eldecl = XML_FALSE;
  p->scaffIndex = NULL;
  p->scaffold = NULL;
  p->scaffLevel = 0;
  p->scaffSize = 0;
  p->scaffCount = 0;
  p->contentStringLen = 0;

  p->keepProcessing = XML_TRUE;
  p->hasParamEntityRefs = XML_FALSE;
  p->standalone = XML_FALSE;
  return p;
}





static void FASTCALL
poolInit(STRING_POOL *pool, const XML_Memory_Handling_Suite *ms)
{
  pool->blocks = NULL;
  pool->freeBlocks = NULL;
  pool->start = NULL;
  pool->ptr = NULL;
  pool->end = NULL;
  pool->mem = ms;
}

static void FASTCALL
hashTableInit(HASH_TABLE *p, const XML_Memory_Handling_Suite *ms)
{
  p->power = 0;
  p->size = 0;
  p->used = 0;
  p->v = NULL;
  p->mem = ms;
}


static void
parserInit(XML_Parser parser, const XML_Char *encodingName /*=NULL*/)
{
  processor = prologInitProcessor;
  XmlPrologStateInit(&prologState);
  protocolEncodingName = (encodingName != NULL ? poolCopyString(&tempPool, encodingName) : NULL);
  curBase = NULL;
  XmlInitEncoding(&initEncoding, &encoding, 0);
  userData = NULL;
  handlerArg = NULL;
  startElementHandler = NULL;
  endElementHandler = NULL;
  characterDataHandler = NULL;
  processingInstructionHandler = NULL;
  commentHandler = NULL;
  startCdataSectionHandler = NULL;
  endCdataSectionHandler = NULL;
  defaultHandler = NULL;
  startDoctypeDeclHandler = NULL;
  endDoctypeDeclHandler = NULL;
  unparsedEntityDeclHandler = NULL;
  notationDeclHandler = NULL;
  startNamespaceDeclHandler = NULL;
  endNamespaceDeclHandler = NULL;
  notStandaloneHandler = NULL;
  externalEntityRefHandler = NULL;
  externalEntityRefHandlerArg = parser;
  skippedEntityHandler = NULL;
  elementDeclHandler = NULL;
  attlistDeclHandler = NULL;
  entityDeclHandler = NULL;
  xmlDeclHandler = NULL;
  bufferPtr = buffer;
  bufferEnd = buffer;
  parseEndByteIndex = 0;
  parseEndPtr = NULL;
  declElementType = NULL;
  declAttributeId = NULL;
  declEntity = NULL;
  doctypeName = NULL;
  doctypeSysid = NULL;
  doctypePubid = NULL;
  declAttributeType = NULL;
  declNotationName = NULL;
  declNotationPublicId = NULL;
  declAttributeIsCdata = XML_FALSE;
  declAttributeIsId = XML_FALSE;
  memset(&position, 0, sizeof(POSITION));
  errorCode = XML_ERROR_NONE;
  eventPtr = NULL;
  eventEndPtr = NULL;
  positionPtr = NULL;
  openInternalEntities = NULL;
  defaultExpandInternalEntities = XML_TRUE;
  tagLevel = 0;
  tagStack = NULL;
  inheritedBindings = NULL;
  nSpecifiedAtts = 0;
  unknownEncodingMem = NULL;
  unknownEncodingRelease = NULL;
  unknownEncodingData = NULL;
  parentParser = NULL;
  ps_parsing = XML_INITIALIZED;
#ifdef XML_DTD
  isParamEntity = XML_FALSE;
  useForeignDTD = XML_FALSE;
  paramEntityParsing = XML_PARAM_ENTITY_PARSING_NEVER;
#endif
  hash_secret_salt = 0;
}















void XMLCALL
XML_SetUserData(XML_Parser parser, void *p)
{
  if (handlerArg == userData) //多为NULL
    handlerArg = userData = p;
  else
    userData = p;
}




void XMLCALL
XML_SetElementHandler(XML_Parser parser, XML_StartElementHandler start,XML_EndElementHandler end)
{
  startElementHandler = start;
  endElementHandler = end;
}




// static
void MediaCodecsXmlParser::StartElementHandlerWrapper(
        void *me, const char *name, const char **attrs) {
    static_cast<MediaCodecsXmlParser*>(me)->startElementHandler(name, attrs);
}

// static
void MediaCodecsXmlParser::EndElementHandlerWrapper(void *me, const char *name) {
    static_cast<MediaCodecsXmlParser*>(me)->endElementHandler(name);
}


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

        case SECTION_DECODERS://解码
        {
            if (strEq(name, "MediaCodec")) {
                mParsingStatus = addMediaCodecFromAttributes(false /* encoder */, attrs);

                mCurrentSection = SECTION_DECODER;
            }
            break;
        }

        case SECTION_ENCODERS://编码
        {
            if (strEq(name, "MediaCodec")) {
                mParsingStatus = addMediaCodecFromAttributes(true /* encoder */, attrs);

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
                mParsingStatus = addTypeFromAttributes(attrs,(mCurrentSection == SECTION_ENCODER));
                mCurrentSection =(mCurrentSection == SECTION_DECODER ?SECTION_DECODER_TYPE : SECTION_ENCODER_TYPE);
            }
        }
        inType = false;
        // fall through

        case SECTION_DECODER_TYPE:
        case SECTION_ENCODER_TYPE:
        {
            // ignore limits and features specified outside of type
            bool outside = !inType &&  mCurrentType == mCurrentCodec->second.typeMap.end();
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
                mCurrentSection = (mCurrentSection == SECTION_DECODER_TYPE ? SECTION_DECODER : SECTION_ENCODER);

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

bool parseBoolean(const char* s) {
    return striEq(s, "y") ||
            striEq(s, "yes") ||
            striEq(s, "t") ||
            striEq(s, "true") ||
            striEq(s, "1");
}


// encoder 解码为true，编码为 false
status_t MediaCodecsXmlParser::addMediaCodecFromAttributes(bool encoder, const char **attrs) {
    const char *name = nullptr;
    const char *type = nullptr;
    const char *update = nullptr;

    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strEq(attrs[i], "name")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addMediaCodecFromAttributes: name is null");
                return -EINVAL;
            }
            name = attrs[i];
        } else if (strEq(attrs[i], "type")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addMediaCodecFromAttributes: type is null");
                return -EINVAL;
            }
            type = attrs[i];
        } else if (strEq(attrs[i], "update")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addMediaCodecFromAttributes: update is null");
                return -EINVAL;
            }
            update = attrs[i];
        } else {
            ALOGE("addMediaCodecFromAttributes: unrecognized attribute: %s", attrs[i]);
            return -EINVAL;
        }
        ++i;
    }

    if (name == nullptr) {
        ALOGE("addMediaCodecFromAttributes: name not found");
        return -EINVAL;
    }

    mUpdate = (update != nullptr) && parseBoolean(update);
    mCurrentCodec = mCodecMap.find(name);//查看是否已经添加
    if (mCurrentCodec == mCodecMap.end()) { // New codec name
        if (mUpdate) {
            ALOGE("addMediaCodecFromAttributes: updating non-existing codec");
            return -EINVAL;
        }
        // Create a new codec in mCodecMap

        /**
        struct CodecProperties {
            bool isEncoder;    ///< Whether this codec is an encoder or a decoder
            size_t order;      ///< Order of appearance in the file (starting from 0)
            QuirkSet quirkSet; ///< Set of quirks requested by this codec
            TypeMap typeMap;   ///< Map of types supported by this codec
        };
        */

        //  CodecMap::iterator mCurrentCodec;
        mCurrentCodec = mCodecMap.insert(Codec(name, CodecProperties())).first; //typedef std::map<std::string, CodecProperties> CodecMap;
        if (type != nullptr) {
            //  TypeMap::iterator mCurrentType;
            //  typedef std::map<std::string, std::string> AttributeMap;
            mCurrentType = mCurrentCodec->second.typeMap.insert(Type(type, AttributeMap())).first; //   typedef std::pair<std::string, AttributeMap> Type;
        } else {
            mCurrentType = mCurrentCodec->second.typeMap.end();
        }
        mCurrentCodec->second.isEncoder = encoder;
        mCurrentCodec->second.order = mCodecCounter++;
    } else { // Existing codec name
        if (!mUpdate) {
            ALOGE("addMediaCodecFromAttributes: adding existing codec");
            return -EINVAL;
        }
        if (type != nullptr) {
            mCurrentType = mCurrentCodec->second.typeMap.find(type);
            if (mCurrentType == mCurrentCodec->second.typeMap.end()) {
                ALOGE("addMediaCodecFromAttributes: updating non-existing type");
                return -EINVAL;
            }
        } else {
            // This should happen only when the codec has at most one type.
            mCurrentType = mCurrentCodec->second.typeMap.begin();
        }
    }

    return OK;
}




status_t MediaCodecsXmlParser::addLimit(const char **attrs) {
    const char* a_name = nullptr;
    const char* a_default = nullptr;
    const char* a_in = nullptr;
    const char* a_max = nullptr;
    const char* a_min = nullptr;
    const char* a_range = nullptr;
    const char* a_ranges = nullptr;
    const char* a_scale = nullptr;
    const char* a_value = nullptr;

    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strEq(attrs[i], "name")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: name is null");
                return -EINVAL;
            }
            a_name = attrs[i];
        } else if (strEq(attrs[i], "default")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: default is null");
                return -EINVAL;
            }
            a_default = attrs[i];
        } else if (strEq(attrs[i], "in")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: in is null");
                return -EINVAL;
            }
            a_in = attrs[i];
        } else if (strEq(attrs[i], "max")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: max is null");
                return -EINVAL;
            }
            a_max = attrs[i];
        } else if (strEq(attrs[i], "min")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: min is null");
                return -EINVAL;
            }
            a_min = attrs[i];
        } else if (strEq(attrs[i], "range")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: range is null");
                return -EINVAL;
            }
            a_range = attrs[i];
        } else if (strEq(attrs[i], "ranges")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: ranges is null");
                return -EINVAL;
            }
            a_ranges = attrs[i];
        } else if (strEq(attrs[i], "scale")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: scale is null");
                return -EINVAL;
            }
            a_scale = attrs[i];
        } else if (strEq(attrs[i], "value")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addLimit: value is null");
                return -EINVAL;
            }
            a_value = attrs[i];
        } else {
            ALOGE("addLimit: unrecognized limit: %s", attrs[i]);
            return -EINVAL;
        }
        ++i;
    }

    if (a_name == nullptr) {
        ALOGE("limit with no 'name' attribute");
        return -EINVAL;
    }

    // size, blocks, bitrate, frame-rate, blocks-per-second, aspect-ratio,
    // measured-frame-rate, measured-blocks-per-second: range
    // quality: range + default + [scale]
    // complexity: range + default
    if (mCurrentType == mCurrentCodec->second.typeMap.end()) {
        ALOGW("ignoring null type");
        return OK;
    }

    std::string range;
    if (strEq(a_name, "aspect-ratio") ||
            strEq(a_name, "bitrate") ||
            strEq(a_name, "block-count") ||
            strEq(a_name, "blocks-per-second") ||
            strEq(a_name, "complexity") ||
            strEq(a_name, "frame-rate") ||
            strEq(a_name, "quality") ||
            strEq(a_name, "size") ||
            strEq(a_name, "measured-blocks-per-second") ||
            strHasPrefix(a_name, "measured-frame-rate-")) {
        // "range" is specified in exactly one of the following forms:
        // 1) min-max
        // 2) value-value
        // 3) range
        if (a_min != nullptr && a_max != nullptr) {
            // min-max
            if (a_range != nullptr || a_value != nullptr) {
                return limitError(a_name, "has 'min' and 'max' as well as 'range' or "
                        "'value' attributes");
            }
            range = a_min;
            range += '-';
            range += a_max;
        } else if (a_min != nullptr || a_max != nullptr) {
            return limitError(a_name, "has only 'min' or 'max' attribute");
        } else if (a_value != nullptr) {
            // value-value
            if (a_range != nullptr) {
                return limitError(a_name, "has both 'range' and 'value' attributes");
            }
            range = a_value;
            range += '-';
            range += a_value;
        } else if (a_range == nullptr) {
            return limitError(a_name, "with no 'range', 'value' or 'min'/'max' attributes");
        } else {
            // range
            range = a_range;
        }

        // "aspect-ratio" requires some special treatment.
        if (strEq(a_name, "aspect-ratio")) {
            // "aspect-ratio" must have "in".
            if (a_in == nullptr) {
                return limitFoundMissingAttr(a_name, "in", false);
            }
            // "in" must be either "pixels" or "blocks".
            if (!strEq(a_in, "pixels") && !strEq(a_in, "blocks")) {
                return limitInvalidAttr(a_name, "in", a_in);
            }
            // name will be "pixel-aspect-ratio-range" or
            // "block-aspect-ratio-range".
            mCurrentType->second[
                    std::string(a_in).substr(0, strlen(a_in) - 1) +
                    "-aspect-ratio-range"] = range;
        } else {
            // For everything else (apart from "aspect-ratio"), simply append
            // "-range" to the name for the range-type property.
            mCurrentType->second[std::string(a_name) + "-range"] = range;

            // Only "quality" may have "scale".
            if (!strEq(a_name, "quality") && a_scale != nullptr) {
                return limitFoundMissingAttr(a_name, "scale");
            } else if (strEq(a_name, "quality")) {
                // The default value of "quality-scale" is "linear".
                mCurrentType->second["quality-scale"] = a_scale == nullptr ?
                        "linear" : a_scale;
            }

            // "quality" and "complexity" must have "default".
            // Other limits must not have "default".
            if (strEq(a_name, "quality") || strEq(a_name, "complexity")) {
                if (a_default == nullptr) {
                    return limitFoundMissingAttr(a_name, "default", false);
                }
                // name will be "quality-default" or "complexity-default".
                mCurrentType->second[std::string(a_name) + "-default"] = a_default;
            } else if (a_default != nullptr) {
                return limitFoundMissingAttr(a_name, "default", true);
            }
        }
    } else {
        if (a_default != nullptr) {
            return limitFoundMissingAttr(a_name, "default");
        }
        if (a_in != nullptr) {
            return limitFoundMissingAttr(a_name, "in");
        }
        if (a_scale != nullptr) {
            return limitFoundMissingAttr(a_name, "scale");
        }
        if (a_range != nullptr) {
            return limitFoundMissingAttr(a_name, "range");
        }
        if (a_min != nullptr) {
            return limitFoundMissingAttr(a_name, "min");
        }

        if (a_max != nullptr) {
            // "max" must exist if and only if name is "channel-count" or
            // "concurrent-instances".
            // "min" is not ncessary.
            if (strEq(a_name, "channel-count") || strEq(a_name, "concurrent-instances")) {
                mCurrentType->second[std::string("max-") + a_name] = a_max;
            } else {
                return limitFoundMissingAttr(a_name, "max", false);
            }
        } else if (strEq(a_name, "channel-count") || strEq(a_name, "concurrent-instances")) {
            return limitFoundMissingAttr(a_name, "max");
        }

        if (a_ranges != nullptr) {
            // "ranges" must exist if and only if name is "sample-rate".
            if (strEq(a_name, "sample-rate")) {
                mCurrentType->second["sample-rate-ranges"] = a_ranges;
            } else {
                return limitFoundMissingAttr(a_name, "ranges", false);
            }
        } else if (strEq(a_name, "sample-rate")) {
            return limitFoundMissingAttr(a_name, "ranges");
        }

        if (a_value != nullptr) {
            // "value" must exist if and only if name is "alignment" or
            // "block-size".
            if (strEq(a_name, "alignment") || strEq(a_name, "block-size")) {
                mCurrentType->second[a_name] = a_value;
            } else {
                return limitFoundMissingAttr(a_name, "value", false);
            }
        } else if (strEq(a_name, "alignment") || strEq(a_name, "block-size")) {
            return limitFoundMissingAttr(a_name, "value", false);
        }

    }

    return OK;
}

status_t MediaCodecsXmlParser::addFeature(const char **attrs) {
    size_t i = 0;
    const char *name = nullptr;
    int32_t optional = -1;
    int32_t required = -1;
    const char *value = nullptr;

    while (attrs[i] != nullptr) {
        if (strEq(attrs[i], "name")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addFeature: name is null");
                return -EINVAL;
            }
            name = attrs[i];
        } else if (strEq(attrs[i], "optional")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addFeature: optional is null");
                return -EINVAL;
            }
            optional = parseBoolean(attrs[i]) ? 1 : 0;
        } else if (strEq(attrs[i], "required")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addFeature: required is null");
                return -EINVAL;
            }
            required = parseBoolean(attrs[i]) ? 1 : 0;
        } else if (strEq(attrs[i], "value")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addFeature: value is null");
                return -EINVAL;
            }
            value = attrs[i];
        } else {
            ALOGE("addFeature: unrecognized attribute: %s", attrs[i]);
            return -EINVAL;
        }
        ++i;
    }

    // Every feature must have a name.
    if (name == nullptr) {
        ALOGE("feature with no 'name' attribute");
        return -EINVAL;
    }

    if (mCurrentType == mCurrentCodec->second.typeMap.end()) {
        ALOGW("ignoring null type");
        return OK;
    }

    if ((optional != -1) || (required != -1)) {
        if (optional == required) {
            ALOGE("feature '%s' is both/neither optional and required", name);
            return -EINVAL;
        }
        if ((optional == 1) || (required == 1)) {
            if (value != nullptr) {
                ALOGE("feature '%s' cannot have extra 'value'", name);
                return -EINVAL;
            }
            mCurrentType->second[std::string("feature-") + name] =
                    optional == 1 ? "0" : "1";
            return OK;
        }
    }
    mCurrentType->second[std::string("feature-") + name] = value == nullptr ?
            "0" : value;
    return OK;
}





status_t MediaCodecsXmlParser::addSettingFromAttributes(const char **attrs) {
    const char *name = nullptr;
    const char *value = nullptr;
    const char *update = nullptr;

    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strEq(attrs[i], "name")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addSettingFromAttributes: name is null");
                return -EINVAL;
            }
            name = attrs[i];
        } else if (strEq(attrs[i], "value")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addSettingFromAttributes: value is null");
                return -EINVAL;
            }
            value = attrs[i];
        } else if (strEq(attrs[i], "update")) {
            if (attrs[++i] == nullptr) {
                ALOGE("addSettingFromAttributes: update is null");
                return -EINVAL;
            }
            update = attrs[i];
        } else {
            ALOGE("addSettingFromAttributes: unrecognized attribute: %s", attrs[i]);
            return -EINVAL;
        }
        ++i;
    }

    if (name == nullptr || value == nullptr) {
        ALOGE("addSettingFromAttributes: name or value unspecified");
        return -EINVAL;
    }

    // Boolean values are converted to "0" or "1".
    if (strHasPrefix(name, "supports-")) {
        value = parseBoolean(value) ? "1" : "0";
    }

    mUpdate = (update != nullptr) && parseBoolean(update);
    //  typedef std::map<std::string, std::string> AttributeMap;
    auto attribute = mServiceAttributeMap.find(name);      //      AttributeMap mServiceAttributeMap;
    if (attribute == mServiceAttributeMap.end()) { // New attribute name
        if (mUpdate) {
            ALOGE("addSettingFromAttributes: updating non-existing setting");
            return -EINVAL;
        }
        mServiceAttributeMap.insert(Attribute(name, value));//  typedef std::pair<std::string, std::string> Attribute;
    } else { // Existing attribute name
        if (!mUpdate) {
            ALOGE("addSettingFromAttributes: adding existing setting");
        }
        attribute->second = value;
    }

    return OK;
}





status_t MediaCodecsXmlParser::includeXMLFile(const char **attrs) {
    const char *href = nullptr;
    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strEq(attrs[i], "href")) {
            if (attrs[++i] == nullptr) {
                return -EINVAL;
            }
            href = attrs[i];
        } else {
            ALOGE("includeXMLFile: unrecognized attribute: %s", attrs[i]);
            return -EINVAL;
        }
        ++i;
    }

    // For security reasons and for simplicity, file names can only contain
    // [a-zA-Z0-9_.] and must start with  media_codecs_ and end with .xml
    for (i = 0; href[i] != '\0'; i++) {
        if (href[i] == '.' || href[i] == '_' ||
                (href[i] >= '0' && href[i] <= '9') ||
                (href[i] >= 'A' && href[i] <= 'Z') ||
                (href[i] >= 'a' && href[i] <= 'z')) {
            continue;
        }
        ALOGE("invalid include file name: %s", href);
        return -EINVAL;
    }

    std::string filename = href;
    if (filename.compare(0, 13, "media_codecs_") != 0 || filename.compare(filename.size() - 4, 4, ".xml") != 0) {
        ALOGE("invalid include file name: %s", href);
        return -EINVAL;
    }
    filename.insert(0, mHrefBase);//必命名以“media_codecs_”开头的.xml文件，而且必须在同一级目录

    parseXMLFile(filename.c_str());
    return mParsingStatus;
}














void * XMLCALL
XML_GetBuffer(XML_Parser parser, int len)
{
  if (len < 0) {
    errorCode = XML_ERROR_NO_MEMORY;
    return NULL;
  }
  switch (ps_parsing) {//   ps_parsing = XML_INITIALIZED
  case XML_SUSPENDED:
    errorCode = XML_ERROR_SUSPENDED;
    return NULL;
  case XML_FINISHED:
    errorCode = XML_ERROR_FINISHED;
    return NULL;
  default: ;
  }

  if (len > bufferLim - bufferEnd) {
#ifdef XML_CONTEXT_BYTES
    int keep;
#endif  /* defined XML_CONTEXT_BYTES */
    /* Do not invoke signed arithmetic overflow: */
    int neededSize = (int) ((unsigned)len + (unsigned)(bufferEnd - bufferPtr));
    if (neededSize < 0) {
      errorCode = XML_ERROR_NO_MEMORY;
      return NULL;
    }
#ifdef XML_CONTEXT_BYTES
    keep = (int)(bufferPtr - buffer);
    if (keep > XML_CONTEXT_BYTES)
      keep = XML_CONTEXT_BYTES;
    neededSize += keep;
#endif  /* defined XML_CONTEXT_BYTES */
    if (neededSize  <= bufferLim - buffer) {
#ifdef XML_CONTEXT_BYTES
      if (keep < bufferPtr - buffer) {
        int offset = (int)(bufferPtr - buffer) - keep;
        memmove(buffer, &buffer[offset], bufferEnd - bufferPtr + keep);
        bufferEnd -= offset;
        bufferPtr -= offset;
      }
#else
      memmove(buffer, bufferPtr, bufferEnd - bufferPtr);
      bufferEnd = buffer + (bufferEnd - bufferPtr);
      bufferPtr = buffer;
#endif  /* not defined XML_CONTEXT_BYTES */
    }else {
      char *newBuf;
      int bufferSize = (int)(bufferLim - bufferPtr);
      if (bufferSize == 0)
        bufferSize = INIT_BUFFER_SIZE;
      do {
        /* Do not invoke signed arithmetic overflow: */
        bufferSize = (int) (2U * (unsigned) bufferSize);
      } while (bufferSize < neededSize && bufferSize > 0);
      if (bufferSize <= 0) {
        errorCode = XML_ERROR_NO_MEMORY;
        return NULL;
      }
      newBuf = (char *)MALLOC(bufferSize);
      if (newBuf == 0) {
        errorCode = XML_ERROR_NO_MEMORY;
        return NULL;
      }
      bufferLim = newBuf + bufferSize;
#ifdef XML_CONTEXT_BYTES
      if (bufferPtr) {
        int keep = (int)(bufferPtr - buffer);
        if (keep > XML_CONTEXT_BYTES)
          keep = XML_CONTEXT_BYTES;
        memcpy(newBuf, &bufferPtr[-keep], bufferEnd - bufferPtr + keep);
        FREE(buffer);
        buffer = newBuf;
        bufferEnd = buffer + (bufferEnd - bufferPtr) + keep;
        bufferPtr = buffer + keep;
      }
      else {
        bufferEnd = newBuf + (bufferEnd - bufferPtr);
        bufferPtr = buffer = newBuf;
      }
#else
      if (bufferPtr) {
        memcpy(newBuf, bufferPtr, bufferEnd - bufferPtr);
        FREE(buffer);
      }
      bufferEnd = newBuf + (bufferEnd - bufferPtr);
      bufferPtr = buffer = newBuf;
#endif  /* not defined XML_CONTEXT_BYTES */
    }
    eventPtr = eventEndPtr = NULL;
    positionPtr = NULL;
  }
  return bufferEnd;
}












enum XML_Status XMLCALL
XML_ParseBuffer(XML_Parser parser, int len, int isFinal)
{
  const char *start;
  enum XML_Status result = XML_STATUS_OK;

  switch (ps_parsing) {
  case XML_SUSPENDED:
    errorCode = XML_ERROR_SUSPENDED;
    return XML_STATUS_ERROR;
  case XML_FINISHED:
    errorCode = XML_ERROR_FINISHED;
    return XML_STATUS_ERROR;
  case XML_INITIALIZED:
    if (parentParser == NULL && !startParsing(parser)) {
      errorCode = XML_ERROR_NO_MEMORY;
      return XML_STATUS_ERROR;
    }
  default:
    ps_parsing = XML_PARSING;
  }

  start = bufferPtr;
  positionPtr = start;
  bufferEnd += len;
  parseEndPtr = bufferEnd;
  parseEndByteIndex += len;
  ps_finalBuffer = (XML_Bool)isFinal;

  errorCode = processor(parser, start, parseEndPtr, &bufferPtr);

  if (errorCode != XML_ERROR_NONE) {
    eventEndPtr = eventPtr;
    processor = errorProcessor;
    return XML_STATUS_ERROR;
  }else {
    switch (ps_parsing) {
    case XML_SUSPENDED:
      result = XML_STATUS_SUSPENDED;
      break;
    case XML_INITIALIZED:
    case XML_PARSING:
      if (isFinal) {
        ps_parsing = XML_FINISHED;
        return result;
      }
    default: ;  /* should not happen */
    }
  }

  XmlUpdatePosition(encoding, positionPtr, bufferPtr, &position);
  positionPtr = bufferPtr;
  return result;
}