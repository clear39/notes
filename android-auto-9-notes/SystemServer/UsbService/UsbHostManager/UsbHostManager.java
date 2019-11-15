public class UsbHostManager {
    /*
     * UsbHostManager
     */
    public UsbHostManager(Context context, UsbAlsaManager alsaManager,UsbSettingsManager settingsManager) {
        mContext = context;

        /**
         * 格式 /dev/bus/usb/001/
         * config_usbHostBlacklist 为空
         * mHostBlacklist 用来屏蔽不需要处理的usb设备
         */
        mHostBlacklist = context.getResources().getStringArray(com.android.internal.R.array.config_usbHostBlacklist);
        mUsbAlsaManager = alsaManager;
        mSettingsManager = settingsManager;
        /**
         * config_UsbDeviceConnectionHandling_component 为空
         * 格式 "com.foo/.Blah" becomes package="com.foo" class="com.foo.Blah".
         * 设置组件包名和类名
         */
        String deviceConnectionHandler = context.getResources().getString(com.android.internal.R.string.config_UsbDeviceConnectionHandling_component);
        if (!TextUtils.isEmpty(deviceConnectionHandler)) {
            setUsbDeviceConnectionHandler(ComponentName.unflattenFromString(deviceConnectionHandler));
        }
    }

    public void setUsbDeviceConnectionHandler(@Nullable ComponentName usbDeviceConnectionHandler) {
        synchronized (mHandlerLock) {
            mUsbDeviceConnectionHandler = usbDeviceConnectionHandler;
        }
    }


    public void systemReady() {
        synchronized (mLock) {
            // Create a thread to call into native code to wait for USB host events.
            // This thread will call us back on usbDeviceAdded and usbDeviceRemoved.
            /**
             * private native void monitorUsbHostBus();
             * 
             * @    frameworks/base/services/core/jni/com_android_server_UsbHostManager.cpp
             * usb 主要监听 /dev/bus/usb 目录下 文件的变化
             */
            Runnable runnable = this::monitorUsbHostBus;
            new Thread(null, runnable, "UsbService host thread").start();
        }
    }


    /* Called from JNI in monitorUsbHostBus() to report new USB devices
       Returns true if successful, i.e. the USB Audio device descriptors are
       correctly parsed and the unique device is added to the audio device list.
       
       usbDeviceAdded 为jni层回调
     */
    @SuppressWarnings("unused")
    private boolean usbDeviceAdded(String deviceAddress, int deviceClass, int deviceSubclass,byte[] descriptors) {
        if (DEBUG) {
            Slog.d(TAG, "usbDeviceAdded(" + deviceAddress + ") - start");
        }

        /**
         * 排除不处理的黑名单usb
         */
        if (isBlackListed(deviceAddress)) {
            if (DEBUG) {
                Slog.d(TAG, "device address is black listed");
            }
            return false;
        }
        UsbDescriptorParser parser = new UsbDescriptorParser(deviceAddress, descriptors);
        logUsbDevice(parser);

        if (isBlackListed(deviceClass, deviceSubclass)) {
            if (DEBUG) {
                Slog.d(TAG, "device class is black listed");
            }
            return false;
        }



        synchronized (mLock) {
            if (mDevices.get(deviceAddress) != null) {
                Slog.w(TAG, "device already on mDevices list: " + deviceAddress);
                //TODO If this is the same peripheral as is being connected, replace
                // it with the new connection.
                return false;
            }

            UsbDevice newDevice = parser.toAndroidUsbDevice();
            if (newDevice == null) {
                Slog.e(TAG, "Couldn't create UsbDevice object.");
                // Tracking
                addConnectionRecord(deviceAddress, ConnectionRecord.CONNECT_BADDEVICE,parser.getRawDescriptors());
            } else {

                mDevices.put(deviceAddress, newDevice);
                Slog.d(TAG, "Added device " + newDevice);

                // It is fine to call this only for the current user as all broadcasts are
                // sent to all profiles of the user and the dialogs should only show once.
                ComponentName usbDeviceConnectionHandler = getUsbDeviceConnectionHandler();
                if (usbDeviceConnectionHandler == null) {
                    getCurrentUserSettings().deviceAttached(newDevice);
                } else {
                    getCurrentUserSettings().deviceAttachedForFixedHandler(newDevice,usbDeviceConnectionHandler);
                }

                mUsbAlsaManager.usbDeviceAdded(deviceAddress, newDevice, parser);

                // Tracking
                addConnectionRecord(deviceAddress, ConnectionRecord.CONNECT,parser.getRawDescriptors());
            }
        }

        if (DEBUG) {
            Slog.d(TAG, "beginUsbDeviceAdded(" + deviceAddress + ") end");
        }

        return true;
    }

    private void addConnectionRecord(String deviceAddress, int mode, byte[] rawDescriptors) {
        mNumConnects++;
        /**
         * private static final int MAX_CONNECT_RECORDS = 32;
         */
        while (mConnections.size() >= MAX_CONNECT_RECORDS) {
            mConnections.removeFirst();
        }
        ConnectionRecord rec = new ConnectionRecord(deviceAddress, mode, rawDescriptors);
        mConnections.add(rec);
        if (mode != ConnectionRecord.DISCONNECT) {
            mLastConnect = rec;
        }
    }


    

}