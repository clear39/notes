/** \ingroup libusb_asyncio
 * The generic USB transfer structure. The user populates this structure and
 * then submits it in order to request a transfer. After the transfer has
 * completed, the library populates the transfer with the results and passes
 * it back to the user.
 */

struct libusb_transfer {
        /** Handle of the device that this transfer will be submitted to */
        libusb_device_handle *dev_handle;

        /** A bitwise OR combination of \ref libusb_transfer_flags. */
        uint8_t flags;

        /** Address of the endpoint where this transfer will be sent. */
        unsigned char endpoint;

        /** Type of the endpoint from \ref libusb_transfer_type */
        unsigned char type;

        /** Timeout for this transfer in milliseconds. A value of 0 indicates no
         * timeout. */
        unsigned int timeout;

        /** The status of the transfer. Read-only, and only for use within
         * transfer callback function.
         *
         * If this is an isochronous transfer, this field may read COMPLETED even
         * if there were errors in the frames. Use the
         * \ref libusb_iso_packet_descriptor::status "status" field in each packet
         * to determine if errors occurred. */
        enum libusb_transfer_status status;

        /** Length of the data buffer. Must be non-negative. */
        int length;

        /** Actual length of data that was transferred. Read-only, and only for
         * use within transfer callback function. Not valid for isochronous
         * endpoint transfers. */
        int actual_length;

        /** Callback function. This will be invoked when the transfer completes,
         * fails, or is cancelled. */
        libusb_transfer_cb_fn callback;

        /** User context data to pass to the callback function. */
        void *user_data;

        /** Data buffer */
        unsigned char *buffer;

        /** Number of isochronous packets. Only used for I/O with isochronous
         * endpoints. Must be non-negative. */
        int num_iso_packets;
        
        /** Isochronous packet descriptors, for isochronous transfers only. */
        struct libusb_iso_packet_descriptor iso_packet_desc[ZERO_SIZED_ARRAY];
};
