//	@/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/common/managerdefinitions/src/Serializer.cpp

status_t PolicySerializer::deserialize(const char *configFile, AudioPolicyConfig &config)
{
    xmlDocPtr doc;
    doc = xmlParseFile(configFile);
    if (doc == NULL) {
        ALOGE("%s: Could not parse %s document.", __FUNCTION__, configFile);
        return BAD_VALUE;
    }
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        ALOGE("%s: Could not parse %s document: empty.", __FUNCTION__, configFile);
        xmlFreeDoc(doc);
        return BAD_VALUE;
    }
    if (xmlXIncludeProcess(doc) < 0) {
         ALOGE("%s: libxml failed to resolve XIncludes on %s document.", __FUNCTION__, configFile);
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) mRootElementName.c_str()))  {
        ALOGE("%s: No %s root element found in xml data %s.", __FUNCTION__, mRootElementName.c_str(),(const char *)cur->name);
        xmlFreeDoc(doc);
        return BAD_VALUE;
    }

    string version = getXmlAttribute(cur, versionAttribute);//	version="1.0"
    if (version.empty()) {
        ALOGE("%s: No version found in root node %s", __FUNCTION__, mRootElementName.c_str());
        return BAD_VALUE;
    }
    /***
     * const uint32_t PolicySerializer::gMajor = 1;
     * const uint32_t PolicySerializer::gMinor = 0;
     * mVersion 在 PolicySerializer 构造方法中 通过 gMajor 和 gMinor 构建而成的 其值为"1.0"，所以是相等的
     * */
    if (version != mVersion) {
        ALOGE("%s: Version does not match; expect %s got %s", __FUNCTION__, mVersion.c_str(),version.c_str());
        return BAD_VALUE;
    }

     /***
     * 解析所有标签 modules 下的子标签 module
     * 
     * 
     * */
    // Lets deserialize children
    // Modules
    ModuleTraits::Collection modules; // @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/common/managerdefinitions/include/Serializer.h
    deserializeCollection<ModuleTraits>(doc, cur, modules, &config);
    config.setHwModules(modules);


    /***
     * 解析所有标签 volumes 下的子标签 volume
     * 
     * 
     * 
     * */
    // deserialize volume section
    VolumeTraits::Collection volumes;
    deserializeCollection<VolumeTraits>(doc, cur, volumes, &config);
    config.setVolumes(volumes);



    /***
     * 该标签和 modules 和 volumes 是同一级标签
     * 这里解析标签tag为 globalConfiguration，其包含属性名称为 speaker_drc_enabled（其值为false或者true），
     * 通过AudioPolicyConfig 的 setSpeakerDrcEnabled 方法设置该属性值
     * */
    // Global Configuration
    GlobalConfigTraits::deserialize(cur, config);

    xmlFreeDoc(doc);
    return android::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

/***
 *  
    ModuleTraits::Collection modules;									//	typedef HwModuleCollection Collection;
    deserializeCollection<ModuleTraits>(doc, cur, modules, &config);
    config.setHwModules(modules);
*/

/*
解析的格式为：
    <modules>
        <module name="primary" halVersion="2.0">
        ......
        </module>
        <module name="a2dp" halVersion="2.0">
        ......
        </module>
        <module name="usb" halVersion="2.0">
        ......
        </module>
        <module name="r_submix" halVersion="2.0">
        ......
        </module>
    </modules>
*/
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,typename ModuleTraits::Collection &collection,typename ModuleTraits::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
    	//	const char *const ModuleTraits::collectionTag = "modules";
    	//	const char *const ModuleTraits::tag = "module";
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) &&  xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {//	const char *const ModuleTraits::collectionTag = "modules";
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {//const char *const ModuleTraits::tag = "module"
                /***
                 * typename ModuleTraits::sp<HwModule> element;
                */
                typename Trait::PtrElement element;	//	 typedef HwModule Element; 		typedef sp<Element> PtrElement;
                //  ModuleTraits::deserialize(doc, child, element, serializingContext)
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status != NO_ERROR) {
                    return status;
                }
                if (collection.add(element) < 0) {//将HwModule添加到集合
                    ALOGE("%s: could not add element to %s collection", __FUNCTION__,Trait::collectionTag);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}

/*
    <module name="primary" halVersion="2.0">
    ......
    </module>
*/
//status_t ModuleTraits::deserialize(xmlDocPtr doc, const xmlNode *root, sp<HwModule> &module,PtrSerializingCtx ctx)
status_t ModuleTraits::deserialize(xmlDocPtr doc, const xmlNode *root, PtrElement &module,PtrSerializingCtx ctx)
{
    string name = getXmlAttribute(root, Attributes::name);//	const char ModuleTraits::Attributes::name[] = "name";
    if (name.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::name);
        return BAD_VALUE;
    }
    uint32_t versionMajor = 0, versionMinor = 0;
    string versionLiteral = getXmlAttribute(root, Attributes::version);//	const char ModuleTraits::Attributes::version[] = "halVersion";
    if (!versionLiteral.empty()) {
        sscanf(versionLiteral.c_str(), "%u.%u", &versionMajor, &versionMinor);//  2.0
        ALOGV("%s: mHalVersion = major %u minor %u",  __FUNCTION__,versionMajor, versionMajor);
    }

    ALOGV("%s: %s %s=%s", __FUNCTION__, tag, Attributes::name, name.c_str());

    // module = new HwModule(name.c_str(), versionMajor, versionMinor);
    module = new Element(name.c_str(), versionMajor, versionMinor);//	typedef HwModule Element;

    // Deserialize childrens: Audio Mix Port, Audio Device Ports (Source/Sink), Audio Routes
    //  frameworks/av/services/audiopolicy/common/managerdefinitions/include/HwModule.h:37:typedef Vector<sp<IOProfile> > IOProfileCollection;
    //  frameworks/av/services/audiopolicy/common/managerdefinitions/include/Serializer.h:99:    typedef IOProfileCollection Collection;
    MixPortTraits::Collection mixPorts;
    deserializeCollection<MixPortTraits>(doc, root, mixPorts, NULL);
    module->setProfiles(mixPorts);

    DevicePortTraits::Collection devicePorts;
    deserializeCollection<DevicePortTraits>(doc, root, devicePorts, NULL);
    module->setDeclaredDevices(devicePorts);

    RouteTraits::Collection routes;
    deserializeCollection<RouteTraits>(doc, root, routes, module.get());
    module->setRoutes(routes);

    /***
     * 
            <attachedDevices>
                <item>Speaker</item>
                <item>Built-In Mic</item>
            </attachedDevices>
            <defaultOutputDevice>Speaker</defaultOutputDevice>

     * */
    const xmlNode *children = root->xmlChildrenNode;
    while (children != NULL) {
        if (!xmlStrcmp(children->name, (const xmlChar *)childAttachedDevicesTag)) {//const char *const ModuleTraits::childAttachedDevicesTag = "attachedDevices";
            ALOGV("%s: %s %s found", __FUNCTION__, tag, childAttachedDevicesTag);
            const xmlNode *child = children->xmlChildrenNode;
            while (child != NULL) {
                if (!xmlStrcmp(child->name, (const xmlChar *)childAttachedDeviceTag)) {//	const char *const ModuleTraits::childAttachedDeviceTag = "item";
                    xmlChar *attachedDevice = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                    if (attachedDevice != NULL) {
                        ALOGV("%s: %s %s=%s", __FUNCTION__, tag, childAttachedDeviceTag, (const char*)attachedDevice);
                        //这里通过名称查找对应的声明设备，然后设置到 AudioPolicyConfig 中
                        sp<DeviceDescriptor> device = module->getDeclaredDevices().getDeviceFromTagName(String8((const char*)attachedDevice));
                        ctx->addAvailableDevice(device);
                        xmlFree(attachedDevice);
                    }
                }
                child = child->next;
            }
        }
        if (!xmlStrcmp(children->name, (const xmlChar *)childDefaultOutputDeviceTag)) {//const char *const ModuleTraits::childDefaultOutputDeviceTag = "defaultOutputDevice";
            xmlChar *defaultOutputDevice = xmlNodeListGetString(doc, children->xmlChildrenNode, 1);;
            if (defaultOutputDevice != NULL) {
                ALOGV("%s: %s %s=%s", __FUNCTION__, tag, childDefaultOutputDeviceTag,(const char*)defaultOutputDevice);
                //这里通过名称查找对应的声明设备，然后设置到 AudioPolicyConfig 中
                sp<DeviceDescriptor> device = module->getDeclaredDevices().getDeviceFromTagName(String8((const char*)defaultOutputDevice));
                if (device != 0 && ctx->getDefaultOutputDevice() == 0) {
                    //设置默认输出设备
                    ctx->setDefaultOutputDevice(device);
                    ALOGV("%s: default is %08x", __FUNCTION__, ctx->getDefaultOutputDevice()->type());
                }
                xmlFree(defaultOutputDevice);
            }
        }
        children = children->next;
    }
    return NO_ERROR;
}


/***
    MixPortTraits::Collection mixPorts;
    // 将 IOProfile 成员添加到 mixPorts 列表上
    deserializeCollection<MixPortTraits>(doc, root, mixPorts, NULL);
    module->setProfiles(mixPorts);

    解析格式：
    <module name="primary" halVersion="2.0">
         。。。。。。
        <mixPorts>
            <mixPort name="primary output" role="source" flags="AUDIO_OUTPUT_FLAG_PRIMARY">
                <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
            </mixPort>
            <mixPort name="esai output" role="source" flags="AUDIO_OUTPUT_FLAG_DIRECT">
                <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_5POINT1,AUDIO_CHANNEL_OUT_7POINT1"/>
            </mixPort>
            <mixPort name="primary input" role="sink">
                <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="8000,11025,16000,22050,24000,32000,44100,48000" channelMasks="AUDIO_CHANNEL_IN_MONO,AUDIO_CHANNEL_IN_STEREO"/>
            </mixPort>
        </mixPorts>
        。。。。。。
    </module>
*/

//  deserializeCollection<MixPortTraits>(doc, root, mixPorts, NULL);
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename MixPortTraits::Collection &collection,
                                      typename MixPortTraits::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
        //  const char *const MixPortTraits::collectionTag = "mixPorts";
        //  const char *const MixPortTraits::tag = "mixPort";
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) && xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {// MixPortTraits::collectionTag = "mixPorts";
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) { // MixPortTraits::tag = "mixPort";
                typename Trait::PtrElement element; // MixPortTraits::sp<IOProfile>
                // MixPortTraits::deserialize(doc, child, element, serializingContext);
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status != NO_ERROR) {
                    return status;
                }
                if (collection.add(element) < 0) {  // MixPortTraits::Collection &collection
                    ALOGE("%s: could not add element to %s collection", __FUNCTION__,Trait::collectionTag);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {// MixPortTraits::tag = "mixPort";
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}
/**
    <mixPort name="primary output" role="source" flags="AUDIO_OUTPUT_FLAG_PRIMARY">
        <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
    </mixPort>
 * */
//  status_t MixPortTraits::deserialize(_xmlDoc *doc, const _xmlNode *child, sp<IOProfile> &mixPort,PtrSerializingCtx /*serializingContext*/)
status_t MixPortTraits::deserialize(_xmlDoc *doc, const _xmlNode *child, PtrElement &mixPort,PtrSerializingCtx /*serializingContext*/)
{
    string name = getXmlAttribute(child, Attributes::name);//   const char MixPortTraits::Attributes::name[] = "name";
    if (name.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::name);
        return BAD_VALUE;
    }

    ALOGV("%s: %s %s=%s", __FUNCTION__, tag, Attributes::name, name.c_str());
    string role = getXmlAttribute(child, Attributes::role); //const char MixPortTraits::Attributes::role[] = "role";
    if (role.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::role);
        return BAD_VALUE;
    }

    ALOGV("%s: Role=%s", __FUNCTION__, role.c_str());
    audio_port_role_t portRole = role == "source" ? AUDIO_PORT_ROLE_SOURCE : AUDIO_PORT_ROLE_SINK;

    //  typedef IOProfile Element;
    //  typedef sp<IOProfile> PtrElement;

    // mixPort = new IOProfile(String8(name.c_str()), portRole);
    mixPort = new Element(String8(name.c_str()), portRole);

    AudioProfileTraits::Collection profiles;
    deserializeCollection<AudioProfileTraits>(doc, child, profiles, NULL);
    if (profiles.isEmpty()) {//如果 没有  <profile> 标签 设置默认标签
        sp <AudioProfile> dynamicProfile = new AudioProfile(gDynamicFormat,ChannelsVector(), SampleRateVector());
        dynamicProfile->setDynamicFormat(true);
        dynamicProfile->setDynamicChannels(true);
        dynamicProfile->setDynamicRate(true);
        profiles.add(dynamicProfile);
    }
    mixPort->setAudioProfiles(profiles);

    string flags = getXmlAttribute(child, Attributes::flags);   //const char MixPortTraits::Attributes::flags[] = "flags";
    if (!flags.empty()) {
        // Source role
        if (portRole == AUDIO_PORT_ROLE_SOURCE) {
            mixPort->setFlags(OutputFlagConverter::maskFromString(flags));
        } else {
            // Sink role
            mixPort->setFlags(InputFlagConverter::maskFromString(flags));
        }
    }

    // Deserialize children
    AudioGainTraits::Collection gains;
    deserializeCollection<AudioGainTraits>(doc, child, gains, NULL);
    mixPort->setGains(gains);

    return NO_ERROR;
}

/*
    //typedef AudioProfileVector Collection;
    AudioProfileTraits::Collection profiles;
    deserializeCollection<AudioProfileTraits>(doc, child, profiles, NULL);


    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
 * */

static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,typename AudioProfileTraits::Collection &collection,
                        typename AudioProfileTraits::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
        //const char *const AudioProfileTraits::collectionTag = "profiles";
        //const char *const AudioProfileTraits::tag = "profile";
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) &&
                xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {//AudioProfileTraits::collectionTag = "profiles";
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {//AudioProfileTraits::tag = "profile";
                //  typedef sp<AudioProfile> PtrElement;
                typename Trait::PtrElement element;
                //   AudioProfileTraits::deserialize(doc, child, element, serializingContext);
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status != NO_ERROR) {
                    return status;
                }
                if (collection.add(element) < 0) {
                    ALOGE("%s: could not add element to %s collection", __FUNCTION__,
                          Trait::collectionTag);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}

/*
<profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
*/
status_t AudioProfileTraits::deserialize(_xmlDoc /*doc*/, const _xmlNode *root, sp<AudioProfile> &profile,PtrSerializingCtx /*serializingContext*/)
{
    string samplingRates = getXmlAttribute(root, Attributes::samplingRates);// 48000 
    string format = getXmlAttribute(root, Attributes::format); //AUDIO_FORMAT_PCM_16_BIT
    string channels = getXmlAttribute(root, Attributes::channelMasks);//AUDIO_CHANNEL_OUT_STEREO

    //  typedef AudioProfile Element;
    profile = new Element(formatFromString(format, gDynamicFormat),channelMasksFromString(channels, ","),samplingRatesFromString(samplingRates, ","));

    profile->setDynamicFormat(profile->getFormat() == gDynamicFormat);
    profile->setDynamicChannels(profile->getChannels().isEmpty());
    profile->setDynamicRate(profile->getSampleRates().isEmpty());

    return NO_ERROR;
}

/*
    //  typedef AudioGainCollection Collection;
    AudioGainTraits::Collection gains;
    deserializeCollection<AudioGainTraits>(doc, child, gains, NULL);
    mixPort->setGains(gains);
*/
//  deserializeCollection<AudioGainTraits>(doc, child, gains, NULL);
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename AudioGainTraits::Collection &collection,
                                      typename AudioGainTraits::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
        //  const char *const AudioGainTraits::tag = "gain";
        //  const char *const AudioGainTraits::collectionTag = "gains";
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) &&
                xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {//  AudioGainTraits::collectionTag = "gains";
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {//   AudioGainTraits::tag = "gain";
                //typedef sp<Element> PtrElement;
                typename Trait::PtrElement element;
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status != NO_ERROR) {
                    return status;
                }
                if (collection.add(element) < 0) {
                    ALOGE("%s: could not add element to %s collection", __FUNCTION__,Trait::collectionTag);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}

// AudioGainTraits::deserialize(doc, child, element, serializingContext);
status_t AudioGainTraits::deserialize(_xmlDoc /*doc*/, const _xmlNode *root, sp<Element> &gain,PtrSerializingCtx /*serializingContext*/)
{
    static uint32_t index = 0;
    // typedef AudioGain Element;
    gain = new Element(index++, true);

    string mode = getXmlAttribute(root, Attributes::mode); // AudioGainTraits::Attributes::mode[] = "mode";
    if (!mode.empty()) {
        gain->setMode(GainModeConverter::maskFromString(mode));
    }

    string channelsLiteral = getXmlAttribute(root, Attributes::channelMask);//AudioGainTraits::Attributes::channelMask[] = "channel_mask";
    if (!channelsLiteral.empty()) {
        gain->setChannelMask(channelMaskFromString(channelsLiteral));
    }

    string minValueMBLiteral = getXmlAttribute(root, Attributes::minValueMB);// AudioGainTraits::Attributes::minValueMB[] = "minValueMB";
    uint32_t minValueMB;
    if (!minValueMBLiteral.empty() && convertTo(minValueMBLiteral, minValueMB)) {
        gain->setMinValueInMb(minValueMB);
    }

    string maxValueMBLiteral = getXmlAttribute(root, Attributes::maxValueMB);// AudioGainTraits::Attributes::maxValueMB[] = "maxValueMB";
    uint32_t maxValueMB;
    if (!maxValueMBLiteral.empty() && convertTo(maxValueMBLiteral, maxValueMB)) {
        gain->setMaxValueInMb(maxValueMB);
    }

    string defaultValueMBLiteral = getXmlAttribute(root, Attributes::defaultValueMB);// AudioGainTraits::Attributes::defaultValueMB[] = "defaultValueMB";
    uint32_t defaultValueMB;
    if (!defaultValueMBLiteral.empty() && convertTo(defaultValueMBLiteral, defaultValueMB)) {
        gain->setDefaultValueInMb(defaultValueMB);
    }

    string stepValueMBLiteral = getXmlAttribute(root, Attributes::stepValueMB);//   AudioGainTraits::Attributes::stepValueMB[] = "stepValueMB";
    uint32_t stepValueMB;
    if (!stepValueMBLiteral.empty() && convertTo(stepValueMBLiteral, stepValueMB)) {
        gain->setStepValueInMb(stepValueMB);
    }

    string minRampMsLiteral = getXmlAttribute(root, Attributes::minRampMs);//   AudioGainTraits::Attributes::minRampMs[] = "minRampMs";
    uint32_t minRampMs;
    if (!minRampMsLiteral.empty() && convertTo(minRampMsLiteral, minRampMs)) {
        gain->setMinRampInMs(minRampMs);
    }

    string maxRampMsLiteral = getXmlAttribute(root, Attributes::maxRampMs);//   AudioGainTraits::Attributes::maxRampMs[] = "maxRampMs";
    uint32_t maxRampMs;
    if (!maxRampMsLiteral.empty() && convertTo(maxRampMsLiteral, maxRampMs)) {
        gain->setMaxRampInMs(maxRampMs);
    }
    ALOGV("%s: adding new gain mode %08x channel mask %08x min mB %d max mB %d", __FUNCTION__,
          gain->getMode(), gain->getChannelMask(), gain->getMinValueInMb(),
          gain->getMaxValueInMb());

    if (gain->getMode() == 0) {
        return BAD_VALUE;
    }
    return NO_ERROR;
}

/*
    DevicePortTraits::Collection devicePorts;
    deserializeCollection<DevicePortTraits>(doc, root, devicePorts, NULL);
    module->setDeclaredDevices(devicePorts);

    解析格式：
    <module name="primary" halVersion="2.0">
         。。。。。。
        <devicePorts>
                <!-- Output devices declaration, i.e. Sink DEVICE PORT -->
                <devicePort tagName="Earpiece" type="AUDIO_DEVICE_OUT_EARPIECE" role="sink">
                   <profile name="" format="AUDIO_FORMAT_PCM_16_BIT"
                            samplingRates="48000" channelMasks="AUDIO_CHANNEL_IN_MONO"/>
                </devicePort>
                <devicePort tagName="Speaker" role="sink" type="AUDIO_DEVICE_OUT_SPEAKER" address="">
                    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT"
                             samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
                    <gains>
                        <gain name="gain_1" mode="AUDIO_GAIN_MODE_JOINT"
                              minValueMB="-8400"
                              maxValueMB="4000"
                              defaultValueMB="0"
                              stepValueMB="100"/>
                    </gains>
                </devicePort>
                <devicePort tagName="Wired Headset" type="AUDIO_DEVICE_OUT_WIRED_HEADSET" role="sink">
                    <profile name="" format="AUDIO_FORMAT_PCM_16_BIT"
                             samplingRates="48000" channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
                </devicePort>
        </devicePorts>
        。。。。。。
    </module>
*/

//  deserializeCollection<DevicePortTraits>(doc, root, devicePorts, NULL);
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename DevicePortTraits::Collection &collection,
                                      typename DevicePortTraits::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {

        //  const char *const DevicePortTraits::tag = "devicePort";
        //  const char *const DevicePortTraits::collectionTag = "devicePorts";
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) &&
                xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {// DevicePortTraits::collectionTag = "devicePorts";
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {//  DevicePortTraits::tag = "devicePort";
                //  typedef DeviceDescriptor Element; 
                typename Trait::PtrElement element;
                // 在下面再次分析
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status != NO_ERROR) {
                    return status;
                }
                if (collection.add(element) < 0) {
                    ALOGE("%s: could not add element to %s collection", __FUNCTION__,
                          Trait::collectionTag);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}


status_t DevicePortTraits::deserialize(_xmlDoc *doc, const _xmlNode *root, sp<DeviceDescriptor> &deviceDesc,PtrSerializingCtx /*serializingContext*/)
{
    string name = getXmlAttribute(root, Attributes::tagName); //    DevicePortTraits::Attributes::tagName[] = "tagName";
    if (name.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::tagName);
        return BAD_VALUE;
    }
    ALOGV("%s: %s %s=%s", __FUNCTION__, tag, Attributes::tagName, name.c_str());
    string typeName = getXmlAttribute(root, Attributes::type);//    DevicePortTraits::Attributes::type[] = "type";
    if (typeName.empty()) {
        ALOGE("%s: no type for %s", __FUNCTION__, name.c_str());
        return BAD_VALUE;
    }
    ALOGV("%s: %s %s=%s", __FUNCTION__, tag, Attributes::type, typeName.c_str());
    string role = getXmlAttribute(root, Attributes::role);  //  DevicePortTraits::Attributes::role[] = "role";
    if (role.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::role);
        return BAD_VALUE;
    }
    ALOGV("%s: %s %s=%s", __FUNCTION__, tag, Attributes::role, role.c_str());
    audio_port_role_t portRole = (role == Attributes::roleSource) ? AUDIO_PORT_ROLE_SOURCE : AUDIO_PORT_ROLE_SINK;

    audio_devices_t type = AUDIO_DEVICE_NONE;
    if (!deviceFromString(typeName, type) ||
            (!audio_is_input_device(type) && portRole == AUDIO_PORT_ROLE_SOURCE) ||
            (!audio_is_output_devices(type) && portRole == AUDIO_PORT_ROLE_SINK)) {
        ALOGW("%s: bad type %08x", __FUNCTION__, type);
        return BAD_VALUE;
    }

    //  typedef DeviceDescriptor Element;
    deviceDesc = new Element(type, String8(name.c_str()));

    string address = getXmlAttribute(root, Attributes::address); // DevicePortTraits::Attributes::address[] = "address";
    if (!address.empty()) {
        ALOGV("%s: address=%s for %s", __FUNCTION__, address.c_str(), name.c_str());
        deviceDesc->mAddress = String8(address.c_str());
    }
    /*
    关于AudioProfileTraits 在 上面mixport中已经有分析,对应标签 profile
    */
    AudioProfileTraits::Collection profiles; 
    deserializeCollection<AudioProfileTraits>(doc, root, profiles, NULL);
    if (profiles.isEmpty()) {
        sp <AudioProfile> dynamicProfile = new AudioProfile(gDynamicFormat,ChannelsVector(), SampleRateVector());
        dynamicProfile->setDynamicFormat(true);
        dynamicProfile->setDynamicChannels(true);
        dynamicProfile->setDynamicRate(true);
        profiles.add(dynamicProfile);
    }
    deviceDesc->setAudioProfiles(profiles);

    /***
     * 关于AudioGainTraits 在 上面mixport中已经有分析，对应标签 gain
     * */
    // Deserialize AudioGain children
    deserializeCollection<AudioGainTraits>(doc, root, deviceDesc->mGains, NULL);
    ALOGV("%s: adding device tag %s type %08x address %s", __FUNCTION__,deviceDesc->getName().string(), type, deviceDesc->mAddress.string());
    return NO_ERROR;
}




status_t RouteTraits::deserialize(_xmlDoc /*doc*/, const _xmlNode *root, PtrElement &element,PtrSerializingCtx ctx)
{
    string type = getXmlAttribute(root, Attributes::type);
    if (type.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::type);
        return BAD_VALUE;
    }
    audio_route_type_t routeType = (type == Attributes::typeMix) ? AUDIO_ROUTE_MIX : AUDIO_ROUTE_MUX;

    ALOGV("%s: %s %s=%s", __FUNCTION__, tag, Attributes::type, type.c_str());
    element = new Element(routeType);

    string sinkAttr = getXmlAttribute(root, Attributes::sink);
    if (sinkAttr.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::sink);
        return BAD_VALUE;
    }
    // Convert Sink name to port pointer
    sp<AudioPort> sink = ctx->findPortByTagName(String8(sinkAttr.c_str()));
    if (sink == NULL) {
        ALOGE("%s: no sink found with name=%s", __FUNCTION__, sinkAttr.c_str());
        return BAD_VALUE;
    }
    element->setSink(sink);

    string sourcesAttr = getXmlAttribute(root, Attributes::sources);
    if (sourcesAttr.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::sources);
        return BAD_VALUE;
    }
    // Tokenize and Convert Sources name to port pointer
    AudioPortVector sources;
    char *sourcesLiteral = strndup(sourcesAttr.c_str(), strlen(sourcesAttr.c_str()));
    char *devTag = strtok(sourcesLiteral, ",");
    while (devTag != NULL) {
        if (strlen(devTag) != 0) {
            sp<AudioPort> source = ctx->findPortByTagName(String8(devTag));
            if (source == NULL) {
                ALOGE("%s: no source found with name=%s", __FUNCTION__, devTag);
                free(sourcesLiteral);
                return BAD_VALUE;
            }
            sources.add(source);
        }
        devTag = strtok(NULL, ",");
    }
    free(sourcesLiteral);

    sink->addRoute(element);
    for (size_t i = 0; i < sources.size(); i++) {
        sp<AudioPort> source = sources.itemAt(i);
        source->addRoute(element);
    }
    element->setSources(sources);
    return NO_ERROR;
}









//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/***
    VolumeTraits::Collection volumes;
    deserializeCollection<VolumeTraits>(doc, cur, volumes, &config);
    config.setVolumes(volumes);


    <volumes>

        <volume stream="AUDIO_STREAM_VOICE_CALL" deviceCategory="DEVICE_CATEGORY_HEADSET">
            <point>0,-4200</point>
            <point>33,-2800</point>
            <point>66,-1400</point>
            <point>100,0</point>
        </volume>


         // 这里 ref 属性使用来对应同一个reference
         <volume stream="AUDIO_STREAM_PATCH" deviceCategory="DEVICE_CATEGORY_HEARING_AID" ref="FULL_SCALE_VOLUME_CURVE"/>

        <reference name="FULL_SCALE_VOLUME_CURVE">
            <!-- Full Scale reference Volume Curve -->
            <point>0,0</point>
            <point>100,0</point>
        </reference>

    </volumes>

*/
template <class Trait>
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,typename Trait::Collection &collection,typename Trait::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
    	//	const char *const VolumeTraits::collectionTag = "volumes";
    	//	const char *const VolumeTraits::tag = "volume";
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) &&  xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {//	const char *const VolumeTraits::collectionTag = "volumes";
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {//	const char *const VolumeTraits::tag = "volume";
                // typename VolumeTraits::sp<VolumeCurve> element;
                typename Trait::PtrElement element; 
                //status_t status = VolumeTraits::deserialize(doc, child, element, serializingContext);  
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status != NO_ERROR) {
                    return status;
                }
                if (collection.add(element) < 0) {
                    ALOGE("%s: could not add element to %s collection", __FUNCTION__,Trait::collectionTag);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}

//status_t VolumeTraits::deserialize(_xmlDoc *doc, const _xmlNode *root, sp<VolumeCurve> &element, PtrSerializingCtx /*serializingContext*/)
status_t VolumeTraits::deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element, PtrSerializingCtx /*serializingContext*/)
{
    string streamTypeLiteral = getXmlAttribute(root, Attributes::stream);// const char VolumeTraits::Attributes::stream[] = "stream";
    if (streamTypeLiteral.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::stream);
        return BAD_VALUE;
    }
    audio_stream_type_t streamType;
    if (!StreamTypeConverter::fromString(streamTypeLiteral, streamType)) {
        ALOGE("%s: Invalid %s", __FUNCTION__, Attributes::stream);
        return BAD_VALUE;
    }
    string deviceCategoryLiteral = getXmlAttribute(root, Attributes::deviceCategory);//	const char VolumeTraits::Attributes::deviceCategory[] = "deviceCategory";
    if (deviceCategoryLiteral.empty()) {
        ALOGE("%s: No %s found", __FUNCTION__, Attributes::deviceCategory);
        return BAD_VALUE;
    }
    device_category deviceCategory;
    if (!DeviceCategoryConverter::fromString(deviceCategoryLiteral, deviceCategory)) {
        ALOGE("%s: Invalid %s=%s", __FUNCTION__, Attributes::deviceCategory,deviceCategoryLiteral.c_str());
        return BAD_VALUE;
    }

    string referenceName = getXmlAttribute(root, Attributes::reference);//	const char VolumeTraits::Attributes::reference[] = "ref";
    const _xmlNode *ref = NULL;
    if (!referenceName.empty()) {
        getReference<VolumeTraits>(root->parent, ref, referenceName);
        if (ref == NULL) {
            ALOGE("%s: No reference Ptr found for %s", __FUNCTION__, referenceName.c_str());
            return BAD_VALUE;
        }
    }

    // element = new VolumeCurve(deviceCategory, streamType); 
    element = new Element(deviceCategory, streamType);

    const xmlNode *child = referenceName.empty() ? root->xmlChildrenNode : ref->xmlChildrenNode;
    while (child != NULL) {
        if (!xmlStrcmp(child->name, (const xmlChar *)volumePointTag)) {//	const char *const VolumeTraits::volumePointTag = "point";
            xmlChar *pointDefinition = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (pointDefinition == NULL) {
                return BAD_VALUE;
            }
            ALOGV("%s: %s=%s", __FUNCTION__, tag, (const char*)pointDefinition);
            Vector<int32_t> point;
            collectionFromString<DefaultTraits<int32_t> >((const char*)pointDefinition, point, ",");
            if (point.size() != 2) {
                ALOGE("%s: Invalid %s: %s", __FUNCTION__, volumePointTag,(const char*)pointDefinition);
                return BAD_VALUE;
            }
            element->add(CurvePoint(point[0], point[1]));
            xmlFree(pointDefinition);
        }
        child = child->next;
    }
    return NO_ERROR;
}





















/***
 * 这里解析标签tag为 globalConfiguration，其包含属性名称为 speaker_drc_enabled（其值为false或者true），
 * 通过AudioPolicyConfig 的 setSpeakerDrcEnabled 方法设置该属性值
 * */
status_t GlobalConfigTraits::deserialize(const xmlNode *cur, AudioPolicyConfig &config)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
        if (!xmlStrcmp(root->name, (const xmlChar *)tag)) {//   const char *const GlobalConfigTraits::tag = "globalConfiguration";
            //  const char GlobalConfigTraits::Attributes::speakerDrcEnabled[] = "speaker_drc_enabled";
            string speakerDrcEnabled = getXmlAttribute(root, Attributes::speakerDrcEnabled);
            bool isSpeakerDrcEnabled;
            if (!speakerDrcEnabled.empty() && convertTo<string, bool>(speakerDrcEnabled, isSpeakerDrcEnabled)) {
                config.setSpeakerDrcEnabled(isSpeakerDrcEnabled);//true
            }
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}