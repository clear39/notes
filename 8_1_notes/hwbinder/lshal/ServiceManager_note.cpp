//	@system/hwservicemanager/ServiceManager.cpp

Return<void> ServiceManager::list(list_cb _hidl_cb) {
    pid_t pid = IPCThreadState::self()->getCallingPid();
    if (!mAcl.canList(pid)) {
        _hidl_cb({});
        return Void();
    }

    hidl_vec<hidl_string> list;

    list.resize(countExistingService());

    size_t idx = 0;
    forEachExistingService([&] (const HidlService *service) {
        list[idx++] = service->string();
    });

    _hidl_cb(list);
    return Void();
}


size_t ServiceManager::countExistingService() const {
    size_t total = 0;
    forEachExistingService([&] (const HidlService *) {
        ++total;
    });
    return total;
}


void ServiceManager::forEachExistingService(std::function<void(const HidlService *)> f) const {
    forEachServiceEntry([f] (const HidlService *service) {
        if (service->getService() == nullptr) {
            return;
        }
        f(service);
    });
}

/*
    std::map<
        std::string, // package::interface e.x. "android.hidl.manager@1.0::IServiceManager"
        PackageInterfaceMap
    > mServiceMap;



*/
void ServiceManager::forEachServiceEntry(std::function<void(const HidlService *)> f) const {
    for (const auto &interfaceMapping : mServiceMap) {
        const auto &instanceMap = interfaceMapping.second.getInstanceMap();

        for (const auto &instanceMapping : instanceMap) {
            f(instanceMapping.second.get());
        }
    }
}


using InstanceMap = std::map<
            std::string, // instance name e.x. "manager"
            std::unique_ptr<HidlService>
        >;

    struct PackageInterfaceMap {
        InstanceMap &getInstanceMap();
        const InstanceMap &getInstanceMap() const;

        /**
         * Finds a HidlService with the desired name. If none,
         * returns nullptr. HidlService::getService() might also be nullptr
         * if there are registered IServiceNotification objects for it. Return
         * value should be treated as a temporary reference.
         */
        HidlService *lookup(
            const std::string &name);
        const HidlService *lookup(
            const std::string &name) const;

        void insertService(std::unique_ptr<HidlService> &&service);

        void addPackageListener(sp<IServiceNotification> listener);
        bool removePackageListener(const wp<IBase>& who);
        bool removeServiceListener(const wp<IBase>& who);

        void sendPackageRegistrationNotification(
            const hidl_string &fqName,
            const hidl_string &instanceName);

    private:
        InstanceMap mInstanceMap{};

        std::vector<sp<IServiceNotification>> mPackageListeners{};
    };








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


//关于文件解析可一个看manifest.xml_parse_note.cpp
vintf::Transport getTransportFromManifest(const FQName &fqName, const std::string &instanceName,const vintf::HalManifest *vm) {
    if (vm == nullptr) {
        return vintf::Transport::EMPTY;
    }
    //	@system/libvintf/HalManifest.cpp
    return vm->getTransport(fqName.package(), vintf::Version{fqName.getPackageMajorVersion(), fqName.getPackageMinorVersion()},fqName.name(), instanceName);
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