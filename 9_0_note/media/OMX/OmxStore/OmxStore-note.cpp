/**
xqli@linux:/work/workcodes/aosp-p9.x-auto-alpha$ find frameworks/av vendor/nxp -type f -name media_codecs_performance.xml
vendor/nxp/fsl-proprietary/media-profile/imx8mm/media_codecs_performance.xml
vendor/nxp/fsl-proprietary/media-profile/imx8dq/media_codecs_performance.xml
vendor/nxp/fsl-proprietary/media-profile/imx8mq/media_codecs_performance.xml
vendor/nxp/fsl-proprietary/media-profile/imx8q/media_codecs_performance.xml
vendor/nxp/fsl-proprietary/media-profile/media_codecs_performance.xml
xqli@linux:/work/workcodes/aosp-p9.x-auto-alpha$ find frameworks/av vendor/nxp -type f -name media_codecs.xml
vendor/nxp/fsl-proprietary/media-profile/imx8mm/media_codecs.xml
vendor/nxp/fsl-proprietary/media-profile/imx8dq/media_codecs.xml
vendor/nxp/fsl-proprietary/media-profile/imx8mq/media_codecs.xml
vendor/nxp/fsl-proprietary/media-profile/imx8q/media_codecs.xml
vendor/nxp/fsl-proprietary/media-profile/media_codecs.xml
 */ 


//  @ frameworks/av/media/libstagefright/omx/1.0/OmxStore.cpp
struct OmxStore : public IOmxStore {
    OmxStore(
        const char* owner = "default",
        const char* const* searchDirs = MediaCodecsXmlParser::defaultSearchDirs,
        const char* mainXmlName = MediaCodecsXmlParser::defaultMainXmlName,
        const char* performanceXmlName = MediaCodecsXmlParser::defaultPerformanceXmlName,
        const char* profilingResultsXmlPath = MediaCodecsXmlParser::defaultProfilingResultsXmlPath //该文件存在
        );

    /**
     *  @   hardware/interfaces/media/omx/1.0/IOmxStore.hal
     *  struct Attribute {
            string key;
            string value;
        };
     * typedef Attribute ServiceAttribute;
     */ 
    hidl_vec<ServiceAttribute> mServiceAttributeList;
    //调用接口，获取 MediaCodecsXmlParser 解析的 xml文件中 Settings 标签属性集合
    Return<void> OmxStore::listServiceAttributes(listServiceAttributes_cb _hidl_cb)；


    /**
     * @   hardware/interfaces/media/omx/1.0/IOmxStore.hal
     * 
     * struct RoleInfo {
        string role;
        string type;
        bool isEncoder;
        bool preferPlatformNodes;
        vec<NodeInfo> nodes;
       };

     *
     * typedef Attribute NodeAttribute;
     * 
     * struct NodeInfo {
        string name;
        string owner;
        vec<NodeAttribute> attributes;
       };
     */ 
    hidl_vec<RoleInfo> mRoleList;
    Return<void> listRoles(listRoles_cb) override;


    /**
     * 获取所有支持的组件的前缀 这里为 OMX.
     */ 
    hidl_string mPrefix;
    Return<void> getNodePrefix(getNodePrefix_cb) override;



}


//  @