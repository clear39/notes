


void DrmManagerService::instantiate() {
    ALOGV("instantiate");
    defaultServiceManager()->addService(String16("drm.drmManager"), new DrmManagerService());

    if (0 >= trustedUids.size()) {
        // TODO
        // Following implementation is just for reference.
        // Each OEM manufacturer should implement/replace with their own solutions.

        // Add trusted uids here
        trustedUids.push(AID_MEDIA);
    }

    selinux_enabled = is_selinux_enabled();
    if (selinux_enabled > 0 && getcon(&drmserver_context) != 0) {
        ALOGE("SELinux: DrmManagerService failed to get context for DrmManagerService. Aborting.\n");
        abort();
    }

    union selinux_callback cb;
    cb.func_log = selinux_log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);
}

DrmManagerService::DrmManagerService() : mDrmManager(NULL) {
    ALOGV("created");
    mDrmManager = new DrmManager();
    mDrmManager->loadPlugIns();
}




DecryptHandle* DrmManagerService::openDecryptSession(int uniqueId, int fd, off64_t offset, off64_t length, const char* mime) {
    ALOGV("Entering DrmManagerService::openDecryptSession");
    if (isProtectedCallAllowed(OPEN_DECRYPT_SESSION)) {
        return mDrmManager->openDecryptSession(uniqueId, fd, offset, length, mime);
    }

    return NULL;
}

