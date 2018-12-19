//	@frameworks/av/media/libstagefright/FileSource.cpp
/* static */
bool FileSource::requiresDrm(int fd, int64_t offset, int64_t length, const char *mime) {
    std::unique_ptr<DrmManagerClient> drmClient(new DrmManagerClient());
    sp<DecryptHandle> decryptHandle = drmClient->openDecryptSession(fd, offset, length, mime);
    bool requiresDrm = false;
    if (decryptHandle != nullptr) {
        requiresDrm = decryptHandle->decryptApiType == DecryptApiType::CONTAINER_BASED;
        drmClient->closeDecryptSession(decryptHandle);
    }
    return requiresDrm;
}