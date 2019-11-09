//  @   system/core/adb/transport.h
//  @   system/core/adb/transport_usb.cpp



explicit UsbConnection::UsbConnection(usb_handle* handle) : handle_(handle) {

}

bool UsbConnection::Read(apacket* packet) {
    int rc = remote_read(packet, handle_);
    return rc == 0;
}


// On Android devices, we rely on the kernel to provide buffered read.
// So we can recover automatically from EOVERFLOW.
static int remote_read(apacket* p, usb_handle* usb) {
    if (usb_read(usb, &p->msg, sizeof(amessage))) {
        PLOG(ERROR) << "remote usb: read terminated (message)";
        return -1;
    }

    if (p->msg.data_length) {
        if (p->msg.data_length > MAX_PAYLOAD) {
            PLOG(ERROR) << "remote usb: read overflow (data length = " << p->msg.data_length << ")";
            return -1;
        }

        p->payload.resize(p->msg.data_length);
        if (usb_read(usb, &p->payload[0], p->payload.size())) {
            PLOG(ERROR) << "remote usb: terminated (data)";
            return -1;
        }
    }

    return 0;
}

//  @   system/core/adb/daemon/usb.cpp
int usb_read(usb_handle* h, void* data, int len) {
    /** 
     * 执行的是 usb_ffs_aio_read 
     * */ 
    return h->read(h, data, len);
}


static int usb_ffs_aio_read(usb_handle* h, void* data, int len) {
    return usb_ffs_do_aio(h, data, len, true);
}

static int usb_ffs_do_aio(usb_handle* h, const void* data, int len, bool read) {
    /**
     *   init_functionfs 中初始化
     * 
     * h->read_aiob.fd 指向 "/dev/usb-ffs/adb/ep1"
     * 
     * h->write_aiob 指向  “/dev/usb-ffs/adb/ep2”
    */
    aio_block* aiob = read ? &h->read_aiob : &h->write_aiob;
    bool zero_packet = false;

    int num_bufs = len / USB_FFS_BULK_SIZE + (len % USB_FFS_BULK_SIZE == 0 ? 0 : 1);
    const char* cur_data = reinterpret_cast<const char*>(data);
    int packet_size = getMaxPacketSize(aiob->fd);

    if (posix_madvise(const_cast<void*>(data), len, POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED) < 0) {
        D("[ Failed to madvise: %d ]", errno);
    }

    for (int i = 0; i < num_bufs; i++) {
        int buf_len = std::min(len, USB_FFS_BULK_SIZE);
        /**
         * @    system/core/libasyncio/AsyncIO.cpp
        */
        io_prep(&aiob->iocb[i], aiob->fd, cur_data, buf_len, 0, read);

        len -= buf_len;
        cur_data += buf_len;

        if (len == 0 && buf_len % packet_size == 0 && read) {
            // adb does not expect the device to send a zero packet after data transfer,
            // but the host *does* send a zero packet for the device to read.
            zero_packet = true;
        }
    }
    if (zero_packet) {
        io_prep(&aiob->iocb[num_bufs], aiob->fd, reinterpret_cast<const void*>(cur_data), packet_size, 0, read);
        num_bufs += 1;
    }

    while (true) {
        /**
         * 
        */
        if (TEMP_FAILURE_RETRY(io_submit(aiob->ctx, num_bufs, aiob->iocbs.data())) < num_bufs) {
            PLOG(ERROR) << "aio: got error submitting " << (read ? "read" : "write");
            return -1;
        }
        /**
         * 
         * 
        */
        if (TEMP_FAILURE_RETRY(io_getevents(aiob->ctx, num_bufs, num_bufs, aiob->events.data(), nullptr)) < num_bufs) {
            PLOG(ERROR) << "aio: got error waiting " << (read ? "read" : "write");
            return -1;
        }
        if (num_bufs == 1 && aiob->events[0].res == -EINTR) {
            continue;
        }
        for (int i = 0; i < num_bufs; i++) {
            if (aiob->events[i].res < 0) {
                errno = -aiob->events[i].res;
                PLOG(ERROR) << "aio: got error event on " << (read ? "read" : "write") << " total bufs " << num_bufs;
                return -1;
            }
        }
        return 0;
    }
}



