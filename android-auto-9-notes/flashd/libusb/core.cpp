///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   external/libusb/libusb/core.c:2062:int API_EXPORTED libusb_init(libusb_context **context)
int API_EXPORTED libusb_init(libusb_context **context)
{
	struct libusb_device *dev, *next;
	char *dbg = getenv("LIBUSB_DEBUG");
	struct libusb_context *ctx;
	static int first_init = 1;
	int r = 0;

	usbi_mutex_static_lock(&default_context_lock);

	if (!timestamp_origin.tv_sec) {
		usbi_gettimeofday(&timestamp_origin, NULL);
	}

    /**
     * 由于 context 传递参数为 NULL
     * @    external/libusb/libusb/core.c:73:struct libusb_context *usbi_default_context = NULL;
    */
	if (!context && usbi_default_context) {
		usbi_dbg("reusing default context");
		default_context_refcnt++;
		usbi_mutex_static_unlock(&default_context_lock);
		return 0;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		r = LIBUSB_ERROR_NO_MEM;
		goto err_unlock;
	}

#ifdef ENABLE_DEBUG_LOGGING
	ctx->debug = LIBUSB_LOG_LEVEL_DEBUG;
#endif

	if (dbg) {
		ctx->debug = atoi(dbg);
		if (ctx->debug)
			ctx->debug_fixed = 1;
	}

	/* default context should be initialized before calling usbi_dbg */
	if (!usbi_default_context) {
		usbi_default_context = ctx;
		default_context_refcnt++;
		usbi_dbg("created default context");
	}

	usbi_dbg("libusb v%u.%u.%u.%u%s", libusb_version_internal.major, libusb_version_internal.minor,
		libusb_version_internal.micro, libusb_version_internal.nano, libusb_version_internal.rc);

	usbi_mutex_init(&ctx->usb_devs_lock);
	usbi_mutex_init(&ctx->open_devs_lock);
	usbi_mutex_init(&ctx->hotplug_cbs_lock);
	list_init(&ctx->usb_devs);
	list_init(&ctx->open_devs);
	list_init(&ctx->hotplug_cbs);

	usbi_mutex_static_lock(&active_contexts_lock);
	if (first_init) {
		first_init = 0;
		list_init (&active_contexts_list);
	}
	list_add (&ctx->list, &active_contexts_list);
	usbi_mutex_static_unlock(&active_contexts_lock);
    /**
     * 对应
     * @    /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/os/linux_usbfs.c
     * op_init 函数
    */
	if (usbi_backend->init) {
		r = usbi_backend->init(ctx);
		if (r)
			goto err_free_ctx;
	}

	r = usbi_io_init(ctx);
	if (r < 0)
		goto err_backend_exit;

	usbi_mutex_static_unlock(&default_context_lock);

	if (context)
		*context = ctx;

	return 0;

err_backend_exit:
	if (usbi_backend->exit)
		usbi_backend->exit();
err_free_ctx:
	if (ctx == usbi_default_context) {
		usbi_default_context = NULL;
		default_context_refcnt--;
	}

	usbi_mutex_static_lock(&active_contexts_lock);
	list_del (&ctx->list);
	usbi_mutex_static_unlock(&active_contexts_lock);

	usbi_mutex_lock(&ctx->usb_devs_lock);
	list_for_each_entry_safe(dev, next, &ctx->usb_devs, list, struct libusb_device) {
		list_del(&dev->list);
		libusb_unref_device(dev);
	}
	usbi_mutex_unlock(&ctx->usb_devs_lock);

	usbi_mutex_destroy(&ctx->open_devs_lock);
	usbi_mutex_destroy(&ctx->usb_devs_lock);
	usbi_mutex_destroy(&ctx->hotplug_cbs_lock);

	free(ctx);
err_unlock:
	usbi_mutex_static_unlock(&default_context_lock);
	return r;
}


static int op_init(struct libusb_context *ctx)
{
	struct stat statbuf;
	int r;
    /**
     * @    /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/os/linux_usbfs.c
     * find_usbfs_path 设置默认路径 “/dev/bus/usb”，并且确认路径是否存在，返回路径 “/dev/bus/usb”
    */
	usbfs_path = find_usbfs_path();
	if (!usbfs_path) {
		usbi_err(ctx, "could not find usbfs");
		return LIBUSB_ERROR_OTHER;
	}

	if (monotonic_clkid == -1)
		monotonic_clkid = find_monotonic_clock();

	if (supports_flag_bulk_continuation == -1) {
		/* bulk continuation URB flag available from Linux 2.6.32 */
		supports_flag_bulk_continuation = kernel_version_ge(2,6,32);
		if (supports_flag_bulk_continuation == -1) {
			usbi_err(ctx, "error checking for bulk continuation support");
			return LIBUSB_ERROR_OTHER;
		}
	}

	if (supports_flag_bulk_continuation)
		usbi_dbg("bulk continuation flag supported");

	if (-1 == supports_flag_zero_packet) {
		/* zero length packet URB flag fixed since Linux 2.6.31 */
		supports_flag_zero_packet = kernel_version_ge(2,6,31);
		if (-1 == supports_flag_zero_packet) {
			usbi_err(ctx, "error checking for zero length packet support");
			return LIBUSB_ERROR_OTHER;
		}
	}

	if (supports_flag_zero_packet)
		usbi_dbg("zero length packet flag supported");

	if (-1 == sysfs_has_descriptors) {
		/* sysfs descriptors has all descriptors since Linux 2.6.26 */
		sysfs_has_descriptors = kernel_version_ge(2,6,26);
		if (-1 == sysfs_has_descriptors) {
			usbi_err(ctx, "error checking for sysfs descriptors");
			return LIBUSB_ERROR_OTHER;
		}
	}

	if (-1 == sysfs_can_relate_devices) {
		/** sysfs has busnum since Linux 2.6.22
         * external/libusb/libusb/os/linux_usbfs.c 
        */
		sysfs_can_relate_devices = kernel_version_ge(2,6,22);
		if (-1 == sysfs_can_relate_devices) {
			usbi_err(ctx, "error checking for sysfs busnum");
			return LIBUSB_ERROR_OTHER;
		}
	}

	if (sysfs_can_relate_devices || sysfs_has_descriptors) {
		r = stat(SYSFS_DEVICE_PATH, &statbuf);
		if (r != 0 || !S_ISDIR(statbuf.st_mode)) {
			usbi_warn(ctx, "sysfs not mounted");
			sysfs_can_relate_devices = 0;
			sysfs_has_descriptors = 0;
		}
	}

	if (sysfs_can_relate_devices)
		usbi_dbg("sysfs can relate devices");

	if (sysfs_has_descriptors)
		usbi_dbg("sysfs has complete descriptors");

	usbi_mutex_static_lock(&linux_hotplug_startstop_lock);
	r = LIBUSB_SUCCESS;
	if (init_count == 0) {
		/* start up hotplug event handler */
		r = linux_start_event_monitor();
	}
	if (r == LIBUSB_SUCCESS) {
		r = linux_scan_devices(ctx);
		if (r == LIBUSB_SUCCESS)
			init_count++;
		else if (init_count == 0)
			linux_stop_event_monitor();
	} else
		usbi_err(ctx, "error starting hotplug event monitor");
	usbi_mutex_static_unlock(&linux_hotplug_startstop_lock);

	return r;
}

static int linux_start_event_monitor(void)
{
#if defined(USE_UDEV)
	return linux_udev_start_event_monitor();
#else
	return linux_netlink_start_event_monitor();
#endif
}


int linux_netlink_start_event_monitor(void)
{
	struct sockaddr_nl sa_nl = { .nl_family = AF_NETLINK, .nl_groups = NL_GROUP_KERNEL };
	int socktype = SOCK_RAW;
	int opt = 1;
	int ret;

#if defined(SOCK_CLOEXEC)
	socktype |= SOCK_CLOEXEC;
#endif
#if defined(SOCK_NONBLOCK)
	socktype |= SOCK_NONBLOCK;
#endif

	linux_netlink_socket = socket(PF_NETLINK, socktype, NETLINK_KOBJECT_UEVENT);
	if (linux_netlink_socket == -1 && errno == EINVAL) {
		usbi_dbg("failed to create netlink socket of type %d, attempting SOCK_RAW", socktype);
		linux_netlink_socket = socket(PF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	}

	if (linux_netlink_socket == -1) {
		usbi_err(NULL, "failed to create netlink socket (%d)", errno);
		goto err;
	}

	ret = set_fd_cloexec_nb(linux_netlink_socket);
	if (ret == -1)
		goto err_close_socket;

	ret = bind(linux_netlink_socket, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
	if (ret == -1) {
		usbi_err(NULL, "failed to bind netlink socket (%d)", errno);
		goto err_close_socket;
	}

	ret = setsockopt(linux_netlink_socket, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt));
	if (ret == -1) {
		usbi_err(NULL, "failed to set netlink socket SO_PASSCRED option (%d)", errno);
		goto err_close_socket;
	}

	ret = usbi_pipe(netlink_control_pipe);
	if (ret) {
		usbi_err(NULL, "failed to create netlink control pipe");
		goto err_close_socket;
	}

	ret = pthread_create(&libusb_linux_event_thread, NULL, linux_netlink_event_thread_main, NULL);
	if (ret != 0) {
		usbi_err(NULL, "failed to create netlink event thread (%d)", ret);
		goto err_close_pipe;
	}

	return LIBUSB_SUCCESS;

err_close_pipe:
	close(netlink_control_pipe[0]);
	close(netlink_control_pipe[1]);
	netlink_control_pipe[0] = -1;
	netlink_control_pipe[1] = -1;
err_close_socket:
	close(linux_netlink_socket);
	linux_netlink_socket = -1;
err:
	return LIBUSB_ERROR_OTHER;
}


static void *linux_netlink_event_thread_main(void *arg)
{
	char dummy;
	ssize_t r;
	struct pollfd fds[] = {
		{ .fd = netlink_control_pipe[0],
		  .events = POLLIN },
		{ .fd = linux_netlink_socket,
		  .events = POLLIN },
	};

	UNUSED(arg);

	usbi_dbg("netlink event thread entering");

	while (poll(fds, 2, -1) >= 0) {
		if (fds[0].revents & POLLIN) {
			/* activity on control pipe, read the byte and exit */
			r = usbi_read(netlink_control_pipe[0], &dummy, sizeof(dummy));
			if (r <= 0)
				usbi_warn(NULL, "netlink control pipe read failed");
			break;
		}
		if (fds[1].revents & POLLIN) {
			usbi_mutex_static_lock(&linux_hotplug_lock);
			linux_netlink_read_message();
			usbi_mutex_static_unlock(&linux_hotplug_lock);
		}
	}

	usbi_dbg("netlink event thread exiting");

	return NULL;
}


static int linux_scan_devices(struct libusb_context *ctx)
{
	int ret;

	usbi_mutex_static_lock(&linux_hotplug_lock);

#if defined(USE_UDEV)
	ret = linux_udev_scan_devices(ctx);
#else
	ret = linux_default_scan_devices(ctx);
#endif

	usbi_mutex_static_unlock(&linux_hotplug_lock);

	return ret;
}


static int linux_default_scan_devices (struct libusb_context *ctx)
{
	/* we can retrieve device list and descriptors from sysfs or usbfs.
	 * sysfs is preferable, because if we use usbfs we end up resuming
	 * any autosuspended USB devices. however, sysfs is not available
	 * everywhere, so we need a usbfs fallback too.
	 *
	 * as described in the "sysfs vs usbfs" comment at the top of this
	 * file, sometimes we have sysfs but not enough information to
	 * relate sysfs devices to usbfs nodes.  op_init() determines the
	 * adequacy of sysfs and sets sysfs_can_relate_devices.
	 */
	if (sysfs_can_relate_devices != 0)
		return sysfs_get_device_list(ctx);
	else
		return usbfs_get_device_list(ctx);
}









///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   external/libusb/libusb/core.c
ssize_t API_EXPORTED libusb_get_device_list(libusb_context *ctx,libusb_device ***list)
{
    /**
     * 
    */
	struct discovered_devs *discdevs = discovered_devs_alloc();
	struct libusb_device **ret;
	int r = 0;
	ssize_t i, len;
    /**
     * @    /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/libusbi.h
        #define USBI_GET_CONTEXT(ctx)				\
        do {						\
            if (!(ctx))				\
                (ctx) = usbi_default_context;	\
        } while(0)

        这里给 ctx 复制为 usbi_default_context
     * 
    */
	USBI_GET_CONTEXT(ctx);
	usbi_dbg("");

	if (!discdevs)
		return LIBUSB_ERROR_NO_MEM;

    /***
     * 这里 libusb_has_capability 返回 true
    */
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		/* backend provides hotplug support */
		struct libusb_device *dev;

        /**
         * 由于 hotplug_poll 不会空，
         * 则这里执行 hotplug_poll 
         */
		if (usbi_backend->hotplug_poll)
			usbi_backend->hotplug_poll();

		usbi_mutex_lock(&ctx->usb_devs_lock);
        /***
        
        */
		list_for_each_entry(dev, &ctx->usb_devs, list, struct libusb_device) {
            // @ /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/core.c
			discdevs = discovered_devs_append(discdevs, dev);

			if (!discdevs) {
				r = LIBUSB_ERROR_NO_MEM;
				break;
			}
		}
		usbi_mutex_unlock(&ctx->usb_devs_lock);
	} else {
		/* backend does not provide hotplug support */
		r = usbi_backend->get_device_list(ctx, &discdevs);
	}

	if (r < 0) {
		len = r;
		goto out;
	}

	/* convert discovered_devs into a list */
	len = discdevs->len;
	ret = calloc(len + 1, sizeof(struct libusb_device *));
	if (!ret) {
		len = LIBUSB_ERROR_NO_MEM;
		goto out;
	}

	ret[len] = NULL;
	for (i = 0; i < len; i++) {
		struct libusb_device *dev = discdevs->devices[i];
		ret[i] = libusb_ref_device(dev);
	}
	*list = ret;

out:
	if (discdevs)
		discovered_devs_free(discdevs);
	return len;
}





//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/libusbi.h
struct discovered_devs {
	size_t len;
	size_t capacity;
	struct libusb_device *devices
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
	[] /* valid C99 code */
#else
	[0] /* non-standard, but usually working code */
#endif
	;
};


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/core.c
static struct discovered_devs *discovered_devs_alloc(void)
{
    /**
     *  @   #define DISCOVERED_DEVICES_SIZE_STEP 8 
    */
	struct discovered_devs *ret = malloc(sizeof(*ret) + (sizeof(void *) * DISCOVERED_DEVICES_SIZE_STEP));

	if (ret) {
		ret->len = 0;
		ret->capacity = DISCOVERED_DEVICES_SIZE_STEP;
	}
	return ret;
}




 
/** \ingroup libusb_misc
 * Check at runtime if the loaded library has a given capability.
 * This call should be performed after \ref libusb_init(), to ensure the
 * backend has updated its capability set.
 *
 * \param capability the \ref libusb_capability to check for
 * \returns nonzero if the running library has the capability, 0 otherwise
 * 
 * 
 * @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/core.c
 * 
 * external/libusb/libusb/core.c:48:const struct usbi_os_backend * const usbi_backend = &linux_usbfs_backend;
 * 
 */
int API_EXPORTED libusb_has_capability(uint32_t capability)
{
	switch (capability) {
	case LIBUSB_CAP_HAS_CAPABILITY:
		return 1;
	case LIBUSB_CAP_HAS_HOTPLUG:
		return !(usbi_backend->get_device_list);
	case LIBUSB_CAP_HAS_HID_ACCESS:
		return (usbi_backend->caps & USBI_CAP_HAS_HID_ACCESS);
	case LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER:
		return (usbi_backend->caps & USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER);
	}
	return 0;
}

/**
 * @    /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/os/linux_usbfs.c
*/
const struct usbi_os_backend linux_usbfs_backend = {
	.name = "Linux usbfs",
	.caps = USBI_CAP_HAS_HID_ACCESS|USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER,
	.init = op_init,
	.exit = op_exit,
	.get_device_list = NULL,
	.hotplug_poll = op_hotplug_poll,
	.get_device_descriptor = op_get_device_descriptor,
	.get_active_config_descriptor = op_get_active_config_descriptor,
	.get_config_descriptor = op_get_config_descriptor,
	.get_config_descriptor_by_value = op_get_config_descriptor_by_value,

	.open = op_open,
	.close = op_close,
	.get_configuration = op_get_configuration,
	.set_configuration = op_set_configuration,
	.claim_interface = op_claim_interface,
	.release_interface = op_release_interface,

	.set_interface_altsetting = op_set_interface,
	.clear_halt = op_clear_halt,
	.reset_device = op_reset_device,

	.alloc_streams = op_alloc_streams,
	.free_streams = op_free_streams,

	.dev_mem_alloc = op_dev_mem_alloc,
	.dev_mem_free = op_dev_mem_free,

	.kernel_driver_active = op_kernel_driver_active,
	.detach_kernel_driver = op_detach_kernel_driver,
	.attach_kernel_driver = op_attach_kernel_driver,

	.destroy_device = op_destroy_device,

	.submit_transfer = op_submit_transfer,
	.cancel_transfer = op_cancel_transfer,
	.clear_transfer_priv = op_clear_transfer_priv,

	.handle_events = op_handle_events,

	.clock_gettime = op_clock_gettime,

#ifdef USBI_TIMERFD_AVAILABLE
	.get_timerfd_clockid = op_get_timerfd_clockid,
#endif

	.device_priv_size = sizeof(struct linux_device_priv),
	.device_handle_priv_size = sizeof(struct linux_device_handle_priv),
	.transfer_priv_size = sizeof(struct linux_transfer_priv),
};



static void op_hotplug_poll(void)
{
/**
 * USE_UDEV 没有定义
*/
#if defined(USE_UDEV)
	linux_udev_hotplug_poll();
#else
    // 执行这里 
    /**
     * @    external/libusb/libusb/os/linux_netlink.c
    */
	linux_netlink_hotplug_poll();
#endif
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/external/libusb/libusb/os/linux_netlink.c
void linux_netlink_hotplug_poll(void)
{
        int r;

        usbi_mutex_static_lock(&linux_hotplug_lock);
        do {
                r = linux_netlink_read_message();
        } while (r == 0);
        usbi_mutex_static_unlock(&linux_hotplug_lock);
}

static int linux_netlink_read_message(void)
{
	char cred_buffer[CMSG_SPACE(sizeof(struct ucred))];
	char msg_buffer[2048];
	const char *sys_name = NULL;
	uint8_t busnum, devaddr;
	int detached, r;
	ssize_t len;
	struct cmsghdr *cmsg;
	struct ucred *cred;
	struct sockaddr_nl sa_nl;
	struct iovec iov = { .iov_base = msg_buffer, .iov_len = sizeof(msg_buffer) };
	struct msghdr msg = {
		.msg_iov = &iov, .msg_iovlen = 1,
		.msg_control = cred_buffer, .msg_controllen = sizeof(cred_buffer),
		.msg_name = &sa_nl, .msg_namelen = sizeof(sa_nl)
	};

	/* read netlink message */
	len = recvmsg(linux_netlink_socket, &msg, 0);
	if (len == -1) {
		if (errno != EAGAIN && errno != EINTR)
			usbi_err(NULL, "error receiving message from netlink (%d)", errno);
		return -1;
	}

	if (len < 32 || (msg.msg_flags & MSG_TRUNC)) {
		usbi_err(NULL, "invalid netlink message length");
		return -1;
	}

	if (sa_nl.nl_groups != NL_GROUP_KERNEL || sa_nl.nl_pid != 0) {
		usbi_dbg("ignoring netlink message from unknown group/PID (%u/%u)",
			 (unsigned int)sa_nl.nl_groups, (unsigned int)sa_nl.nl_pid);
		return -1;
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg || cmsg->cmsg_type != SCM_CREDENTIALS) {
		usbi_dbg("ignoring netlink message with no sender credentials");
		return -1;
	}

	cred = (struct ucred *)CMSG_DATA(cmsg);
	if (cred->uid != 0) {
		usbi_dbg("ignoring netlink message with non-zero sender UID %u", (unsigned int)cred->uid);
		return -1;
	}

	r = linux_netlink_parse(msg_buffer, (size_t)len, &detached, &sys_name, &busnum, &devaddr);
	if (r)
		return r;

	usbi_dbg("netlink hotplug found device busnum: %hhu, devaddr: %hhu, sys_name: %s, removed: %s",
		 busnum, devaddr, sys_name, detached ? "yes" : "no");

	/* signal device is available (or not) to all contexts */
	if (detached)
		linux_device_disconnected(busnum, devaddr);
	else
		linux_hotplug_enumerate(busnum, devaddr, sys_name);

	return 0;
}





/** \ingroup libusb_dev
 * Increment the reference count of a device.
 * \param dev the device to reference
 * \returns the same device
 */
DEFAULT_VISIBILITY
libusb_device * LIBUSB_CALL libusb_ref_device(libusb_device *dev)
{
	usbi_mutex_lock(&dev->lock);
	dev->refcnt++;
	usbi_mutex_unlock(&dev->lock);
	return dev;
}