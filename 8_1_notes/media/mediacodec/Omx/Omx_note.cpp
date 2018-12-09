//	@hardware/interfaces/media/omx/1.0/IOmx.hal
interface IOmx {

    /**
     * Information for an IOmxNode component.
     */
    struct ComponentInfo {
        string mName;
        vec<string> mRoles;
    };

    /**
     * List available components.
     *
     * @return status The status of the call.
     * @return nodeList The list of ComponentInfo.
     */
    listNodes(
        ) generates (
            Status status,
            vec<ComponentInfo> nodeList
        );


    /**
     * Allocate an IOmxNode instance with the specified node name.
     *
     * @param name The name of the node to create.
     * @param observer An observer object that will receive messages from
     * the created instance.
     * @return status The status of the call.
     * @return omxNode The allocated instance of `IOmxNode`.
     */
    allocateNode(
            string name,
            IOmxObserver observer
        ) generates (
            Status status,
            IOmxNode omxNode
        );

    /**
     * Create an input surface for recording.
     *
     * @return status The status of the call.
     * @return producer The associated producer end of the buffer queue.
     * @return source The associated `IGraphicBufferSource`.
     */
    createInputSurface(
        ) generates (
            Status status,
            IGraphicBufferProducer producer,
            IGraphicBufferSource source
        );
};





//	@frameworks/av/media/libstagefright/omx/include/media/stagefright/omx/1.0/Omx.h
struct Omx : public IOmx, public hidl_death_recipient, public OmxNodeOwner {
    Omx();
    virtual ~Omx();

    // Methods from IOmx
    Return<void> listNodes(listNodes_cb _hidl_cb) override;
    Return<void> allocateNode(const hidl_string& name,const sp<IOmxObserver>& observer,allocateNode_cb _hidl_cb) override;
    Return<void> createInputSurface(createInputSurface_cb _hidl_cb) override;

    // Method from hidl_death_recipient
    void serviceDied(uint64_t cookie, const wp<IBase>& who) override;

    // Method from OmxNodeOwner
    virtual status_t freeNode(sp<OMXNodeInstance> const& instance) override;

protected:
    OMXMaster* mMaster;
    Mutex mLock;
    KeyedVector<wp<IBase>, sp<OMXNodeInstance> > mLiveNodes;
    KeyedVector<OMXNodeInstance*, wp<IBase> > mNode2Observer;
    MediaCodecsXmlParser mParser;
};



//	@frameworks/av/media/libstagefright/omx/1.0/Omx.cpp
Omx::Omx() :
    mMaster(new OMXMaster()),
    mParser() {	//	MediaCodecsXmlParser mParser;
}



Return<void> Omx::listNodes(listNodes_cb _hidl_cb) {
    std::list<::android::IOMX::ComponentInfo> list;
    char componentName[256];
    for (OMX_U32 index = 0;mMaster->enumerateComponents(componentName, sizeof(componentName), index) == OMX_ErrorNone;++index) {
        list.push_back(::android::IOMX::ComponentInfo());
        ::android::IOMX::ComponentInfo& info = list.back();
        info.mName = componentName;
        ::android::Vector<::android::String8> roles;
        OMX_ERRORTYPE err = mMaster->getRolesOfComponent(componentName, &roles);
        if (err == OMX_ErrorNone) {
            for (OMX_U32 i = 0; i < roles.size(); ++i) {
                info.mRoles.push_back(roles[i]);
            }
        }
    }

    hidl_vec<ComponentInfo> tList;
    tList.resize(list.size());
    size_t i = 0;
    for (auto const& info : list) {
        convertTo(&(tList[i++]), info);
    }
    _hidl_cb(toStatus(OK), tList);
    return Void();
}



Return<void> Omx::allocateNode(const hidl_string& name,const sp<IOmxObserver>& observer,allocateNode_cb _hidl_cb) {

    using ::android::IOMXNode;
    using ::android::IOMXObserver;

    sp<OMXNodeInstance> instance;
    {
        Mutex::Autolock autoLock(mLock);
        if (mLiveNodes.size() == kMaxNodeInstances) {
            _hidl_cb(toStatus(NO_MEMORY), nullptr);
            return Void();
        }

        instance = new OMXNodeInstance(this, new LWOmxObserver(observer), name.c_str());

        OMX_COMPONENTTYPE *handle;
        OMX_ERRORTYPE err = mMaster->makeComponentInstance(name.c_str(), &OMXNodeInstance::kCallbacks,instance.get(), &handle);

        if (err != OMX_ErrorNone) {
            LOG(ERROR) << "Failed to allocate omx component "
                    "'" << name.c_str() << "' "
                    " err=" << asString(err) <<
                    "(0x" << std::hex << unsigned(err) << ")";
            _hidl_cb(toStatus(StatusFromOMXError(err)), nullptr);
            return Void();
        }
        instance->setHandle(handle);

        // Find quirks from mParser
        const auto& codec = mParser.getCodecMap().find(name.c_str());
        if (codec == mParser.getCodecMap().cend()) {
            LOG(WARNING) << "Failed to obtain quirks for omx component "
                    "'" << name.c_str() << "' "
                    "from XML files";
        } else {
            uint32_t quirks = 0;
            for (const auto& quirk : codec->second.quirkSet) {
                if (quirk == "requires-allocate-on-input-ports") {
                    quirks |= OMXNodeInstance::
                            kRequiresAllocateBufferOnInputPorts;
                }
                if (quirk == "requires-allocate-on-output-ports") {
                    quirks |= OMXNodeInstance::
                            kRequiresAllocateBufferOnOutputPorts;
                }
            }
            instance->setQuirks(quirks);
        }

        mLiveNodes.add(observer.get(), instance);
        mNode2Observer.add(instance.get(), observer.get());
    }
    observer->linkToDeath(this, 0);

    _hidl_cb(toStatus(OK), new TWOmxNode(instance));
    return Void();
}