Disk::Disk(const std::string& eventPath, dev_t device,
        const std::string& nickname, int flags) :
        mDevice(device), mSize(-1), mNickname(nickname), mFlags(flags), mCreated(false), mJustPartitioned(false) {
    mId = StringPrintf("disk:%u,%u", major(device), minor(device));
    mEventPath = eventPath;
    mSysPath = StringPrintf("/sys/%s", eventPath.c_str());
    mDevPath = StringPrintf("/dev/block/vold/%s", mId.c_str());
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
    if (mknod(cpath, mode, dev) < 0) {
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
    notifyEvent(ResponseCode::DiskCreated, StringPrintf("%d", mFlags));
    readMetadata();
    readPartitions();
    return OK;
}




status_t Disk::readMetadata() {
    mSize = -1;
    mLabel.clear();

    int fd = open(mDevPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd != -1) {
        if (ioctl(fd, BLKGETSIZE64, &mSize)) {
            mSize = -1;
        }
        close(fd);
    }

    unsigned int majorId = major(mDevice);
    switch (majorId) {
    case kMajorBlockLoop: {
        mLabel = "Virtual";
        break;
    }
    case kMajorBlockScsiA: case kMajorBlockScsiB: case kMajorBlockScsiC: case kMajorBlockScsiD:
    case kMajorBlockScsiE: case kMajorBlockScsiF: case kMajorBlockScsiG: case kMajorBlockScsiH:
    case kMajorBlockScsiI: case kMajorBlockScsiJ: case kMajorBlockScsiK: case kMajorBlockScsiL:
    case kMajorBlockScsiM: case kMajorBlockScsiN: case kMajorBlockScsiO: case kMajorBlockScsiP: {
        std::string path(mSysPath + "/device/vendor");
        std::string tmp;
        if (!ReadFileToString(path, &tmp)) {
            PLOG(WARNING) << "Failed to read vendor from " << path;
            return -errno;
        }
        mLabel = tmp;
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

    notifyEvent(ResponseCode::DiskSizeChanged, StringPrintf("%" PRIu64, mSize));
    notifyEvent(ResponseCode::DiskLabelChanged, mLabel);
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
    cmd.push_back(kSgdiskPath);
    cmd.push_back("--android-dump");
    cmd.push_back(mDevPath);

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
                LOG(WARNING) << mId << " is ignoring partition " << i
                        << " beyond max supported devices";
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

    notifyEvent(ResponseCode::DiskScanned);
    mJustPartitioned = false;
    return OK;
}



status_t ForkExecvp(const std::vector<std::string>& args,std::vector<std::string>& output) {
    return ForkExecvp(args, output, nullptr);
}

status_t ForkExecvp(const std::vector<std::string>& args,
        std::vector<std::string>& output, security_context_t context) {
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