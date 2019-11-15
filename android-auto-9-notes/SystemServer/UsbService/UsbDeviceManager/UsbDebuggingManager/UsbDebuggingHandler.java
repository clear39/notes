

class UsbDebuggingHandler extends Handler {
    public UsbDebuggingHandler(Looper looper) {
        super(looper);
    }


    public void handleMessage(Message msg) {
        switch (msg.what) {
            case MESSAGE_ADB_ENABLED:
                if (mAdbEnabled)
                    break;

                mAdbEnabled = true;

                /***
                 * 启动线程监听adbd
                 * 如果监听到“PK”数据响应，则触发 MESSAGE_ADB_CONFIRM
                 */
                mThread = new UsbDebuggingThread();
                mThread.start();

                break;

            case MESSAGE_ADB_DISABLED:
                if (!mAdbEnabled)
                    break;

                mAdbEnabled = false;

                if (mThread != null) {
                    mThread.stopListening();
                    mThread = null;
                }

                break;

            case MESSAGE_ADB_ALLOW: {
                String key = (String)msg.obj;
                /** */
                String fingerprints = getFingerprints(key);

                if (!fingerprints.equals(mFingerprints)) {
                    Slog.e(TAG, "Fingerprints do not match. Got "  + fingerprints + ", expected " + mFingerprints);
                    break;
                }

                if (msg.arg1 == 1) {
                    writeKey(key);
                }

                if (mThread != null) {
                    mThread.sendResponse("OK");
                }
                break;
            }

            case MESSAGE_ADB_DENY:
                if (mThread != null) {
                    mThread.sendResponse("NO");
                }
                break;

            case MESSAGE_ADB_CONFIRM: {
                if ("trigger_restart_min_framework".equals(SystemProperties.get("vold.decrypt"))) {
                    Slog.d(TAG, "Deferring(推迟) adb confirmation until after vold decrypt (解密)");
                    if (mThread != null) {
                        /**
                         * 往 socket(adbd) 发送 "NO"
                         */
                        mThread.sendResponse("NO");
                    }
                    break;
                }
                String key = (String)msg.obj;
                /**
                 * getFingerprints 为 UsbDebuggingManager 成员方法
                 */
                String fingerprints = getFingerprints(key);
                if ("".equals(fingerprints)) {
                    if (mThread != null) {
                        mThread.sendResponse("NO");
                    }
                    break;
                }
                mFingerprints = fingerprints;
                 /**
                 * startConfirmation 为 UsbDebuggingManager 成员方法
                 */
                startConfirmation(key, mFingerprints);
                break;
            }

            case MESSAGE_ADB_CLEAR:
                deleteKeyFile();
                break;
        }
    }
}