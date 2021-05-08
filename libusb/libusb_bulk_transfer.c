

int API_EXPORTED libusb_bulk_transfer(struct libusb_device_handle *dev_handle,
        unsigned char endpoint, unsigned char *data, int length, int *transferred,
        unsigned int timeout)
{
        return do_sync_bulk_transfer(dev_handle, endpoint, data, length,
                transferred, timeout, LIBUSB_TRANSFER_TYPE_BULK);
}



static int do_sync_bulk_transfer(struct libusb_device_handle *dev_handle,
        unsigned char endpoint, unsigned char *buffer, int length,
        int *transferred, unsigned int timeout, unsigned char type)
{
        struct libusb_transfer *transfer;
        int completed = 0;
        int r;

        if (usbi_handling_events(HANDLE_CTX(dev_handle)))
                return LIBUSB_ERROR_BUSY;

        transfer = libusb_alloc_transfer(0);
        if (!transfer)
                return LIBUSB_ERROR_NO_MEM;

        libusb_fill_bulk_transfer(transfer, dev_handle, endpoint, buffer, length, sync_transfer_cb, &completed, timeout);
        transfer->type = type;

        r = libusb_submit_transfer(transfer);
        if (r < 0) {
                libusb_free_transfer(transfer);
                return r;
        }

        sync_transfer_wait_for_completion(transfer);

        if (transferred)
                *transferred = transfer->actual_length;

        switch (transfer->status) {
        case LIBUSB_TRANSFER_COMPLETED:
                r = 0;
                break;
        case LIBUSB_TRANSFER_TIMED_OUT:
                r = LIBUSB_ERROR_TIMEOUT;
                break;
        case LIBUSB_TRANSFER_STALL:
                r = LIBUSB_ERROR_PIPE;
                break;
        case LIBUSB_TRANSFER_OVERFLOW:
                r = LIBUSB_ERROR_OVERFLOW;
                break;
        case LIBUSB_TRANSFER_NO_DEVICE:
                r = LIBUSB_ERROR_NO_DEVICE;
                break;
        case LIBUSB_TRANSFER_ERROR:
        case LIBUSB_TRANSFER_CANCELLED:
                r = LIBUSB_ERROR_IO;
                break;
                break;
        default:
                usbi_warn(HANDLE_CTX(dev_handle), "unrecognised status code %d", transfer->status);
                r = LIBUSB_ERROR_OTHER;
        }

        libusb_free_transfer(transfer);
        return r;
}






static void sync_transfer_wait_for_completion(struct libusb_transfer *transfer)
{
        int r, *completed = transfer->user_data;
        struct libusb_context *ctx = HANDLE_CTX(transfer->dev_handle);

        while (!*completed) {
                r = libusb_handle_events_completed(ctx, completed);
                if (r < 0) {
                        if (r == LIBUSB_ERROR_INTERRUPTED)
                                continue;
                        usbi_err(ctx, "libusb_handle_events failed: %s, cancelling transfer and retrying", libusb_error_name(r));
                        libusb_cancel_transfer(transfer);
                        continue;
                }
                if (NULL == transfer->dev_handle) {
                        /* transfer completion after libusb_close() */
                        transfer->status = LIBUSB_ERROR_NO_DEVICE;
                        *completed = 1;
                }
        }
}


/** \ingroup libusb_poll
 * Handle any pending events in blocking mode.
 *
 * Like libusb_handle_events(), with the addition of a completed parameter
 * to allow for race free waiting for the completion of a specific transfer.
 *
 * See libusb_handle_events_timeout_completed() for details on the completed
 * parameter.
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param completed pointer to completion integer to check, or NULL
 * \returns 0 on success, or a LIBUSB_ERROR code on failure
 * \ref libusb_mtasync
 */
int API_EXPORTED libusb_handle_events_completed(libusb_context *ctx, int *completed)
{
        struct timeval tv;
        tv.tv_sec = 60;
        tv.tv_usec = 0;
        return libusb_handle_events_timeout_completed(ctx, &tv, completed);
}





/** \ingroup libusb_poll
 * Handle any pending events.
 *
 * libusb determines "pending events" by checking if any timeouts have expired
 * and by checking the set of file descriptors for activity.
 *
 * If a zero timeval is passed, this function will handle any already-pending
 * events and then immediately return in non-blocking style.
 *
 * If a non-zero timeval is passed and no events are currently pending, this
 * function will block waiting for events to handle up until the specified
 * timeout. If an event arrives or a signal is raised, this function will
 * return early.
 *
 * If the parameter completed is not NULL then <em>after obtaining the event
 * handling lock</em> this function will return immediately if the integer
 * pointed to is not 0. This allows for race free waiting for the completion
 * of a specific transfer.
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param tv the maximum time to block waiting for events, or an all zero
 * timeval struct for non-blocking mode
 * \param completed pointer to completion integer to check, or NULL
 * \returns 0 on success, or a LIBUSB_ERROR code on failure
 * \ref libusb_mtasync
 */
int API_EXPORTED libusb_handle_events_timeout_completed(libusb_context *ctx,
        struct timeval *tv, int *completed)
{
        int r; 
        struct timeval poll_timeout;

        USBI_GET_CONTEXT(ctx);
        r = get_next_timeout(ctx, tv, &poll_timeout);
        if (r) {
                /* timeout already expired */
                return handle_timeouts(ctx);
        }

retry:  
        if (libusb_try_lock_events(ctx) == 0) {
                if (completed == NULL || !*completed) {
                        /* we obtained the event lock: do our own event handling */
                        usbi_dbg("doing our own event handling");
                        r = handle_events(ctx, &poll_timeout);
                }
                libusb_unlock_events(ctx);
                return r;
        }

        /* another thread is doing event handling. wait for thread events that
         * notify event completion. */
        libusb_lock_event_waiters(ctx);

        if (completed && *completed)
                goto already_done;

        if (!libusb_event_handler_active(ctx)) {
                /* we hit a race: whoever was event handling earlier finished in the
                 * time it took us to reach this point. try the cycle again. */
                libusb_unlock_event_waiters(ctx);
                usbi_dbg("event handler was active but went away, retrying");
                goto retry;
        }

        usbi_dbg("another thread is doing event handling");
        r = libusb_wait_for_event(ctx, &poll_timeout);

already_done:
        libusb_unlock_event_waiters(ctx);
        
        if (r < 0)
                return r;
        else if (r == 1)
                return handle_timeouts(ctx);
        else    
                return 0;
} 