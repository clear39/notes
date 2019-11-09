//  @   system/core/adb/daemon/usb.cpp

void usb_init() {
    dummy_fd = adb_open("/dev/null", O_WRONLY);
    CHECK_NE(dummy_fd, -1);
    usb_ffs_init();
}


static void usb_ffs_init() {
    D("[ usb_init - using FunctionFS ]");

    usb_handle* h = new usb_handle();

    /**
     * sys.usb.ffs.aio_compat 为空
     * 
     * 这里为false
    */
    if (android::base::GetBoolProperty("sys.usb.ffs.aio_compat", false)) {
        // Devices on older kernels (< 3.18) will not have aio support for ffs
        // unless backported. Fall back on the non-aio functions instead.
        h->write = usb_ffs_write;
        h->read = usb_ffs_read;
    } else {
        h->write = usb_ffs_aio_write;
        h->read = usb_ffs_aio_read;
        aio_block_init(&h->read_aiob);
        aio_block_init(&h->write_aiob);
    }
    h->kick = usb_ffs_kick;
    h->close = usb_ffs_close;

    D("[ usb_init - starting thread ]");
    std::thread(usb_ffs_open_thread, h).detach();
}

static void aio_block_init(aio_block* aiob) {
    /**
     * system/core/adb/adb.h:34:constexpr size_t MAX_PAYLOAD = 1024 * 1024;
     * system/core/adb/daemon/usb.cpp:53:#define USB_FFS_BULK_SIZE 16384
     * system/core/adb/daemon/usb.cpp:56:#define USB_FFS_NUM_BUFS ((MAX_PAYLOAD / USB_FFS_BULK_SIZE) + 1)  // 65
    */
    aiob->iocb.resize(USB_FFS_NUM_BUFS);//std::vector<struct iocb> iocb;
    aiob->iocbs.resize(USB_FFS_NUM_BUFS);// std::vector<struct iocb*> iocbs;
    aiob->events.resize(USB_FFS_NUM_BUFS);// std::vector<struct io_event> events;
    aiob->num_submitted = 0;
    for (unsigned i = 0; i < USB_FFS_NUM_BUFS; i++) {
        aiob->iocbs[i] = &aiob->iocb[i];
    }
}


static void usb_ffs_open_thread(void* x) {
    struct usb_handle* usb = (struct usb_handle*)x;

    /**
     * 设置线程名
    */
    adb_thread_setname("usb ffs open");

    while (true) {
        // wait until the USB device needs opening
        std::unique_lock<std::mutex> lock(usb->lock);
        /**
         * usb->open_new_connection 默认值为 true
        */
        while (!usb->open_new_connection) {
            usb->notify.wait(lock);
        }
        usb->open_new_connection = false;
        lock.unlock();

        while (true) {
            if (init_functionfs(usb)) {
                LOG(INFO) << "functionfs successfully initialized";
                break;
            }
            std::this_thread::sleep_for(1s);
        }

        LOG(INFO) << "registering usb transport";
        register_usb_transport(usb, 0, 0, 1);
    }

    // never gets here
    abort();
}


bool init_functionfs(struct usb_handle* h) {
    LOG(INFO) << "initializing functionfs";

    ssize_t ret;
    struct desc_v1 v1_descriptor;
    struct desc_v2 v2_descriptor;

    v2_descriptor.header.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2);
    v2_descriptor.header.length = cpu_to_le32(sizeof(v2_descriptor));
    v2_descriptor.header.flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC |
                                 FUNCTIONFS_HAS_SS_DESC | FUNCTIONFS_HAS_MS_OS_DESC;
    v2_descriptor.fs_count = 3;
    v2_descriptor.hs_count = 3;
    v2_descriptor.ss_count = 5;
    v2_descriptor.os_count = 1;
    v2_descriptor.fs_descs = fs_descriptors;
    v2_descriptor.hs_descs = hs_descriptors;
    v2_descriptor.ss_descs = ss_descriptors;
    v2_descriptor.os_header = os_desc_header;
    v2_descriptor.os_desc = os_desc_compat;

    if (h->control < 0) { // might have already done this before

        /**
         * USB_FFS_ADB_EP0 = "/dev/usb-ffs/adb/ep0"
        */
        LOG(INFO) << "opening control endpoint " << USB_FFS_ADB_EP0;
        h->control = adb_open(USB_FFS_ADB_EP0, O_RDWR);
        if (h->control < 0) {
            PLOG(ERROR) << "cannot open control endpoint " << USB_FFS_ADB_EP0;
            goto err;
        }

        ret = adb_write(h->control, &v2_descriptor, sizeof(v2_descriptor));
        if (ret < 0) {
            v1_descriptor.header.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC);
            v1_descriptor.header.length = cpu_to_le32(sizeof(v1_descriptor));
            v1_descriptor.header.fs_count = 3;
            v1_descriptor.header.hs_count = 3;
            v1_descriptor.fs_descs = fs_descriptors;
            v1_descriptor.hs_descs = hs_descriptors;
            D("[ %s: Switching to V1_descriptor format errno=%d ]", USB_FFS_ADB_EP0, errno);
            ret = adb_write(h->control, &v1_descriptor, sizeof(v1_descriptor));
            if (ret < 0) {
                D("[ %s: write descriptors failed: errno=%d ]", USB_FFS_ADB_EP0, errno);
                goto err;
            }
        }

        ret = adb_write(h->control, &strings, sizeof(strings));
        if (ret < 0) {
            D("[ %s: writing strings failed: errno=%d]", USB_FFS_ADB_EP0, errno);
            goto err;
        }
        //Signal only when writing the descriptors to ffs
        android::base::SetProperty("sys.usb.ffs.ready", "1");
    }
    /**
     * USB_FFS_ADB_OUT = "/dev/usb-ffs/adb/ep1"
    */
    h->bulk_out = adb_open(USB_FFS_ADB_OUT, O_RDWR);
    if (h->bulk_out < 0) {
        PLOG(ERROR) << "cannot open bulk-out endpoint " << USB_FFS_ADB_OUT;
        goto err;
    }
     /**
         * USB_FFS_ADB_IN = "/dev/usb-ffs/adb/ep2"
        */
    h->bulk_in = adb_open(USB_FFS_ADB_IN, O_RDWR);
    if (h->bulk_in < 0) {
        PLOG(ERROR) << "cannot open bulk-in endpoint " << USB_FFS_ADB_IN;
        goto err;
    }

    /**
     * USB_FFS_NUM_BUFS = 65
     * @    system/core/libasyncio/AsyncIO.cpp
    */
    if (io_setup(USB_FFS_NUM_BUFS, &h->read_aiob.ctx) ||
        io_setup(USB_FFS_NUM_BUFS, &h->write_aiob.ctx)) {
        D("[ aio: got error on io_setup (%d) ]", errno);
    }

    h->read_aiob.fd = h->bulk_out;
    h->write_aiob.fd = h->bulk_in;
    return true;

err:
    if (h->bulk_in > 0) {
        adb_close(h->bulk_in);
        h->bulk_in = -1;
    }
    if (h->bulk_out > 0) {
        adb_close(h->bulk_out);
        h->bulk_out = -1;
    }
    if (h->control > 0) {
        adb_close(h->control);
        h->control = -1;
    }
    return false;
}



