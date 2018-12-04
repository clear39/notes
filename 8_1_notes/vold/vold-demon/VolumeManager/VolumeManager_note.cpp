//	@system/vold/VolumeManager.cpp

VolumeManager *VolumeManager::Instance() {
    if (!sInstance)
        sInstance = new VolumeManager();
    return sInstance;
}

VolumeManager::VolumeManager() {
    mDebug = false;
    mActiveContainers = new AsecIdCollection();	//	typedef android::List<ContainerData*> AsecIdCollection;
    mBroadcaster = NULL;
    mUmsSharingCount = 0;
    mSavedDirtyRatio = -1;
    // set dirty ratio to 0 when UMS is active
    mUmsDirtyRatio = 0;
}



int VolumeManager::start() {
    // Always start from a clean slate by unmounting everything in
    // directories that we own, in case we crashed.
    unmountAll();

    // Assume that we always have an emulated volume on internal
    // storage; the framework will decide if it should be mounted.
    CHECK(mInternalEmulated == nullptr);
    mInternalEmulated = std::shared_ptr<android::vold::VolumeBase>(new android::vold::EmulatedVolume("/data/media"));
    mInternalEmulated->create();

    // Consider creating a virtual disk
    updateVirtualDisk();

    return 0;
}

int VolumeManager::unmountAll() {
    std::lock_guard<std::mutex> lock(mLock);

    // First, try gracefully unmounting all known devices
    if (mInternalEmulated != nullptr) {
        mInternalEmulated->unmount();
    }
    for (const auto& disk : mDisks) {
        disk->unmountAll();
    }

    // Worst case we might have some stale mounts lurking around, so
    // force unmount those just to be safe.
    FILE* fp = setmntent("/proc/mounts", "r");
    if (fp == NULL) {
        SLOGE("Error opening /proc/mounts: %s", strerror(errno));
        return -errno;
    }

    // Some volumes can be stacked on each other, so force unmount in
    // reverse order to give us the best chance of success.
    std::list<std::string> toUnmount;
    mntent* mentry;
    while ((mentry = getmntent(fp)) != NULL) {
        if (strncmp(mentry->mnt_dir, "/mnt/", 5) == 0
                || strncmp(mentry->mnt_dir, "/storage/", 9) == 0) {
            toUnmount.push_front(std::string(mentry->mnt_dir));
        }
    }
    endmntent(fp);

    for (const auto& path : toUnmount) {
        SLOGW("Tearing down stale mount %s", path.c_str());
        android::vold::ForceUnmount(path);
    }

    return 0;
}


/* 第一次
12-19 22:07:44.651  4902  4915 V vold    : ----------------
12-19 22:07:44.650  4902  4915 V vold    : handleBlockEvent with action 1
12-19 22:07:44.650  4902  4915 D NetlinkEvent: NL param 'DEVPATH=/devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda'
12-19 22:07:44.650  4902  4915 D NetlinkEvent: NL param 'MAJOR=8'
12-19 22:07:44.650  4902  4915 D NetlinkEvent: NL param 'MINOR=0'
12-19 22:07:44.650  4902  4915 D NetlinkEvent: NL param 'DEVNAME=sda'
12-19 22:07:44.650  4902  4915 D NetlinkEvent: NL param 'DEVTYPE=disk'
*/



/** 第二次
12-19 22:07:44.596  4902  4915 V vold    : ----------------
12-19 22:07:44.596  4902  4915 V vold    : handleBlockEvent with action 1
12-19 22:07:44.596  4902  4915 D NetlinkEvent: NL param 'DEVPATH=/devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda/sda1'
12-19 22:07:44.596  4902  4915 D NetlinkEvent: NL param 'MAJOR=8'
12-19 22:07:44.596  4902  4915 D NetlinkEvent: NL param 'MINOR=1'
12-19 22:07:44.596  4902  4915 D NetlinkEvent: NL param 'DEVNAME=sda1'
12-19 22:07:44.595  4902  4915 D NetlinkEvent: NL param 'DEVTYPE=partition'
12-19 22:07:44.595  4902  4915 D NetlinkEvent: NL param 'PARTN=1'
*/

void VolumeManager::handleBlockEvent(NetlinkEvent *evt) {
    std::lock_guard<std::mutex> lock(mLock);

    if (mDebug) {
        LOG(VERBOSE) << "----------------";
        LOG(VERBOSE) << "handleBlockEvent with action " << (int) evt->getAction();
        evt->dump();
    }

    std::string eventPath(evt->findParam("DEVPATH")?evt->findParam("DEVPATH"):"");
    std::string devType(evt->findParam("DEVTYPE")?evt->findParam("DEVTYPE"):"");

    if (devType != "disk") return;

    int major = atoi(evt->findParam("MAJOR"));
    int minor = atoi(evt->findParam("MINOR"));
    dev_t device = makedev(major, minor);

    switch (evt->getAction()) {
    case NetlinkEvent::Action::kAdd: {
        for (const auto& source : mDiskSources) {
            if (source->matches(eventPath)) {
                // For now, assume that MMC and virtio-blk (the latter is
                // emulator-specific; see Disk.cpp for details) devices are SD,
                // and that everything else is USB
                int flags = source->getFlags();
                if (major == kMajorBlockMmc
                    || (android::vold::IsRunningInEmulator()
                    && major >= (int) kMajorBlockExperimentalMin
                    && major <= (int) kMajorBlockExperimentalMax)) {
                    flags |= android::vold::Disk::Flags::kSd;
                } else {
                    flags |= android::vold::Disk::Flags::kUsb;
                }

                auto disk = new android::vold::Disk(eventPath, device,
                        source->getNickname(), flags);
                disk->create();
                mDisks.push_back(std::shared_ptr<android::vold::Disk>(disk));
                break;
            }
        }
        break;
    }
    case NetlinkEvent::Action::kChange: {
        LOG(DEBUG) << "Disk at " << major << ":" << minor << " changed";
        for (const auto& disk : mDisks) {
            if (disk->getDevice() == device) {
                disk->readMetadata();
                disk->readPartitions();
            }
        }
        break;
    }
    case NetlinkEvent::Action::kRemove: {
        auto i = mDisks.begin();
        while (i != mDisks.end()) {
            if ((*i)->getDevice() == device) {
                (*i)->destroy();
                i = mDisks.erase(i);
            } else {
                ++i;
            }
        }
        break;
    }
    default: {
        LOG(WARNING) << "Unexpected block event action " << (int) evt->getAction();
        break;
    }
    }
}






//接收到 StorageManagerService 发送过来的 D VoldConnector: SND -> {31 volume mount public:8,1 2 0}
std::shared_ptr<android::vold::VolumeBase> VolumeManager::findVolume(const std::string& id) {// id = "public:8,1"
    // Vold could receive "mount" after "shutdown" command in the extreme case.
    // If this happens, mInternalEmulated will equal nullptr and
    // we need to deal with it in order to avoid null pointer crash.
    if (mInternalEmulated != nullptr && mInternalEmulated->getId() == id) {
        return mInternalEmulated;
    }
    for (const auto& disk : mDisks) {
        auto vol = disk->findVolume(id);
        if (vol != nullptr) {
            return vol;
        }
    }
    return nullptr;
}

