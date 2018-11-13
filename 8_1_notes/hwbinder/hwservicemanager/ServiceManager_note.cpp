//	@system/hwservicemanager/ServiceManager.cpp

Return<ServiceManager::Transport> ServiceManager::getTransport(const hidl_string& fqName, const hidl_string& name) {
    using ::android::hardware::getTransport;//	@system/hwservicemanager/Vintf.cpp

    pid_t pid = IPCThreadState::self()->getCallingPid();
    if (!mAcl.canGet(fqName, pid)) {//selinux 权限检查
        return Transport::EMPTY;
    }

    switch (getTransport(fqName, name)) {
        case vintf::Transport::HWBINDER:
             return Transport::HWBINDER;
        case vintf::Transport::PASSTHROUGH:
             return Transport::PASSTHROUGH;
        case vintf::Transport::EMPTY:
        default:
             return Transport::EMPTY;
    }
}



vintf::Transport getTransport(const std::string &interfaceName, const std::string &instanceName) {
    FQName fqName(interfaceName);//	@system/tools/hidl/utils/FQName.cpp
    if (!fqName.isValid()) {
        LOG(ERROR) << __FUNCTION__ << ": " << interfaceName << " is not a valid fully-qualified name ";
        return vintf::Transport::EMPTY;
    }
    if (!fqName.hasVersion()) {
        LOG(ERROR) << __FUNCTION__ << ": " << fqName.string() << " does not specify a version.";
        return vintf::Transport::EMPTY;
    }
    if (fqName.name().empty()) {
        LOG(ERROR) << __FUNCTION__ << ": " << fqName.string() << " does not specify an interface name.";
        return vintf::Transport::EMPTY;
    }

	//vintf::VintfObject::GetFrameworkHalManifest()		@system/libvintf/VintfObject.cpp
    vintf::Transport tr = getTransportFromManifest(fqName, instanceName,vintf::VintfObject::GetFrameworkHalManifest());
    if (tr != vintf::Transport::EMPTY) {
        return tr;
    }

    //vintf::VintfObject::GetDeviceHalManifest()		@system/libvintf/VintfObject.cpp
    tr = getTransportFromManifest(fqName, instanceName, vintf::VintfObject::GetDeviceHalManifest());
    if (tr != vintf::Transport::EMPTY) {
        return tr;
    }

    LOG(WARNING) << __FUNCTION__ << ": Cannot find entry " << fqName.string() << "/" << instanceName  << " in either framework or device manifest.";
    return vintf::Transport::EMPTY;
}



vintf::Transport getTransportFromManifest(const FQName &fqName, const std::string &instanceName,const vintf::HalManifest *vm) {
    if (vm == nullptr) {
        return vintf::Transport::EMPTY;
    }
    //	@system/libvintf/HalManifest.cpp
    return vm->getTransport(fqName.package(), vintf::Version{fqName.getPackageMajorVersion(), fqName.getPackageMinorVersion()},fqName.name(), instanceName);
}



// static
const HalManifest *VintfObject::GetDeviceHalManifest(bool skipCache =  false) {
	//	static LockedUniquePtr<HalManifest> gDeviceManifest;
    return Get(&gDeviceManifest, skipCache,std::bind(&HalManifest::fetchAllInformation, std::placeholders::_1,"/vendor/manifest.xml"));
}

// static
const HalManifest *VintfObject::GetFrameworkHalManifest(bool skipCache =  false) {
	//	static LockedUniquePtr<HalManifest> gFrameworkManifest;
    return Get(&gFrameworkManifest, skipCache,std::bind(&HalManifest::fetchAllInformation, std::placeholders::_1,"/system/manifest.xml"));
}



template <typename T, typename F>
static const T *Get(LockedUniquePtr<T> *ptr,bool skipCache,const F &fetchAllInformation) {
    std::unique_lock<std::mutex> _lock(ptr->mutex);
    if (skipCache || ptr->object == nullptr) {
        ptr->object = std::make_unique<T>();//std::make_unique<HalManifest>();
        if (fetchAllInformation(ptr->object.get()) != OK) {
            ptr->object = nullptr; // frees the old object
        }
    }
    return ptr->object.get();
}


status_t HalManifest::fetchAllInformation(const std::string &path) {
	//	@system/libvintf/parse_xml.cpp:817:const HalManifestConverter halManifestConverter{};
	//	@system/libvintf/parse_xml.cpp:946:const XmlConverter<HalManifest> &gHalManifestConverter = halManifestConverter;
    return details::fetchAllInformation(path, gHalManifestConverter, this);
}


/**
using NodeType = tinyxml2::XMLElement;
using DocType = tinyxml2::XMLDocument;
*/
//	@system/libvintf/parse_xml.cpp:136
template<typename Object>
struct XmlNodeConverter : public XmlConverter<Object> {
	 inline bool deserialize(Object *o, const std::string &xml) const {
        DocType *doc = createDocument(xml);
        if (doc == nullptr) {
            this->mLastError = "Not a valid XML";
            return false;
        }
        bool ret = deserialize(o, getRootChild(doc));
        deleteDocument(doc);
        return ret;
    }

    // caller is responsible for deleteDocument() call
	inline DocType *createDocument(const std::string &xml) {
	    DocType *doc = new tinyxml2::XMLDocument();
	    if (doc->Parse(xml.c_str()) == tinyxml2::XML_NO_ERROR) {
	        return doc;
	    }
	    delete doc;
	    return nullptr;
	}


    inline NodeType *getRootChild(DocType *parent) {
	    return parent->FirstChildElement();
	}


	inline bool deserialize(Object *object, NodeType *root) const {
        if (nameOf(root) != this->elementName()) {
            return false;
        }
        return this->buildObject(object, root);
    }


}


template <typename T>
status_t fetchAllInformation(const std::string& path, const XmlConverter<T>& converter,T* outObject) {
    std::string info;

    if (gFetcher == nullptr) {
        // Should never happen.
        return NO_INIT;
    }

    status_t result = gFetcher->fetch(path, info);//读取path内容到info

    if (result != OK) {
        return result;
    }

    bool success = converter(outObject, info);
    if (!success) {
        LOG(ERROR) << "Illformed file: " << path << ": " << converter.lastError();
        return BAD_VALUE;
    }
    return OK;
}


// Return the file from the given location as a string.
//
// This class can be used to create a mock for overriding.
class FileFetcher {
   public:
    virtual ~FileFetcher() {}
    virtual status_t fetch(const std::string& path, std::string& fetched) {
        std::ifstream in;

        in.open(path);
        if (!in.is_open()) {
            LOG(WARNING) << "Cannot open " << path;
            return INVALID_OPERATION;
        }

        std::stringstream ss;
        ss << in.rdbuf();
        fetched = ss.str();

        return OK;
    }
};





//	@system/libvintf/HalManifest.cpp
Transport HalManifest::getTransport(const std::string &package, const Version &v,const std::string &interfaceName, const std::string &instanceName) const {

    for (const ManifestHal *hal : getHals(package)) {
        bool found = false;
        for (auto& ver : hal->versions) {
            if (ver.majorVer == v.majorVer && ver.minorVer >= v.minorVer) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find " << to_string(v) << " in supported versions of " << package;
            continue;
        }
        auto it = hal->interfaces.find(interfaceName);
        if (it == hal->interfaces.end()) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find interface '" << interfaceName << "' in " << package << "@" << to_string(v);
            continue;
        }
        const auto &instances = it->second.instances;
        if (instances.find(instanceName) == instances.end()) {
            LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot find instance '" << instanceName << "' in "  << package << "@" << to_string(v) << "::" << interfaceName;
            continue;
        }
        return hal->transportArch.transport;
    }
    LOG(DEBUG) << "HalManifest::getTransport(" << to_string(mType) << "): Cannot get transport for " << package << "@" << v << "::" << interfaceName << "/" << instanceName;
    return Transport::EMPTY;

}








// Methods from ::android::hidl::manager::V1_0::IServiceManager follow.
Return<sp<IBase>> ServiceManager::get(const hidl_string& fqName,const hidl_string& name) {
    pid_t pid = IPCThreadState::self()->getCallingPid();
    if (!mAcl.canGet(fqName, pid)) {//selinux检测
        return nullptr;
    }


	 /*package::interface e.x. "android.hidl.manager@1.0::IServiceManager"*/
    //std::map<std::string,PackageInterfaceMap> mServiceMap;
    auto ifaceIt = mServiceMap.find(fqName);
    if (ifaceIt == mServiceMap.end()) {
        return nullptr;
    }

    const PackageInterfaceMap &ifaceMap = ifaceIt->second;
    const HidlService *hidlService = ifaceMap.lookup(name);

    if (hidlService == nullptr) {
        return nullptr;
    }

    return hidlService->getService();
}

//	@system/hwservicemanager/ServiceManager.h
struct PackageInterfaceMap {
    InstanceMap &getInstanceMap();
    const InstanceMap &getInstanceMap() const;

    /**
     * Finds a HidlService with the desired name. If none,
     * returns nullptr. HidlService::getService() might also be nullptr
     * if there are registered IServiceNotification objects for it. Return
     * value should be treated as a temporary reference.
     */
    HidlService *lookup(const std::string &name);
    const HidlService *lookup(const std::string &name) const;

    void insertService(std::unique_ptr<HidlService> &&service);

    void addPackageListener(sp<IServiceNotification> listener);
    bool removePackageListener(const wp<IBase>& who);
    bool removeServiceListener(const wp<IBase>& who);

    void sendPackageRegistrationNotification(const hidl_string &fqName, const hidl_string &instanceName);

private:
    InstanceMap mInstanceMap{};

    std::vector<sp<IServiceNotification>> mPackageListeners{};
};


const HidlService *ServiceManager::PackageInterfaceMap::lookup(const std::string &name) const {
    auto it = mInstanceMap.find(name);

    if (it == mInstanceMap.end()) {
        return nullptr;
    }

    return it->second.get();
}

HidlService *ServiceManager::PackageInterfaceMap::lookup(const std::string &name) {
   return const_cast<HidlService*>( const_cast<const PackageInterfaceMap*>(this)->lookup(name));
}

sp<IBase> HidlService::getService() const {
    return mService;//	sp<IBase>                             mService;//这里需要注意IBase
}

















Return<bool> ServiceManager::add(const hidl_string& name, const sp<IBase>& service) {
    bool isValidService = false;

    if (service == nullptr) {
        return false;
    }

    // TODO(b/34235311): use HIDL way to determine this
    // also, this assumes that the PID that is registering is the pid that is the service
    pid_t pid = IPCThreadState::self()->getCallingPid();
    auto context = mAcl.getContext(pid);

    //注意这里应该为IRadio函数中interfaceChain,不是IServiceManager中的
    auto ret = service->interfaceChain([&](const auto &interfaceChain) {
        if (interfaceChain.size() == 0) {
            return;
        }

        // First, verify you're allowed to add() the whole interface hierarchy
        for(size_t i = 0; i < interfaceChain.size(); i++) {
            std::string fqName = interfaceChain[i];

            if (!mAcl.canAdd(fqName, context, pid)) {
                return;
            }
        }

        for(size_t i = 0; i < interfaceChain.size(); i++) {
            std::string fqName = interfaceChain[i];

            PackageInterfaceMap &ifaceMap = mServiceMap[fqName];
            HidlService *hidlService = ifaceMap.lookup(name);

            if (hidlService == nullptr) {
                ifaceMap.insertService(std::make_unique<HidlService>(fqName, name, service, pid));
            } else {
                if (hidlService->getService() != nullptr) {
                    auto ret = hidlService->getService()->unlinkToDeath(this);
                    ret.isOk(); // ignore
                }
                hidlService->setService(service, pid);
            }

            ifaceMap.sendPackageRegistrationNotification(fqName, name);
        }

        auto linkRet = service->linkToDeath(this, 0 /*cookie*/);
        linkRet.isOk(); // ignore

        isValidService = true;
    });

    if (!ret.isOk()) {
        LOG(ERROR) << "Failed to retrieve interface chain.";
        return false;
    }

    return isValidService;
}



::android::hardware::Return<void> IRadio::interfaceChain(interfaceChain_cb _hidl_cb){
    _hidl_cb({
        IRadio::descriptor,
        ::android::hardware::radio::V1_0::IRadio::descriptor,
        ::android::hidl::base::V1_0::IBase::descriptor,
    });
    return ::android::hardware::Void();
}






Return<void> ServiceManager::registerPassthroughClient(const hidl_string &fqName,const hidl_string &name) {
    pid_t pid = IPCThreadState::self()->getCallingPid();
    if (!mAcl.canGet(fqName, pid)) {
        /* We guard this function with "get", because it's typically used in
         * the getService() path, albeit for a passthrough service in this
         * case
         */
        return Void();
    }

    PackageInterfaceMap &ifaceMap = mServiceMap[fqName];

    if (name.empty()) {
        LOG(WARNING) << "registerPassthroughClient encounters empty instance name for " << fqName.c_str();
        return Void();
    }

    HidlService *service = ifaceMap.lookup(name);

    if (service == nullptr) {
        auto adding = std::make_unique<HidlService>(fqName, name);
        adding->registerPassthroughClient(pid);
        ifaceMap.insertService(std::move(adding));
    } else {
        service->registerPassthroughClient(pid);
    }
    return Void();
}


void HidlService::registerPassthroughClient(pid_t pid) {
    mPassthroughClients.insert(pid);
}