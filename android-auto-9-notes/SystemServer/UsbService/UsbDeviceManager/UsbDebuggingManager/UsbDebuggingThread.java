

class UsbDebuggingThread extends Thread {

    UsbDebuggingThread() {
        super(TAG);
    }

    @Override
    public void run() {
        if (DEBUG) Slog.d(TAG, "Entering thread");
        while (true) {
            synchronized (this) {
                if (mStopped) {
                    if (DEBUG) Slog.d(TAG, "Exiting thread");
                    return;
                }
                try {
                    openSocketLocked();
                } catch (Exception e) {
                    /* Don't loop too fast if adbd dies, before init restarts it */
                    SystemClock.sleep(1000);
                }
            }
            try {
                listenToSocket();
            } catch (Exception e) {
                /* Don't loop too fast if adbd dies, before init restarts it */
                SystemClock.sleep(1000);
            }
        }
    }


    private void openSocketLocked() throws IOException {
        try {
            /**
             * private static final String ADBD_SOCKET = "adbd";
             */
            LocalSocketAddress address = new LocalSocketAddress(ADBD_SOCKET,LocalSocketAddress.Namespace.RESERVED);
            mInputStream = null;

            if (DEBUG) Slog.d(TAG, "Creating socket");
            mSocket = new LocalSocket();
            mSocket.connect(address);

            mOutputStream = mSocket.getOutputStream();
            mInputStream = mSocket.getInputStream();
        } catch (IOException ioe) {
            closeSocketLocked();
            throw ioe;
        }
    }


    private void listenToSocket() throws IOException {
        try {
            byte[] buffer = new byte[BUFFER_SIZE];
            while (true) {
                int count = mInputStream.read(buffer);
                if (count < 0) {
                    break;
                }

                if (buffer[0] == 'P' && buffer[1] == 'K') {
                    String key = new String(Arrays.copyOfRange(buffer, 2, count));
                    Slog.d(TAG, "Received public key: " + key);
                    Message msg = mHandler.obtainMessage(UsbDebuggingHandler.MESSAGE_ADB_CONFIRM);
                    msg.obj = key;
                    mHandler.sendMessage(msg);
                } else {
                    Slog.e(TAG, "Wrong message: " + (new String(Arrays.copyOfRange(buffer, 0, 2))));
                    break;
                }
            }
        } finally {
            synchronized (this) {
                closeSocketLocked();
            }
        }
    }


    void sendResponse(String msg) {
        synchronized (this) {
            if (!mStopped && mOutputStream != null) {
                try {
                    mOutputStream.write(msg.getBytes());
                }
                catch (IOException ex) {
                    Slog.e(TAG, "Failed to write response:", ex);
                }
            }
        }
    }

}