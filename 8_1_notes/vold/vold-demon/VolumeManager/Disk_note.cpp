Disk::Disk(const std::string& eventPath, dev_t device,
        const std::string& nickname, int flags) :
        mDevice(device), mSize(-1), mNickname(nickname), mFlags(flags), mCreated(false), mJustPartitioned(false) {
    mId = StringPrintf("disk:%u,%u", major(device), minor(device));
    mEventPath = eventPath;//       /devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda
    mSysPath = StringPrintf("/sys/%s", eventPath.c_str());//    /sys//devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda
    mDevPath = StringPrintf("/dev/block/vold/%s", mId.c_str()); //  /dev/block/vold/disk:8,0
    CreateDeviceNode(mDevPath, mDevice);//	@system/vold/Utils.cpp
}



status_t CreateDeviceNode(const std::string& path, dev_t dev) {
    const char* cpath = path.c_str();
    status_t res = 0;

    char* secontext = nullptr;
    if (sehandle) {
        if (!selabel_lookup(sehandle, &secontext, cpath, S_IFBLK)) {
            setfscreatecon(secontext);
        }
    }

    mode_t mode = 0660 | S_IFBLK;
    if (mknod(cpath, mode, dev) < 0) {//    cpath = "/dev/block/vold/disk:8,0"
        if (errno != EEXIST) {
            PLOG(ERROR) << "Failed to create device node for " << major(dev) << ":" << minor(dev) << " at " << path;
            res = -errno;
        }
    }

    if (secontext) {
        setfscreatecon(nullptr);
        freecon(secontext);
    }

    return res;
}




status_t Disk::create() {
    CHECK(!mCreated);
    mCreated = true;
    //  D VoldConnector: RCV <- {640 disk:8,0 8}
    notifyEvent(ResponseCode::DiskCreated, StringPrintf("%d", mFlags));//   system/vold/ResponseCode.h:69:    static const int DiskCreated = 640;
    readMetadata();
    readPartitions();
    return OK;
}




status_t Disk::readMetadata() {
    mSize = -1;
    mLabel.clear();

    
    int fd = open(mDevPath.c_str(), O_RDONLY | O_CLOEXEC);//  /dev/block/vold/disk:8,0
    if (fd != -1) {
        if (ioctl(fd, BLKGETSIZE64, &mSize)) {//获取大小
            mSize = -1;
        }
        close(fd);
    }

    unsigned int majorId = major(mDevice);//    8
    switch (majorId) {
    case kMajorBlockLoop: {
        mLabel = "Virtual";
        break;
    }
    //  system/vold/Disk.cpp:56:static const unsigned int kMajorBlockScsiA = 8;
    case kMajorBlockScsiA: case kMajorBlockScsiB: case kMajorBlockScsiC: case kMajorBlockScsiD:
    case kMajorBlockScsiE: case kMajorBlockScsiF: case kMajorBlockScsiG: case kMajorBlockScsiH:
    case kMajorBlockScsiI: case kMajorBlockScsiJ: case kMajorBlockScsiK: case kMajorBlockScsiL:
    case kMajorBlockScsiM: case kMajorBlockScsiN: case kMajorBlockScsiO: case kMajorBlockScsiP: {
        std::string path(mSysPath + "/device/vendor");//    /sys/devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda/device/vendor
        std::string tmp;
        if (!ReadFileToString(path, &tmp)) {
            PLOG(WARNING) << "Failed to read vendor from " << path;
            return -errno;
        }
        mLabel = tmp;// General
        break;
    }
    case kMajorBlockMmc: {
        std::string path(mSysPath + "/device/manfid");
        std::string tmp;
        if (!ReadFileToString(path, &tmp)) {
            PLOG(WARNING) << "Failed to read manufacturer from " << path;
            return -errno;
        }
        uint64_t manfid = strtoll(tmp.c_str(), nullptr, 16);
        // Our goal here is to give the user a meaningful label, ideally
        // matching whatever is silk-screened on the card.  To reduce
        // user confusion, this list doesn't contain white-label manfid.
        switch (manfid) {
        case 0x000003: mLabel = "SanDisk"; break;
        case 0x00001b: mLabel = "Samsung"; break;
        case 0x000028: mLabel = "Lexar"; break;
        case 0x000074: mLabel = "Transcend"; break;
        }
        break;
    }
    default: {
        if (isVirtioBlkDevice(majorId)) {
            LOG(DEBUG) << "Recognized experimental block major ID " << majorId
                    << " as virtio-blk (emulator's virtual SD card device)";
            mLabel = "Virtual";
            break;
        }
        LOG(WARNING) << "Unsupported block major type " << majorId;
        return -ENOTSUP;
    }
    }

    /*
    通知信息更新
    */
    //  VoldConnector: RCV <- {641 disk:8,0 16025387008}
    notifyEvent(ResponseCode::DiskSizeChanged, StringPrintf("%" PRIu64, mSize));
    //  VoldConnector: RCV <- {642 disk:8,0 General }
    notifyEvent(ResponseCode::DiskLabelChanged, mLabel);

    //  VoldConnector: RCV <- {644 disk:8,0 /sys//devices/platform/5b110000.cdns3/xhci-cdns3/usb1/1-1/1-1.3/1-1.3:1.0/host0/target0:0:0/0:0:0:0/block/sda}
    notifyEvent(ResponseCode::DiskSysPathChanged, mSysPath);
    return OK;
}



status_t Disk::readPartitions() {
    int8_t maxMinors = getMaxMinors();
    if (maxMinors < 0) {
        return -ENOTSUP;
    }

    destroyAllVolumes();

    // Parse partition table

    std::vector<std::string> cmd;
    cmd.push_back(kSgdiskPath); //  system/vold/Disk.cpp:49:static const char* kSgdiskPath = "/system/bin/sgdisk";
    cmd.push_back("--android-dump");
    cmd.push_back(mDevPath);    //  /dev/block/vold/disk:8,0

    std::vector<std::string> output;
    status_t res = ForkExecvp(cmd, output);
    if (res != OK) {
        LOG(WARNING) << "sgdisk failed to scan " << mDevPath;
        notifyEvent(ResponseCode::DiskScanned);
        mJustPartitioned = false;
        return res;
    }

    Table table = Table::kUnknown;
    bool foundParts = false;
    for (const auto& line : output) {
        char* cline = (char*) line.c_str();
        char* token = strtok(cline, kSgdiskToken);
        if (token == nullptr) continue;

        if (!strcmp(token, "DISK")) {
            const char* type = strtok(nullptr, kSgdiskToken);
            if (!strcmp(type, "mbr")) {
                table = Table::kMbr;
            } else if (!strcmp(type, "gpt")) {
                table = Table::kGpt;
            }
        } else if (!strcmp(token, "PART")) {
            foundParts = true;
            int i = strtol(strtok(nullptr, kSgdiskToken), nullptr, 10);
            if (i <= 0 || i > maxMinors) {
                LOG(WARNING) << mId << " is ignoring partition " << i << " beyond max supported devices";
                continue;
            }
            dev_t partDevice = makedev(major(mDevice), minor(mDevice) + i);

            if (table == Table::kMbr) {
                const char* type = strtok(nullptr, kSgdiskToken);

                switch (strtol(type, nullptr, 16)) {
                case 0x06: // FAT16
                case 0x0b: // W95 FAT32 (LBA)
                case 0x0c: // W95 FAT32 (LBA)
                case 0x0e: // W95 FAT16 (LBA)
                    createPublicVolume(partDevice);
                    break;
                }
            } else if (table == Table::kGpt) {
                const char* typeGuid = strtok(nullptr, kSgdiskToken);
                const char* partGuid = strtok(nullptr, kSgdiskToken);

                if (!strcasecmp(typeGuid, kGptBasicData)) {
                    createPublicVolume(partDevice);
                } else if (!strcasecmp(typeGuid, kGptAndroidExpand)) {
                    createPrivateVolume(partDevice, partGuid);
                }
            }
        }
    }

    // Ugly last ditch effort, treat entire disk as partition
    if (table == Table::kUnknown || !foundParts) {
        LOG(WARNING) << mId << " has unknown partition table; trying entire device";

        std::string fsType;
        std::string unused;
        if (ReadMetadataUntrusted(mDevPath, fsType, unused, unused) == OK) {
            createPublicVolume(mDevice);
        } else {
            LOG(WARNING) << mId << " failed to identify, giving up";
        }
    }

    //system/vold/ResponseCode.h:72:    static const int DiskScanned = 643;
    notifyEvent(ResponseCode::DiskScanned);//   D VoldConnector: RCV <- {643 disk:8,0}
    mJustPartitioned = false;
    return OK;
}



status_t ForkExecvp(const std::vector<std::string>& args,std::vector<std::string>& output) {
    return ForkExecvp(args, output, nullptr);
}

status_t ForkExecvp(const std::vector<std::string>& args, std::vector<std::string>& output, security_context_t context) {
    std::string cmd;
    for (size_t i = 0; i < args.size(); i++) {
        cmd += args[i] + " ";
        if (i == 0) {
            LOG(VERBOSE) << args[i];
        } else {
            LOG(VERBOSE) << "    " << args[i];
        }
    }
    output.clear();

    if (setexeccon(context)) {
        LOG(ERROR) << "Failed to setexeccon";
        abort();
    }
    FILE* fp = popen(cmd.c_str(), "r"); // NOLINT
    if (setexeccon(nullptr)) {
        LOG(ERROR) << "Failed to setexeccon";
        abort();
    }

    if (!fp) {
        PLOG(ERROR) << "Failed to popen " << cmd;
        return -errno;
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp) != nullptr) {
        LOG(VERBOSE) << line;
        output.push_back(std::string(line));
    }
    if (pclose(fp) != 0) {
        PLOG(ERROR) << "Failed to pclose " << cmd;
        return -errno;
    }

    return OK;
}




void Disk::createPublicVolume(dev_t device) {
    auto vol = std::shared_ptr<VolumeBase>(new PublicVolume(device));
    if (mJustPartitioned) {// false
        LOG(DEBUG) << "Device just partitioned; silently formatting";
        vol->setSilent(true);
        vol->create();
        vol->format("auto");
        vol->destroy();
        vol->setSilent(false);
    }

    mVolumes.push_back(vol);
    vol->setDiskId(getId());
    vol->create();
}

//  @system/vold/PublicVolume.cpp
PublicVolume::PublicVolume(dev_t device) :VolumeBase(Type::kPublic), mDevice(device), mFusePid(0) {
    setId(StringPrintf("public:%u,%u", major(device), minor(device)));//    public:8,1
    mDevPath = StringPrintf("/dev/block/vold/%s", getId().c_str());//   /dev/block/vold/public:8,1
}


status_t PublicVolume::doCreate() {
    return CreateDeviceNode(mDevPath, mDevice);
}


status_t VolumeBase::create() {
    CHECK(!mCreated);

    mCreated = true;
    status_t res = doCreate();
    //  D VoldConnector: RCV <- {650 public:8,1 0 "disk:8,0" ""}    //这里触发SND
    notifyEvent(ResponseCode::VolumeCreated,StringPrintf("%d \"%s\" \"%s\"", mType, mDiskId.c_str(), mPartGuid.c_str()));
    setState(State::kUnmounted);
    return res;
}


void VolumeBase::setState(State state) {
    mState = state;
    notifyEvent(ResponseCode::VolumeStateChanged, StringPrintf("%d", mState));//  D VoldConnector: RCV <- {651 public:8,1 0}
}

//到这里，创建Disk 和 PublicVolume 已经完成
//////////////////////////////////////////////////////////////////////////////

//接收到 StorageManagerService 发送过来的 D VoldConnector: SND -> {31 volume mount public:8,1 2 0}
std::shared_ptr<VolumeBase> Disk::findVolume(const std::string& id) { //        id = "public:8,1"
    for (auto vol : mVolumes) {
        if (vol->getId() == id) {
            return vol;
        }
        auto stackedVol = vol->findVolume(id);
        if (stackedVol != nullptr) {
            return stackedVol;
        }
    }
    return nullptr;
}



status_t PublicVolume::doMount() {
    // TODO: expand to support mounting other filesystems
    readMetadata();   //这里通过/system/bin/blkid工具读取 文件系统类型，mFsUuid，mFsLabel 并上报给StorageManagerService

    if (mFsType != "vfat") {
        LOG(ERROR) << getId() << " unsupported filesystem " << mFsType;
        return -EIO;
    }

    if (vfat::Check(mDevPath)) {//  @system/vold/fs/Vfat.cpp   通过/system/bin/fsck_msdos工具校验文件系统
        LOG(ERROR) << getId() << " failed filesystem check";
        return -EIO;
    }

    // Use UUID as stable name, if available
    std::string stableName = getId();// "public:8,1"
    if (!mFsUuid.empty()) {
        stableName = mFsUuid;//在readMetadata中获取
    }

    mRawPath = StringPrintf("/mnt/media_rw/%s", stableName.c_str());

    mFuseDefault = StringPrintf("/mnt/runtime/default/%s", stableName.c_str());
    mFuseRead = StringPrintf("/mnt/runtime/read/%s", stableName.c_str());
    mFuseWrite = StringPrintf("/mnt/runtime/write/%s", stableName.c_str());

    //  12-19 22:07:45.271  3588  3726 D VoldConnector: RCV <- {656 public:8,1 /mnt/media_rw/5243-5977}
    setInternalPath(mRawPath);
    if (getMountFlags() & MountFlags::kVisible) {
        setPath(StringPrintf("/storage/%s", stableName.c_str()));   //mPath="/storage/5243-5977"
    } else {
        setPath(mRawPath);
    }

    if (fs_prepare_dir(mRawPath.c_str(), 0700, AID_ROOT, AID_ROOT)) {// system/core/libcutils/fs.c:109
        PLOG(ERROR) << getId() << " failed to create mount points";
        return -errno;
    }

    if (vfat::Mount(mDevPath, mRawPath, false, false, false, AID_MEDIA_RW, AID_MEDIA_RW, 0007, true)) {
        PLOG(ERROR) << getId() << " failed to mount " << mDevPath;
        return -EIO;
    }

    if (getMountFlags() & MountFlags::kPrimary) {
        initAsecStage();
    }

    if (!(getMountFlags() & MountFlags::kVisible)) {
        // Not visible to apps, so no need to spin up FUSE
        return OK;
    }

    if (fs_prepare_dir(mFuseDefault.c_str(), 0700, AID_ROOT, AID_ROOT) ||
            fs_prepare_dir(mFuseRead.c_str(), 0700, AID_ROOT, AID_ROOT) ||
            fs_prepare_dir(mFuseWrite.c_str(), 0700, AID_ROOT, AID_ROOT)) {
        PLOG(ERROR) << getId() << " failed to create FUSE mount points";
        return -errno;
    }

    dev_t before = GetDevice(mFuseWrite);

    //  system/vold/PublicVolume.cpp:41:static const char* kFusePath = "/system/bin/sdcard";
    if (!(mFusePid = fork())) {
        if (getMountFlags() & MountFlags::kPrimary) {
            if (execl(kFusePath, kFusePath,
                    "-u", "1023", // AID_MEDIA_RW
                    "-g", "1023", // AID_MEDIA_RW
                    "-U", std::to_string(getMountUserId()).c_str(),
                    "-w",
                    mRawPath.c_str(),
                    stableName.c_str(),
                    NULL)) {
                PLOG(ERROR) << "Failed to exec";
            }
        } else {
            if (execl(kFusePath, kFusePath,
                    "-u", "1023", // AID_MEDIA_RW
                    "-g", "1023", // AID_MEDIA_RW
                    "-U", std::to_string(getMountUserId()).c_str(),
                    mRawPath.c_str(),
                    stableName.c_str(),
                    NULL)) {
                PLOG(ERROR) << "Failed to exec";
            }
        }

        LOG(ERROR) << "FUSE exiting";
        _exit(1);
    }

    if (mFusePid == -1) {
        PLOG(ERROR) << getId() << " failed to fork";
        return -errno;
    }

    while (before == GetDevice(mFuseWrite)) {
        LOG(VERBOSE) << "Waiting for FUSE to spin up...";
        usleep(50000); // 50ms
    }
    /* sdcardfs will have exited already. FUSE will still be running */
    TEMP_FAILURE_RETRY(waitpid(mFusePid, nullptr, WNOHANG));

    return OK;
}



status_t PublicVolume::readMetadata() {
    status_t res = ReadMetadataUntrusted(mDevPath, mFsType, mFsUuid, mFsLabel);//   @system/vold/Utils.cpp:240

    //  12-19 22:07:44.522  3588  3726 D VoldConnector: RCV <- {652 public:8,1 vfat}
    notifyEvent(ResponseCode::VolumeFsTypeChanged, mFsType);

    //  12-19 22:07:44.521  3588  3726 D VoldConnector: RCV <- {653 public:8,1 5243-5977}
    notifyEvent(ResponseCode::VolumeFsUuidChanged, mFsUuid);

    //  12-19 22:07:44.521  3588  3726 D VoldConnector: RCV <- {654 public:8,1 }
    notifyEvent(ResponseCode::VolumeFsLabelChanged, mFsLabel);
    return res;
}



status_t ReadMetadataUntrusted(const std::string& path, std::string& fsType,std::string& fsUuid, std::string& fsLabel) {
    return readMetadata(path, fsType, fsUuid, fsLabel, true);
}



static status_t readMetadata(const std::string& path, std::string& fsType,
        std::string& fsUuid, std::string& fsLabel, bool untrusted) {
    fsType.clear();
    fsUuid.clear();
    fsLabel.clear();



/**
12-19 22:07:44.594  4902  4917 V vold    : /system/bin/blkid
12-19 22:07:44.594  4902  4917 V vold    :     -c
12-19 22:07:44.594  4902  4917 V vold    :     /dev/null
12-19 22:07:44.594  4902  4917 V vold    :     -s
12-19 22:07:44.594  4902  4917 V vold    :     TYPE
12-19 22:07:44.594  4902  4917 V vold    :     -s
12-19 22:07:44.594  4902  4917 V vold    :     UUID
12-19 22:07:44.594  4902  4917 V vold    :     -s
12-19 22:07:44.594  4902  4917 V vold    :     LABEL
12-19 22:07:44.594  4902  4917 V vold    :     /dev/block/vold/public:8,1
*/
    std::vector<std::string> cmd;
    cmd.push_back(kBlkidPath);  //  @system/vold/Utils.cpp:57:static const char* kBlkidPath = "/system/bin/blkid";
    cmd.push_back("-c");
    cmd.push_back("/dev/null");
    cmd.push_back("-s");
    cmd.push_back("TYPE");
    cmd.push_back("-s");
    cmd.push_back("UUID");
    cmd.push_back("-s");
    cmd.push_back("LABEL");
    cmd.push_back(path);

    std::vector<std::string> output;
    status_t res = ForkExecvp(cmd, output, untrusted ? sBlkidUntrustedContext : sBlkidContext);
    if (res != OK) {
        LOG(WARNING) << "blkid failed to identify " << path;
        return res;
    }

    char value[128];
    for (const auto& line : output) {
        // Extract values from blkid output, if defined
        const char* cline = line.c_str();
        const char* start = strstr(cline, "TYPE=");
        if (start != nullptr && sscanf(start + 5, "\"%127[^\"]\"", value) == 1) {
            fsType = value;
        }

        start = strstr(cline, "UUID=");
        if (start != nullptr && sscanf(start + 5, "\"%127[^\"]\"", value) == 1) {
            fsUuid = value;
        }

        start = strstr(cline, "LABEL=");
        if (start != nullptr && sscanf(start + 6, "\"%127[^\"]\"", value) == 1) {
            fsLabel = value;
        }
    }

    return OK;
}






//  @system/vold/fs/Vfat.cpp
status_t Check(const std::string& source) {
    if (access(kFsckPath, X_OK)) {//    system/vold/fs/Vfat.cpp:59:static const char* kFsckPath = "/system/bin/fsck_msdos";
        SLOGW("Skipping fs checks\n");
        return 0;
    }

    int pass = 1;
    int rc = 0;
    do {
        /**
        12-19 22:07:44.521  4902  4917 V vold    : /system/bin/fsck_msdos
        12-19 22:07:44.521  4902  4917 V vold    :     -p
        12-19 22:07:44.521  4902  4917 V vold    :     -f
        12-19 22:07:44.521  4902  4917 V vold    :     /dev/block/vold/public:8,1
        */
        std::vector<std::string> cmd;
        cmd.push_back(kFsckPath);
        cmd.push_back("-p");
        cmd.push_back("-f");
        cmd.push_back(source);

        // Fat devices are currently always untrusted
        rc = ForkExecvp(cmd, sFsckUntrustedContext);


        //  12-19 22:07:45.272  4902  4917 I Vold    : Filesystem check completed OK
        if (rc < 0) {
            SLOGE("Filesystem check failed due to logwrap error");
            errno = EIO;
            return -1;
        }

        switch(rc) {
        case 0:
            SLOGI("Filesystem check completed OK");
            return 0;

        case 2:
            SLOGE("Filesystem check failed (not a FAT filesystem)");
            errno = ENODATA;
            return -1;

        case 4:
            if (pass++ <= 3) {
                SLOGW("Filesystem modified - rechecking (pass %d)",pass);
                continue;
            }
            SLOGE("Failing check after too many rechecks");
            errno = EIO;
            return -1;

        case 8:
            SLOGE("Filesystem check failed (no filesystem)");
            errno = ENODATA;
            return -1;

        default:
            SLOGE("Filesystem check failed (unknown exit code %d)", rc);
            errno = EIO;
            return -1;
        }
    } while (0);

    return 0;
}



status_t VolumeBase::setInternalPath(const std::string& internalPath) {
    if (mState != State::kChecking) {
        LOG(WARNING) << getId() << " internal path change requires state checking";
        return -EBUSY;
    }

    mInternalPath = internalPath;
    //  12-19 22:07:45.271  3588  3726 D VoldConnector: RCV <- {656 public:8,1 /mnt/media_rw/5243-5977}
    notifyEvent(ResponseCode::VolumeInternalPathChanged, mInternalPath);//  
    return OK;
}














//  system/core/libcutils/fs.c:109
int fs_prepare_dir(const char* path, mode_t mode, uid_t uid, gid_t gid) {
    return fs_prepare_path_impl(path, mode, uid, gid, /*allow_fixup*/ 1, /*prepare_as_dir*/ 1);
}


static int fs_prepare_path_impl(const char* path, mode_t mode, uid_t uid, gid_t gid,
        int allow_fixup, int prepare_as_dir) {
    // Check if path needs to be created
    struct stat sb;
    int create_result = -1;
    if (TEMP_FAILURE_RETRY(lstat(path, &sb)) == -1) {
        if (errno == ENOENT) {
            goto create;
        } else {
            ALOGE("Failed to lstat(%s): %s", path, strerror(errno));
            return -1;
        }
    }

    // Exists, verify status
    int type_ok = prepare_as_dir ? S_ISDIR(sb.st_mode) : S_ISREG(sb.st_mode);
    if (!type_ok) {
        ALOGE("Not a %s: %s", (prepare_as_dir ? "directory" : "regular file"), path);
        return -1;
    }

    int owner_match = ((sb.st_uid == uid) && (sb.st_gid == gid));
    int mode_match = ((sb.st_mode & ALL_PERMS) == mode);
    if (owner_match && mode_match) {
        return 0;
    } else if (allow_fixup) {
        goto fixup;
    } else {
        if (!owner_match) {
            ALOGE("Expected path %s with owner %d:%d but found %d:%d",
                    path, uid, gid, sb.st_uid, sb.st_gid);
            return -1;
        } else {
            ALOGW("Expected path %s with mode %o but found %o",
                    path, mode, (sb.st_mode & ALL_PERMS));
            return 0;
        }
    }

create:
    create_result = prepare_as_dir
        ? TEMP_FAILURE_RETRY(mkdir(path, mode))
        : TEMP_FAILURE_RETRY(open(path, O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_RDONLY, 0644));
    if (create_result == -1) {
        if (errno != EEXIST) {
            ALOGE("Failed to %s(%s): %s",
                    (prepare_as_dir ? "mkdir" : "open"), path, strerror(errno));
            return -1;
        }
    } else if (!prepare_as_dir) {
        // For regular files we need to make sure we close the descriptor
        if (close(create_result) == -1) {
            ALOGW("Failed to close file after create %s: %s", path, strerror(errno));
        }
    }
fixup:
    if (TEMP_FAILURE_RETRY(chmod(path, mode)) == -1) {
        ALOGE("Failed to chmod(%s, %d): %s", path, mode, strerror(errno));
        return -1;
    }
    if (TEMP_FAILURE_RETRY(chown(path, uid, gid)) == -1) {
        ALOGE("Failed to chown(%s, %d, %d): %s", path, uid, gid, strerror(errno));
        return -1;
    }

    return 0;
}





/*

    if (vfat::Mount(mDevPath, mRawPath, false, false, false, AID_MEDIA_RW, AID_MEDIA_RW, 0007, true)) {
        PLOG(ERROR) << getId() << " failed to mount " << mDevPath;
        return -EIO;
    }
*/


status_t Mount(const std::string& source, const std::string& target, bool ro,
        bool remount, bool executable, int ownerUid, int ownerGid, int permMask,
        bool createLost) {
    int rc;
    unsigned long flags;
    char mountData[255];

    const char* c_source = source.c_str();
    const char* c_target = target.c_str();

    flags = MS_NODEV | MS_NOSUID | MS_DIRSYNC | MS_NOATIME;

    flags |= (executable ? 0 : MS_NOEXEC);
    flags |= (ro ? MS_RDONLY : 0);
    flags |= (remount ? MS_REMOUNT : 0);

    snprintf(mountData, sizeof(mountData),
            "utf8,uid=%d,gid=%d,fmask=%o,dmask=%o,shortname=mixed",
            ownerUid, ownerGid, permMask, permMask);

    rc = mount(c_source, c_target, "vfat", flags, mountData);

    if (rc && errno == EROFS) {
        SLOGE("%s appears to be a read only filesystem - retrying mount RO", c_source);
        flags |= MS_RDONLY;
        rc = mount(c_source, c_target, "vfat", flags, mountData);
    }

    if (rc == 0 && createLost) {
        char *lost_path;
        asprintf(&lost_path, "%s/LOST.DIR", c_target);
        if (access(lost_path, F_OK)) {
            /*
             * Create a LOST.DIR in the root so we have somewhere to put
             * lost cluster chains (fsck_msdos doesn't currently do this)
             */
            if (mkdir(lost_path, 0755)) {
                SLOGE("Unable to create LOST.DIR (%s)", strerror(errno));
            }
        }
        free(lost_path);
    }

    return rc;
}



//  system/vold/Utils.cpp:652
dev_t GetDevice(const std::string& path) {
    struct stat sb;
    if (stat(path.c_str(), &sb)) {
        PLOG(WARNING) << "Failed to stat " << path;
        return 0;
    } else {
        return sb.st_dev;
    }
}