//这个是selinux控制
//	@system/hwservicemanager/AccessControl.cpp

AccessControl::AccessControl() {
    mSeHandle = selinux_android_hw_service_context_handle();//  @external/selinux/libselinux/src/android/android.c
    LOG_ALWAYS_FATAL_IF(mSeHandle == NULL, "Failed to acquire SELinux handle.");

    if (getcon(&mSeContext) != 0) {
        LOG_ALWAYS_FATAL("Failed to acquire hwservicemanager context.");
    }

    selinux_status_open(true);//    @external/selinux/libselinux/src/sestatus.c

    mSeCallbacks.func_audit = AccessControl::auditCallback;
    selinux_set_callback(SELINUX_CB_AUDIT, mSeCallbacks);

    mSeCallbacks.func_log = selinux_log_callback; /* defined in libselinux */   //  @external/selinux/libselinux/src/android/android.c
    selinux_set_callback(SELINUX_CB_LOG, mSeCallbacks);
}


static const struct selinux_opt seopts_hwservice_split[] = {
    { SELABEL_OPT_PATH, "/system/etc/selinux/plat_hwservice_contexts" },
    { SELABEL_OPT_PATH, "/vendor/etc/selinux/nonplat_hwservice_contexts" }
};

static const struct selinux_opt seopts_hwservice_rootfs[] = {
    { SELABEL_OPT_PATH, "/plat_hwservice_contexts" },
    { SELABEL_OPT_PATH, "/nonplat_hwservice_contexts" }
};

//  @external/selinux/libselinux/src/android/android.c
struct selabel_handle* selinux_android_hw_service_context_handle(void)
{
    const struct selinux_opt* seopts_service;
    if (access(seopts_hwservice_split[0].value, R_OK) != -1) {
        seopts_service = seopts_hwservice_split;
    } else {
        seopts_service = seopts_hwservice_rootfs;
    }

    return selinux_android_service_open_context_handle(seopts_service, 2);
}


struct selabel_handle* selinux_android_service_open_context_handle(const struct selinux_opt* seopts_service,unsigned nopts)
{
    struct selabel_handle* sehandle;

    sehandle = selabel_open(SELABEL_CTX_ANDROID_SERVICE,seopts_service, nopts);

    if (!sehandle) {
        selinux_log(SELINUX_ERROR, "%s: Error getting service context handle (%s)\n", __FUNCTION__, strerror(errno));
        return NULL;
    }
    selinux_log(SELINUX_INFO, "SELinux: Loaded service_contexts from:\n");
    for (unsigned i = 0; i < nopts; i++) {
        selinux_log(SELINUX_INFO, "    %s\n", seopts_service[i].value);
    }
    return sehandle;
}


/*
 * selinux_status_open
 *
 * It tries to open and mmap kernel status page (/selinux/status).
 * Since Linux 2.6.37 or later supports this feature, we may run
 * fallback routine using a netlink socket on older kernels, if
 * the supplied `fallback' is not zero.
 * It returns 0 on success, or -1 on error.
 */
int selinux_status_open(int fallback)
{
    int fd;
    char    path[PATH_MAX];
    long    pagesize;

    if (!selinux_mnt) {
        errno = ENOENT;
        return -1;
    }

    pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize < 0)
        return -1;

    snprintf(path, sizeof(path), "%s/status", selinux_mnt);
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        goto error;

    selinux_status = mmap(NULL, pagesize, PROT_READ, MAP_SHARED, fd, 0);
    if (selinux_status == MAP_FAILED) {
        close(fd);
        goto error;
    }
    selinux_status_fd = fd;
    last_seqno = (uint32_t)(-1);

    return 0;

error:
    /*
     * If caller wants fallback routine, we try to provide
     * an equivalent functionality using existing netlink
     * socket, although it needs system call invocation to
     * receive event notification.
     */
    if (fallback && avc_netlink_open(0) == 0) {
        union selinux_callback  cb;

        /* register my callbacks */
        cb.func_setenforce = fallback_cb_setenforce;
        selinux_set_callback(SELINUX_CB_SETENFORCE, cb);
        cb.func_policyload = fallback_cb_policyload;
        selinux_set_callback(SELINUX_CB_POLICYLOAD, cb);

        /* mark as fallback mode */
        selinux_status = MAP_FAILED;
        selinux_status_fd = avc_netlink_acquire_fd();
        last_seqno = (uint32_t)(-1);

        fallback_sequence = 0;
        fallback_enforcing = security_getenforce();
        fallback_policyload = 0;

        return 1;
    }
    selinux_status = NULL;

    return -1;
}


int AccessControl::auditCallback(void *data, security_class_t /*cls*/, char *buf, size_t len) {
    struct audit_data *ad = (struct audit_data *)data;

    if (!ad || !ad->interfaceName) {
        ALOGE("No valid hwservicemanager audit data");
        return 0;
    }

    snprintf(buf, len, "interface=%s pid=%d", ad->interfaceName, ad->pid);
    return 0;
}

int selinux_log_callback(int type, const char *fmt, ...)
{
    va_list ap;
    int priority;
    char *strp;

    switch(type) {
    case SELINUX_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
    case SELINUX_INFO:
        priority = ANDROID_LOG_INFO;
        break;
    default:
        priority = ANDROID_LOG_ERROR;
        break;
    }

    va_start(ap, fmt);
    if (vasprintf(&strp, fmt, ap) != -1) {
        LOG_PRI(priority, "SELinux", "%s", strp);
        LOG_EVENT_STRING(AUDITD_LOG_TAG, strp);
        free(strp);
    }
    va_end(ap);
    return 0;
}






bool AccessControl::canGet(const std::string& fqName, pid_t pid) {
    FQName fqIface(fqName);

    if (!fqIface.isValid()) {
        return false;
    }
    const std::string checkName = fqIface.package() + "::" + fqIface.name();

    return checkPermission(getContext(pid), pid, kPermissionGet, checkName.c_str());
}