struct libusb_device_handle {
        /* lock protects claimed_interfaces */
        usbi_mutex_t lock;
        unsigned long claimed_interfaces;

        struct list_head list;
        struct libusb_device *dev;
        int auto_detach_kernel_driver;

        PTR_ALIGNED unsigned char os_priv[ZERO_SIZED_ARRAY];
};