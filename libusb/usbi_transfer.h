struct usbi_transfer {
        int num_iso_packets;
        struct list_head list;
        struct list_head completed_list;
        struct timeval timeout;
        int transferred;
        uint32_t stream_id;
        uint8_t state_flags;   /* Protected by usbi_transfer->lock */
        uint8_t timeout_flags; /* Protected by the flying_stransfers_lock */

        /* this lock is held during libusb_submit_transfer() and
         * libusb_cancel_transfer() (allowing the OS backend to prevent duplicate
         * cancellation, submission-during-cancellation, etc). the OS backend
         * should also take this lock in the handle_events path, to prevent the user
         * cancelling the transfer from another thread while you are processing
         * its completion (presumably there would be races within your OS backend
         * if this were possible).
         * Note paths taking both this and the flying_transfers_lock must
         * always take the flying_transfers_lock first */
        usbi_mutex_t lock;
};

//  usbi_transfer -> libusb_transfer    // usbi_transfer + sizeof(struct usbi_transfer)
#define USBI_TRANSFER_TO_LIBUSB_TRANSFER(transfer)   ((struct libusb_transfer *)(((unsigned char *)(transfer)) + sizeof(struct usbi_transfer)))


// libusb_transfer -> usbi_transfer   // libusb_transfer - sizeof(struct usbi_transfer)
#define LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer)     ((struct usbi_transfer *)(((unsigned char *)(transfer)) - sizeof(struct usbi_transfer)))


// alloc_size = sizeof(struct usbi_transfer)  sizeof(struct libusb_transfer) + (sizeof(struct libusb_iso_packet_descriptor) * (size_t)iso_packets)
