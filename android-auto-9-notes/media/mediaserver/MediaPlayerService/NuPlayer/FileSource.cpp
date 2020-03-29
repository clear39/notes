


/* static */
bool FileSource::requiresDrm(int fd, int64_t offset, int64_t length, const char *mime) {
    std::unique_ptr<DrmManagerClient> drmClient(new DrmManagerClient());
    sp<DecryptHandle> decryptHandle = drmClient->openDecryptSession(fd, offset, length, mime);
    bool requiresDrm = false;
    if (decryptHandle != nullptr) {
        //  @   frameworks/av/include/drm/drm_framework_common.h:236:    static const int CONTAINER_BASED = 0x02;
        requiresDrm = decryptHandle->decryptApiType == DecryptApiType::CONTAINER_BASED;
        drmClient->closeDecryptSession(decryptHandle);
    }
    return requiresDrm;
}