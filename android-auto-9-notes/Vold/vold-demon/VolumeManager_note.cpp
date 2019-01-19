//  @system/vold/VolumeManager.cpp
VolumeManager *VolumeManager::Instance() {
    if (!sInstance)
        sInstance = new VolumeManager();
    return sInstance;
}

VolumeManager::VolumeManager() {
    mDebug = false;
    mNextObbId = 0;
    // For security reasons, assume that a secure keyguard is
    // showing until we hear otherwise
    mSecureKeyguardShowing = true;
}


int VolumeManager::start() {
    ATRACE_NAME("VolumeManager::start");

    // Always start from a clean slate by unmounting everything in
    // directories that we own, in case we crashed.
    unmountAll();

    Devmapper::destroyAll();
    Loop::destroyAll();

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
    ATRACE_NAME("VolumeManager::unmountAll()");

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
        PLOG(ERROR) << "Failed to open /proc/mounts";
        return -errno;
    }

    // Some volumes can be stacked on each other, so force unmount in
    // reverse order to give us the best chance of success.
    std::list<std::string> toUnmount;
    mntent* mentry;
    while ((mentry = getmntent(fp)) != NULL) {
        auto test = std::string(mentry->mnt_dir);
        if ((android::base::StartsWith(test, "/mnt/") &&
             !android::base::StartsWith(test, "/mnt/vendor")) ||
            android::base::StartsWith(test, "/storage/")) {
            toUnmount.push_front(test);
        }
    }
    endmntent(fp);

    for (const auto& path : toUnmount) {
        LOG(DEBUG) << "Tearing down stale mount " << path;
        android::vold::ForceUnmount(path);
    }

    return 0;
}


int VolumeManager::updateVirtualDisk() {
    ATRACE_NAME("VolumeManager::updateVirtualDisk");

    //  static const char* kPropVirtualDisk = "persist.sys.virtual_disk";   //当前系统为空
    if (android::base::GetBoolProperty(kPropVirtualDisk, false)) {
        if (access(kPathVirtualDisk, F_OK) != 0) {
            Loop::createImageFile(kPathVirtualDisk, kSizeVirtualDisk / 512);
        }

        if (mVirtualDisk == nullptr) {
            if (Loop::create(kPathVirtualDisk, mVirtualDiskPath) != 0) {
                LOG(ERROR) << "Failed to create virtual disk";
                return -1;
            }

            struct stat buf;
            if (stat(mVirtualDiskPath.c_str(), &buf) < 0) {
                PLOG(ERROR) << "Failed to stat " << mVirtualDiskPath;
                return -1;
            }

            auto disk = new android::vold::Disk("virtual", buf.st_rdev, "virtual",android::vold::Disk::Flags::kAdoptable | android::vold::Disk::Flags::kSd);
            mVirtualDisk = std::shared_ptr<android::vold::Disk>(disk);
            handleDiskAdded(mVirtualDisk);
        }
    } else {
        if (mVirtualDisk != nullptr) {
            dev_t device = mVirtualDisk->getDevice();
            handleDiskRemoved(device);

            Loop::destroyByDevice(mVirtualDiskPath.c_str());
            mVirtualDisk = nullptr;
        }

        //  static const char* kPathVirtualDisk = "/data/misc/vold/virtual_disk";
        if (access(kPathVirtualDisk, F_OK) == 0) {  //  /data/misc/vold/virtual_disk 路径不存在
            unlink(kPathVirtualDisk);
        }
    }
    return 0;
}


//  /devices/platform/5b110000.usb3/xhci-cdns3/usb1/1-1/1-1.4/1-1.4:1.0/host0/target0:0:0/0:0:0:0/block/sda

/*
# Android fstab file.
#<src>                                                  <mnt_point>         <type>    <mnt_flags and options>                       <fs_mgr_flags>
# The filesystem that contains the filesystem checker binary (typically /system) cannot
# specify MF_CHECK, and must come before any filesystems that do specify MF_CHECK

/devices/platform/5b0d0000.usb/ci_hdrc.0/* auto auto defaults voldmanaged=usb:auto
/dev/block/by-name/system      /            ext4    ro,barrier=1                                                                     wait,slotselect
/dev/block/by-name/userdata    /data        ext4    nosuid,nodev,nodiratime,noatime,nomblk_io_submit,noauto_da_alloc,errors=panic    latemount,wait,formattable,fileencryption=software
/dev/block/by-name/misc        /misc        emmc    defaults                                                                         defaults
*/