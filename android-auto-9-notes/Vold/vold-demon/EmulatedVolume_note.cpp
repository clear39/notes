//  @system/vold/model/EmulatedVolume.cpp
EmulatedVolume::EmulatedVolume(const std::string& rawPath) :
        VolumeBase(Type::kEmulated), mFusePid(0) { //VolumeBase 这里只是设置类型
    setId("emulated");
    mRawPath = rawPath;
    mLabel = "emulated";
}



status_t VolumeBase::create() {
    CHECK(!mCreated);

    mCreated = true;
    status_t res = doCreate();

    auto listener = getListener();
    if (listener) listener->onVolumeCreated(getId(),static_cast<int32_t>(mType), mDiskId, mPartGuid);

    setState(State::kUnmounted);
    return res;
}

status_t VolumeBase::doCreate() {
    return OK;
}

