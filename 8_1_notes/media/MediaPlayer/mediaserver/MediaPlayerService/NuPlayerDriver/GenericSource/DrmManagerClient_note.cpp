//	@frameworks/av/drm/libdrmframework/DrmManagerClient.cpp
DrmManagerClient::DrmManagerClient():mUniqueId(0), mDrmManagerClientImpl(NULL) {	//	@sp<DrmManagerClientImpl> mDrmManagerClientImpl;
    mDrmManagerClientImpl = DrmManagerClientImpl::create(&mUniqueId, true);
    mDrmManagerClientImpl->addClient(mUniqueId);
}


sp<DecryptHandle> DrmManagerClient::openDecryptSession(
        int fd, off64_t offset, off64_t length, const char* mime) {

    return mDrmManagerClientImpl->openDecryptSession(
                    mUniqueId, fd, offset, length, mime);
}