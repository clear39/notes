
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
        parseTopLevelXMLFile(path.c_str(), false);//			media_codecs.xml
    } else {
        ALOGE("Cannot find %s", mainXmlName);
        mParsingStatus = NAME_NOT_FOUND;
    }
    if (findFileInDirs(searchDirs, performanceXmlName, &path)) {//		"media_codecs_performance.xml"		
        parseTopLevelXMLFile(path.c_str(), true);
    }
    if (profilingResultsXmlPath != nullptr) {//	profilingResultsXmlPath = "/data/misc/media/media_codecs_profiling_results.xml"
        parseTopLevelXMLFile(profilingResultsXmlPath, true);
    }
}



bool MediaCodecsXmlParser::parseTopLevelXMLFile(const char *codecs_xml,bool ignore_errors) {
    // get href_base
    const char *href_base_end = strrchr(codecs_xml, '/');
    if (href_base_end != nullptr) {
        mHrefBase = std::string(codecs_xml, href_base_end - codecs_xml + 1);
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

    XML_Parser parser = ::XML_ParserCreate(nullptr);
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
