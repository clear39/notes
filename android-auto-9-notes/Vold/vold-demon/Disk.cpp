//  @   /work/workcodes/aosp-p9.x-auto-ga/system/vold/model/Disk.cpp
Disk::Disk(const std::string& eventPath, dev_t device,
        const std::string& nickname, int flags) :
        mDevice(device), mSize(-1), mNickname(nickname), mFlags(flags), mCreated(
                false), mJustPartitioned(false) {
    mId = StringPrintf("disk:%u,%u", major(device), minor(device));
    mEventPath = eventPath;
    mSysPath = StringPrintf("/sys/%s", eventPath.c_str());
    mDevPath = StringPrintf("/dev/block/vold/%s", mId.c_str());
    CreateDeviceNode(mDevPath, mDevice);
}

status_t Disk::create() {
    CHECK(!mCreated);
    mCreated = true;

    auto listener = VolumeManager::Instance()->getListener();
    if (listener) listener->onDiskCreated(getId(), mFlags);

    readMetadata();
    readPartitions();
    return OK;
}