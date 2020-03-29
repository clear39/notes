


DrmManagerClient::DrmManagerClient(): mUniqueId(0), mDrmManagerClientImpl(NULL) {
    mDrmManagerClientImpl = DrmManagerClientImpl::create(&mUniqueId, true);
    mDrmManagerClientImpl->addClient(mUniqueId);
}