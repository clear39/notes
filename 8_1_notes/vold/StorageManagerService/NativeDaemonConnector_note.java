//	@base/services/core/java/com/android/server/NativeDaemonConnector.java

/**
 * Generic connector class for interfacing with a native daemon which uses the
 * {@code libsysutils} FrameworkListener protocol.
 */
final class NativeDaemonConnector implements Runnable, Handler.Callback, Watchdog.Monitor {
	NativeDaemonConnector(INativeDaemonConnectorCallbacks callbacks, String socket,int responseQueueSize, String logTag, int maxLogSize, PowerManager.WakeLock wl) {
        this(callbacks, socket, responseQueueSize, logTag, maxLogSize, wl,FgThread.get().getLooper());
    }

    NativeDaemonConnector(INativeDaemonConnectorCallbacks callbacks, String socket,int responseQueueSize, String logTag, int maxLogSize, PowerManager.WakeLock wl,Looper looper) {
        mCallbacks = callbacks;
        mSocket = socket;
        mResponseQueue = new ResponseQueue(responseQueueSize);
        mWakeLock = wl;
        if (mWakeLock != null) {
            mWakeLock.setReferenceCounted(true);
        }
        mLooper = looper;
        mSequenceNumber = new AtomicInteger(0);
        TAG = logTag != null ? logTag : "NativeDaemonConnector";
        mLocalLog = new LocalLog(maxLogSize);
    }

    /**
     * Like SystemClock.uptimeMillis, except truncated to an int so it will fit in a message arg.
     * Inaccurate across 49.7 days of uptime, but only used for debugging.
     */
    private int uptimeMillisInt() {
        return (int) SystemClock.uptimeMillis() & Integer.MAX_VALUE;
    }


    @Override
    public boolean handleMessage(Message msg) {
        final String event = (String) msg.obj;
        final int start = uptimeMillisInt();
        final int sent = msg.arg1;
        try {
            if (!mCallbacks.onEvent(msg.what, event, NativeDaemonEvent.unescapeArgs(event))) {
                log(String.format("Unhandled event '%s'", event));
            }
        } catch (Exception e) {
            loge("Error handling '" + event + "': " + e);
        } finally {
            if (mCallbacks.onCheckHoldWakeLock(msg.what) && mWakeLock != null) {
                mWakeLock.release();
            }
            final int end = uptimeMillisInt();
            if (start > sent && start - sent > WARN_EXECUTE_DELAY_MS) {
                loge(String.format("NDC event {%s} processed too late: %dms", event, start - sent));
            }
            if (end > start && end - start > WARN_EXECUTE_DELAY_MS) {
                loge(String.format("NDC event {%s} took too long: %dms", event, end - start));
            }
        }
        return true;
    }

    private void listenToSocket() throws IOException {
        LocalSocket socket = null;

        try {
            socket = new LocalSocket();
            LocalSocketAddress address = determineSocketAddress();

            socket.connect(address);

            InputStream inputStream = socket.getInputStream();
            synchronized (mDaemonLock) {
                mOutputStream = socket.getOutputStream();
            }

            mCallbacks.onDaemonConnected();

            FileDescriptor[] fdList = null;
            byte[] buffer = new byte[BUFFER_SIZE];
            int start = 0;

            while (true) {
                int count = inputStream.read(buffer, start, BUFFER_SIZE - start);
                if (count < 0) {
                    loge("got " + count + " reading with start = " + start);
                    break;
                }
                fdList = socket.getAncillaryFileDescriptors();

                // Add our starting point to the count and reset the start.
                count += start;
                start = 0;

                for (int i = 0; i < count; i++) {
                    if (buffer[i] == 0) {
                        // Note - do not log this raw message since it may contain
                        // sensitive data
                        final String rawEvent = new String(
                                buffer, start, i - start, StandardCharsets.UTF_8);

                        boolean releaseWl = false;
                        try {
                            final NativeDaemonEvent event =
                                    NativeDaemonEvent.parseRawEvent(rawEvent, fdList);

                            log("RCV <- {" + event + "}");

                            if (event.isClassUnsolicited()) {
                                // TODO: migrate to sending NativeDaemonEvent instances
                                if (mCallbacks.onCheckHoldWakeLock(event.getCode())
                                        && mWakeLock != null) {
                                    mWakeLock.acquire();
                                    releaseWl = true;
                                }
                                Message msg = mCallbackHandler.obtainMessage(
                                        event.getCode(), uptimeMillisInt(), 0, event.getRawEvent());
                                if (mCallbackHandler.sendMessage(msg)) {
                                    releaseWl = false;
                                }
                            } else {
                                mResponseQueue.add(event.getCmdNumber(), event);
                            }
                        } catch (IllegalArgumentException e) {
                            log("Problem parsing message " + e);
                        } finally {
                            if (releaseWl) {
                                mWakeLock.release();
                            }
                        }

                        start = i + 1;
                    }
                }

                if (start == 0) {
                    log("RCV incomplete");
                }

                // We should end at the amount we read. If not, compact then
                // buffer and read again.
                if (start != count) {
                    final int remaining = BUFFER_SIZE - start;
                    System.arraycopy(buffer, start, buffer, 0, remaining);
                    start = remaining;
                } else {
                    start = 0;
                }
            }
        } catch (IOException ex) {
            loge("Communications error: " + ex);
            throw ex;
        } finally {
            synchronized (mDaemonLock) {
                if (mOutputStream != null) {
                    try {
                        loge("closing stream for " + mSocket);
                        mOutputStream.close();
                    } catch (IOException e) {
                        loge("Failed closing output stream: " + e);
                    }
                    mOutputStream = null;
                }
            }

            try {
                if (socket != null) {
                    socket.close();
                }
            } catch (IOException ex) {
                loge("Failed closing socket: " + ex);
            }
        }
    }

}
