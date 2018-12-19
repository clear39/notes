//	@frameworks/av/media/libstagefright/MediaCodecList.cpp
// static
sp<IMediaCodecList> MediaCodecList::getInstance() {
    Mutex::Autolock _l(sRemoteInitMutex);
    if (sRemoteList == nullptr) {
        sp<IBinder> binder = defaultServiceManager()->getService(String16("media.player"));
        sp<IMediaPlayerService> service = interface_cast<IMediaPlayerService>(binder);
        if (service.get() != nullptr) {
            sRemoteList = service->getCodecList();
            if (sRemoteList != nullptr) {
                sBinderDeathObserver = new BinderDeathObserver();
                binder->linkToDeath(sBinderDeathObserver.get());
            }
        }
        if (sRemoteList == nullptr) {
            // if failed to get remote list, create local list
            sRemoteList = getLocalInstance();
        }
    }
    return sRemoteList;
}



// static
sp<IMediaCodecList> MediaCodecList::getLocalInstance() {
    Mutex::Autolock autoLock(sInitMutex);

    if (sCodecList == nullptr) {
        MediaCodecList *codecList = new MediaCodecList(&sOmxInfoBuilder);
        if (codecList->initCheck() == OK) {
            sCodecList = codecList;

            if (isProfilingNeeded()) {
                ALOGV("Codec profiling needed, will be run in separated thread.");
                pthread_t profiler;
                if (pthread_create(&profiler, nullptr, profilerThreadWrapper, nullptr) != 0) {
                    ALOGW("Failed to create thread for codec profiling.");
                }
            }
        } else {
            // failure to initialize may be temporary. retry on next call.
            delete codecList;
        }
    }

    return sCodecList;
}



MediaCodecList::MediaCodecList(MediaCodecListBuilderBase* builder) {
    mGlobalSettings = new AMessage();
    mCodecInfos.clear();
    MediaCodecListWriter writer(this);
    mInitCheck = builder->buildMediaCodecList(&writer);
}


//	@framworks/av/media/libstagefright/OmxInfoBuilder.cpp
status_t OmxInfoBuilder::buildMediaCodecList(MediaCodecListWriter* writer) {
    bool treble;
    sp<IOMX> omx;
    std::vector<IOMXStore::RoleInfo> roles;

    treble = property_get_bool("persist.media.treble_omx", true);//true
    if (treble) {
        using namespace ::android::hardware::media::omx::V1_0;
        using ::android::hardware::hidl_vec;
        using ::android::hardware::hidl_string;

        // Obtain IOmxStore
        sp<IOmxStore> omxStore = IOmxStore::getService();
        if (omxStore == nullptr) {
            ALOGE("Cannot connect to an IOmxStore instance.");
            return NO_INIT;
        }

        // List service attributes (global settings)
        Status status;
        hidl_vec<IOmxStore::ServiceAttribute> serviceAttributes;
        auto transStatus = omxStore->listServiceAttributes(
                [&status, &serviceAttributes]
                (Status inStatus, const hidl_vec<IOmxStore::ServiceAttribute>&
                        inAttributes) {
                    status = inStatus;
                    serviceAttributes = inAttributes;
                });
        if (!transStatus.isOk()) {
            ALOGE("Fail to obtain global settings from IOmxStore.");
            return NO_INIT;
        }
        if (status != Status::OK) {
            ALOGE("IOmxStore reports parsing error.");
            return NO_INIT;
        }
        for (const auto& p : serviceAttributes) {
            writer->addGlobalSetting(
                    p.key.c_str(), p.value.c_str());
        }

        // List roles and convert to IOMXStore's format
        transStatus = omxStore->listRoles(
                [&roles]
                (const hidl_vec<IOmxStore::RoleInfo>& inRoleList) {
                    roles.reserve(inRoleList.size());
                    for (const auto& inRole : inRoleList) {
                        IOMXStore::RoleInfo role;
                        role.role = inRole.role;
                        role.type = inRole.type;
                        role.isEncoder = inRole.isEncoder;
                        role.preferPlatformNodes = inRole.preferPlatformNodes;
                        std::vector<IOMXStore::NodeInfo>& nodes =
                                role.nodes;
                        nodes.reserve(inRole.nodes.size());
                        for (const auto& inNode : inRole.nodes) {
                            IOMXStore::NodeInfo node;
                            node.name = inNode.name;
                            node.owner = inNode.owner;
                            std::vector<IOMXStore::Attribute>& attributes =
                                    node.attributes;
                            attributes.reserve(inNode.attributes.size());
                            for (const auto& inAttr : inNode.attributes) {
                                IOMXStore::Attribute attr;
                                attr.key = inAttr.key;
                                attr.value = inAttr.value;
                                attributes.push_back(std::move(attr));
                            }
                            nodes.push_back(std::move(node));
                        }
                        roles.push_back(std::move(role));
                    }
                });
        if (!transStatus.isOk()) {
            ALOGE("Fail to obtain codec roles from IOmxStore.");
            return NO_INIT;
        }
    } else {
        // Obtain IOMXStore
        sp<IServiceManager> sm = defaultServiceManager();
        if (sm == nullptr) {
            ALOGE("Cannot obtain the default service manager.");
            return NO_INIT;
        }
        sp<IBinder> codecBinder = sm->getService(String16("media.codec"));
        if (codecBinder == nullptr) {
            ALOGE("Cannot obtain the media codec service.");
            return NO_INIT;
        }
        sp<IMediaCodecService> codecService =
                interface_cast<IMediaCodecService>(codecBinder);
        if (codecService == nullptr) {
            ALOGE("Wrong type of media codec service obtained.");
            return NO_INIT;
        }
        omx = codecService->getOMX();
        if (omx == nullptr) {
            ALOGE("Cannot connect to an IOMX instance.");
        }
        sp<IOMXStore> omxStore = codecService->getOMXStore();
        if (omxStore == nullptr) {
            ALOGE("Cannot connect to an IOMXStore instance.");
            return NO_INIT;
        }

        // List service attributes (global settings)
        std::vector<IOMXStore::Attribute> serviceAttributes;
        status_t status = omxStore->listServiceAttributes(&serviceAttributes);
        if (status != OK) {
            ALOGE("Fail to obtain global settings from IOMXStore.");
            return NO_INIT;
        }
        for (const auto& p : serviceAttributes) {
            writer->addGlobalSetting(
                    p.key.c_str(), p.value.c_str());
        }

        // List roles
        status = omxStore->listRoles(&roles);
        if (status != OK) {
            ALOGE("Fail to obtain codec roles from IOMXStore.");
            return NO_INIT;
        }
    }

    // Convert roles to lists of codecs

    // codec name -> index into swCodecs
    std::map<std::string, std::unique_ptr<MediaCodecInfoWriter> > swCodecName2Info;
    // codec name -> index into hwCodecs
    std::map<std::string, std::unique_ptr<MediaCodecInfoWriter> > hwCodecName2Info;
    // owner name -> MediaCodecInfo
    // This map will be used to obtain the correct IOmx service(s) needed for
    // creating IOmxNode instances and querying capabilities.
    std::map<std::string, std::vector<sp<MediaCodecInfo> > > owner2CodecInfo;

    for (const auto& role : roles) {
        const auto& typeName = role.type;
        bool isEncoder = role.isEncoder;
        bool preferPlatformNodes = role.preferPlatformNodes;
        // If preferPlatformNodes is true, hardware nodes must be added after
        // platform (software) nodes. hwCodecs is used to hold hardware nodes
        // that need to be added after software nodes for the same role.
        std::vector<const IOMXStore::NodeInfo*> hwCodecs;
        for (const auto& node : role.nodes) {
            const auto& nodeName = node.name;
            bool isSoftware = (nodeName.compare(0, 10, "OMX.google") == 0) || (nodeName.find("sw-based") <= nodeName.length());
            MediaCodecInfoWriter* info;
            if (isSoftware) {
                auto c2i = swCodecName2Info.find(nodeName);
                if (c2i == swCodecName2Info.end()) {
                    // Create a new MediaCodecInfo for a new node.
                    c2i = swCodecName2Info.insert(std::make_pair(
                            nodeName, writer->addMediaCodecInfo())).first;
                    info = c2i->second.get();
                    info->setName(nodeName.c_str());
                    info->setOwner(node.owner.c_str());
                    info->setEncoder(isEncoder);
                } else {
                    // The node has been seen before. Simply retrieve the
                    // existing MediaCodecInfoWriter.
                    info = c2i->second.get();
                }
            } else {
                auto c2i = hwCodecName2Info.find(nodeName);
                if (c2i == hwCodecName2Info.end()) {
                    // Create a new MediaCodecInfo for a new node.
                    if (!preferPlatformNodes) {
                        c2i = hwCodecName2Info.insert(std::make_pair(
                                nodeName, writer->addMediaCodecInfo())).first;
                        info = c2i->second.get();
                        info->setName(nodeName.c_str());
                        info->setOwner(node.owner.c_str());
                        info->setEncoder(isEncoder);
                    } else {
                        // If preferPlatformNodes is true, this node must be
                        // added after all software nodes.
                        hwCodecs.push_back(&node);
                        continue;
                    }
                } else {
                    // The node has been seen before. Simply retrieve the
                    // existing MediaCodecInfoWriter.
                    info = c2i->second.get();
                }
            }
            std::unique_ptr<MediaCodecInfo::CapabilitiesWriter> caps = info->addMime(typeName.c_str());
            if (queryCapabilities( node, typeName.c_str(), isEncoder, caps.get()) != OK) {
                ALOGW("Fail to add mime %s to codec %s", typeName.c_str(), nodeName.c_str());
                info->removeMime(typeName.c_str());
            }
        }

        // If preferPlatformNodes is true, hardware nodes will not have been
        // added in the loop above, but rather saved in hwCodecs. They are
        // going to be added here.
        if (preferPlatformNodes) {
            for (const auto& node : hwCodecs) {
                MediaCodecInfoWriter* info;
                const auto& nodeName = node->name;
                auto c2i = hwCodecName2Info.find(nodeName);
                if (c2i == hwCodecName2Info.end()) {
                    // Create a new MediaCodecInfo for a new node.
                    c2i = hwCodecName2Info.insert(std::make_pair(
                            nodeName, writer->addMediaCodecInfo())).first;
                    info = c2i->second.get();
                    info->setName(nodeName.c_str());
                    info->setOwner(node->owner.c_str());
                    info->setEncoder(isEncoder);
                } else {
                    // The node has been seen before. Simply retrieve the
                    // existing MediaCodecInfoWriter.
                    info = c2i->second.get();
                }
                std::unique_ptr<MediaCodecInfo::CapabilitiesWriter> caps =
                        info->addMime(typeName.c_str());
                if (queryCapabilities(
                        *node, typeName.c_str(), isEncoder, caps.get()) != OK) {
                    ALOGW("Fail to add mime %s to codec %s "
                          "after software codecs",
                          typeName.c_str(), nodeName.c_str());
                    info->removeMime(typeName.c_str());
                }
            }
        }
    }
    return OK;
}



status_t queryCapabilities(const IOMXStore::NodeInfo& node, const char* mime, bool isEncoder,MediaCodecInfo::CapabilitiesWriter* caps) {
    sp<ACodec> codec = new ACodec();
    status_t err = codec->queryCapabilities(
            node.owner.c_str(), node.name.c_str(), mime, isEncoder, caps);
    if (err != OK) {
        return err;
    }
    for (const auto& attribute : node.attributes) {
        // All features have an int32 value except
        // "feature-bitrate-modes", which has a string value.
        if ((attribute.key.compare(0, 8, "feature-") == 0) &&
                (attribute.key.compare(8, 15, "bitrate-modes")
                 != 0)) {
            // If this attribute.key is a feature that is not a bitrate
            // control, add an int32 value.
            caps->addDetail(
                    attribute.key.c_str(),
                    attribute.value == "1" ? 1 : 0);
        } else {
            // Non-feature attributes
            caps->addDetail(
                    attribute.key.c_str(), attribute.value.c_str());
        }
    }
    return OK;
}