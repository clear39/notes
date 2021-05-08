struct usbfs_urb {
        unsigned char type;
        unsigned char endpoint;
        int status;
        unsigned int flags;
        void *buffer;
        int buffer_length;
        int actual_length;
        int start_frame;
        union {
                int number_of_packets;  /* Only used for isoc urbs */
                unsigned int stream_id; /* Only used with bulk streams */
        };
        int error_count;
        unsigned int signr;
        void *usercontext;
        struct usbfs_iso_packet_desc iso_frame_desc[0];
};