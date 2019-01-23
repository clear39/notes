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




//  @ frameworks/av/media/libstagefright/omx/1.0/OmxStore.cpp
struct OmxStore : public IOmxStore {
    OmxStore(
        const char* owner = "default",
        const char* const* searchDirs = MediaCodecsXmlParser::defaultSearchDirs,
        const char* mainXmlName = MediaCodecsXmlParser::defaultMainXmlName,
        const char* performanceXmlName = MediaCodecsXmlParser::defaultPerformanceXmlName,
        const char* profilingResultsXmlPath = MediaCodecsXmlParser::defaultProfilingResultsXmlPath //该文件存在
        ); 

    
}