//	@frameworks/native/cmds/servicemanager/servicemanager.rc
service servicemanager /system/bin/servicemanager
    class core animation
    user system
    group system readproc
    critical
    onrestart restart healthd
    onrestart restart zygote
    onrestart restart audioserver
    onrestart restart media
    onrestart restart surfaceflinger
    onrestart restart inputflinger
    onrestart restart drm
    onrestart restart cameraserver
    writepid /dev/cpuset/system-background/tasks
    shutdown critical


//	@frameworks/native/cmds/servicemanager/service_manager.c
int main(int argc, char** argv)
{
    struct binder_state *bs;
    union selinux_callback cb;
    char *driver;

    if (argc > 1) {
        driver = argv[1];
    } else {
        driver = "/dev/binder";
    }

    bs = binder_open(driver, 128*1024);
    if (!bs) {
#ifdef VENDORSERVICEMANAGER  //  当为vndservicemanager  VENDORSERVICEMANAGER=1 ; 定义在@frameworks/native/cmds/servicemanager/Android.bp
        ALOGW("failed to open binder driver %s\n", driver);
        while (true) {
            sleep(UINT_MAX);
        }
#else
        ALOGE("failed to open binder driver %s\n", driver);
#endif
        return -1;
    }

    if (binder_become_context_manager(bs)) {
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

    cb.func_audit = audit_callback;//	@frameworks/native/cmds/servicemanager/service_manager.c:352
    selinux_set_callback(SELINUX_CB_AUDIT, cb);
    // selinux_log_callback 打印 以SELinux为tag的logcat日志
    cb.func_log = selinux_log_callback;	//	@external/selinux/libselinux/src/android/android.c:
    selinux_set_callback(SELINUX_CB_LOG, cb);

#ifdef VENDORSERVICEMANAGER			//  当为vndservicemanager  VENDORSERVICEMANAGER=1 ; 定义在@frameworks/native/cmds/servicemanager/Android.bp
    sehandle = selinux_android_vendor_service_context_handle();//	@external/selinux/libselinux/src/android/android.c
#else
    sehandle = selinux_android_service_context_handle();//	@external/selinux/libselinux/src/android/android.c
#endif
    selinux_status_open(true);//	@external/selinux/libselinux/src/sestatus.c:255

    if (sehandle == NULL) {
        ALOGE("SELinux: Failed to acquire sehandle. Aborting.\n");
        abort();
    }


    //	static char *service_manager_context;
    if (getcon(&service_manager_context) != 0) {
        ALOGE("SELinux: Failed to acquire service_manager context. Aborting.\n");
        abort();
    }


    binder_loop(bs, svcmgr_handler);

    return 0;
}


//	@external/selinux/libselinux/src/android/android.c
static const struct selinux_opt seopts_service_split[] = {
    { SELABEL_OPT_PATH, "/system/etc/selinux/plat_service_contexts" },
    { SELABEL_OPT_PATH, "/vendor/etc/selinux/nonplat_service_contexts" }
};


static const struct selinux_opt seopts_service_rootfs[] = {
    { SELABEL_OPT_PATH, "/plat_service_contexts" },
    { SELABEL_OPT_PATH, "/nonplat_service_contexts" }
};


struct selabel_handle* selinux_android_service_context_handle(void)
{
    const struct selinux_opt* seopts_service;

    // Prefer files from /system & /vendor, fall back to files from /
    if (access(seopts_service_split[0].value, R_OK) != -1) {
        seopts_service = seopts_service_split;
    } else {
        seopts_service = seopts_service_rootfs;
    }

#ifdef FULL_TREBLE
    // Treble compliant devices can only serve plat_service_contexts from servicemanager
    return selinux_android_service_open_context_handle(seopts_service, 1);
#else
    return selinux_android_service_open_context_handle(seopts_service, 2);
#endif
}


struct selabel_handle* selinux_android_service_open_context_handle(const struct selinux_opt* seopts_service,unsigned nopts)
{
	//	selabel_handle @external/selinux/libselinux/src/label_internal.h:88
    struct selabel_handle* sehandle;

    //	@external/selinux/libselinux/src/label.c:354
    //	selabel_open主要做俩件事:
    //第一件 加载 策略文件
    //第二件 初始化 selabel_handle,以及后面访问的函数指针
    sehandle = selabel_open(SELABEL_CTX_ANDROID_SERVICE,seopts_service, nopts);

    if (!sehandle) {
        selinux_log(SELINUX_ERROR, "%s: Error getting service context handle (%s)\n",__FUNCTION__, strerror(errno));
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
int selinux_status_open(int fallback)// true
{
	int	fd;
	char	path[PATH_MAX];
	long	pagesize;

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
		union selinux_callback	cb;

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





