//  @   system/core/adb/daemon/usb.cpp


struct aio_block {
    std::vector<struct iocb> iocb;
    std::vector<struct iocb*> iocbs;
    std::vector<struct io_event> events;
    aio_context_t ctx;
    int num_submitted;
    int fd;
};


struct usb_handle {
    usb_handle() : kicked(false) {
    }

    std::condition_variable notify;
    std::mutex lock;
    std::atomic<bool> kicked;
    bool open_new_connection = true;

    int (*write)(usb_handle* h, const void* data, int len);
    int (*read)(usb_handle* h, void* data, int len);
    void (*kick)(usb_handle* h);
    void (*close)(usb_handle* h);

    // FunctionFS
    int control = -1;
    int bulk_out = -1; /* "out" from the host's perspective => source for adbd */
    int bulk_in = -1;  /* "in" from the host's perspective => sink for adbd */

    // Access to these blocks is very not thread safe. Have one block for both the
    // read and write threads.
    struct aio_block read_aiob;
    struct aio_block write_aiob;
};