//	@frameworks/av/services/audiopolicy/common/managerdefinitions/src/Serializer.cpp
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
        ALOGE("%s: No %s root element found in xml data %s.", __FUNCTION__, mRootElementName.c_str(),
              (const char *)cur->name);
        xmlFreeDoc(doc);
        return BAD_VALUE;
    }

    string version = getXmlAttribute(cur, versionAttribute);//	version="1.0"
    if (version.empty()) {
        ALOGE("%s: No version found in root node %s", __FUNCTION__, mRootElementName.c_str());
        return BAD_VALUE;
    }
    if (version != mVersion) {
        ALOGE("%s: Version does not match; expect %s got %s", __FUNCTION__, mVersion.c_str(),version.c_str());
        return BAD_VALUE;
    }
    // Lets deserialize children
    // Modules
    ModuleTraits::Collection modules;
    deserializeCollection<ModuleTraits>(doc, cur, modules, &config);
    config.setHwModules(modules);

    // deserialize volume section
    VolumeTraits::Collection volumes;
    deserializeCollection<VolumeTraits>(doc, cur, volumes, &config);
    config.setVolumes(volumes);

    // Global Configuration
    GlobalConfigTraits::deserialize(cur, config);

    xmlFreeDoc(doc);
    return android::OK;
}



/***
    ModuleTraits::Collection modules;									//	typedef HwModuleCollection Collection;
    deserializeCollection<ModuleTraits>(doc, cur, modules, &config);
    config.setHwModules(modules);
*/
template <class Trait>
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,typename Trait::Collection &collection,typename Trait::PtrSerializingCtx serializingContext)
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
                typename Trait::PtrElement element;							//	 typedef HwModule Element; 		typedef sp<Element> PtrElement;
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


//	typedef sp<Element> PtrElement;
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

    module = new Element(name.c_str(), versionMajor, versionMinor);//	typedef HwModule Element;

    // Deserialize childrens: Audio Mix Port, Audio Device Ports (Source/Sink), Audio Routes
    MixPortTraits::Collection mixPorts;
    deserializeCollection<MixPortTraits>(doc, root, mixPorts, NULL);
    module->setProfiles(mixPorts);

    DevicePortTraits::Collection devicePorts;
    deserializeCollection<DevicePortTraits>(doc, root, devicePorts, NULL);
    module->setDeclaredDevices(devicePorts);

    RouteTraits::Collection routes;
    deserializeCollection<RouteTraits>(doc, root, routes, module.get());
    module->setRoutes(routes);

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
                sp<DeviceDescriptor> device = module->getDeclaredDevices().getDeviceFromTagName(String8((const char*)defaultOutputDevice));
                if (device != 0 && ctx->getDefaultOutputDevice() == 0) {
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










//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/***
    VolumeTraits::Collection volumes;
    deserializeCollection<VolumeTraits>(doc, cur, volumes, &config);
    config.setVolumes(volumes);
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
        ALOGE("%s: Invalid %s=%s", __FUNCTION__, Attributes::deviceCategory,
              deviceCategoryLiteral.c_str());
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

    element = new Element(deviceCategory, streamType);

    const xmlNode *child = referenceName.empty() ? root->xmlChildrenNode : ref->xmlChildrenNode;
    while (child != NULL) {
        if (!xmlStrcmp(child->name, (const xmlChar *)volumePointTag)) {//	const char *const VolumeTraits::volumePointTag = "point";
            xmlChar *pointDefinition = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);;
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