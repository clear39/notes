//system/hwservicemanager/ServiceManager.cpp

Return<bool> ServiceManager::add(const hidl_string& name, const sp<IBase>& service) {
    bool isValidService = false;

    if (service == nullptr) {
        return false;
    }

    // TODO(b/34235311): use HIDL way to determine this
    // also, this assumes that the PID that is registering is the pid that is the service
    pid_t pid = IPCThreadState::self()->getCallingPid();

    auto ret = service->interfaceChain([&](const auto &interfaceChain) {
        if (interfaceChain.size() == 0) {
            return;
        }

        // First, verify you're allowed to add() the whole interface hierarchy
        for(size_t i = 0; i < interfaceChain.size(); i++) {
            std::string fqName = interfaceChain[i];

            if (!mAcl.canAdd(fqName, pid)) {
                return;
            }
        }

        for(size_t i = 0; i < interfaceChain.size(); i++) {
            std::string fqName = interfaceChain[i];

            PackageInterfaceMap &ifaceMap = mServiceMap[fqName];
            HidlService *hidlService = ifaceMap.lookup(name);

            if (hidlService == nullptr) {
                ifaceMap.insertService(
                    std::make_unique<HidlService>(fqName, name, service, pid));
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
