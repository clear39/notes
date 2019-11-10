

//  @   frameworks/native/services/displayservice/DisplayEventReceiver.cpp
Return<Status> DisplayEventReceiver::init(const sp<IEventCallback>& callback) {
    std::unique_lock<std::mutex> lock(mMutex);

    if (mAttached != nullptr || callback == nullptr) {
        return Status::BAD_VALUE;
    }

    mAttached = new AttachedEvent(callback);

    return mAttached->valid() ? Status::SUCCESS : Status::UNKNOWN;
}

