//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbDeviceManager.java
public class UsbDeviceManager implements ActivityManagerInternal.ScreenObserver {

    static {
        sBlackListedInterfaces = new HashSet<>();
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_AUDIO);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_COMM);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_PRINTER);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_MASS_STORAGE);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_HUB);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_CDC_DATA);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_CSCID);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_CONTENT_SEC);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_VIDEO);
        sBlackListedInterfaces.add(UsbConstants.USB_CLASS_WIRELESS_CONTROLLER);
    }


    public UsbDeviceManager(Context context, UsbAlsaManager alsaManager,UsbSettingsManager settingsManager) {
        mContext = context;
        mContentResolver = context.getContentResolver();
        PackageManager pm = mContext.getPackageManager();
        mHasUsbAccessory = pm.hasSystemFeature(PackageManager.FEATURE_USB_ACCESSORY);
        /***
         * 
         */
        initRndisAddress();

        boolean halNotPresent = false;
        try {
            IUsbGadget.getService(true);
        } catch (RemoteException e) {
            Slog.e(TAG, "USB GADGET HAL present but exception thrown", e);
        } catch (NoSuchElementException e) {
            halNotPresent = true;
            Slog.i(TAG, "USB GADGET HAL not present in the device", e);
        }

        mControlFds = new HashMap<>();
        FileDescriptor mtpFd = nativeOpenControl(UsbManager.USB_FUNCTION_MTP);
        if (mtpFd == null) {
            Slog.e(TAG, "Failed to open control for mtp");
        }
        mControlFds.put(UsbManager.FUNCTION_MTP, mtpFd);
        FileDescriptor ptpFd = nativeOpenControl(UsbManager.USB_FUNCTION_PTP);
        if (mtpFd == null) {
            Slog.e(TAG, "Failed to open control for mtp");
        }
        mControlFds.put(UsbManager.FUNCTION_PTP, ptpFd);

        boolean secureAdbEnabled = SystemProperties.getBoolean("ro.adb.secure", false);
        boolean dataEncrypted = "1".equals(SystemProperties.get("vold.decrypt"));
        if (secureAdbEnabled && !dataEncrypted) {
            mDebuggingManager = new UsbDebuggingManager(context);
        }

        if (halNotPresent) {
            /**
             * Initialze the legacy UsbHandler
             */
            mHandler = new UsbHandlerLegacy(FgThread.get().getLooper(), mContext, this,mDebuggingManager, alsaManager, settingsManager);
        } else {
            /**
             * Initialize HAL based UsbHandler
             */
            mHandler = new UsbHandlerHal(FgThread.get().getLooper(), mContext, this,mDebuggingManager, alsaManager, settingsManager);
        }

        /**
         * nativeIsStartRequested  @   frameworks/base/services/core/jni/com_android_server_UsbDeviceManager.cpp
         * 打开/dev/usb_accessory设备，并且 int result = ioctl(fd, ACCESSORY_IS_START_REQUESTED);
         * bionic/libc/kernel/android/uapi/linux/usb/f_accessory.h:50:#define ACCESSORY_IS_START_REQUESTED _IO('M', 7)
         */
        if (nativeIsStartRequested()) {
            if (DEBUG) Slog.d(TAG, "accessory attached at boot");
            startAccessoryMode();
        }

        BroadcastReceiver portReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                UsbPort port = intent.getParcelableExtra(UsbManager.EXTRA_PORT);
                UsbPortStatus status = intent.getParcelableExtra(UsbManager.EXTRA_PORT_STATUS);
                mHandler.updateHostState(port, status);
            }
        };

        BroadcastReceiver chargingReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                int chargePlug = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
                boolean usbCharging = chargePlug == BatteryManager.BATTERY_PLUGGED_USB;
                mHandler.sendMessage(MSG_UPDATE_CHARGING_STATE, usbCharging);
            }
        };

        BroadcastReceiver hostReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Iterator devices = ((UsbManager) context.getSystemService(Context.USB_SERVICE)).getDeviceList().entrySet().iterator();
                if (intent.getAction().equals(UsbManager.ACTION_USB_DEVICE_ATTACHED)) {
                    mHandler.sendMessage(MSG_UPDATE_HOST_STATE, devices, true);
                } else {
                    mHandler.sendMessage(MSG_UPDATE_HOST_STATE, devices, false);
                }
            }
        };

        BroadcastReceiver languageChangedReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mHandler.sendEmptyMessage(MSG_LOCALE_CHANGED);
            }
        };

        mContext.registerReceiver(portReceiver,new IntentFilter(UsbManager.ACTION_USB_PORT_CHANGED));
        mContext.registerReceiver(chargingReceiver,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));

        IntentFilter filter = new IntentFilter(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        mContext.registerReceiver(hostReceiver, filter);

        mContext.registerReceiver(languageChangedReceiver,new IntentFilter(Intent.ACTION_LOCALE_CHANGED));

        // Watch for USB configuration changes
        mUEventObserver = new UsbUEventObserver();
        //     private static final String USB_STATE_MATCH = "DEVPATH=/devices/virtual/android_usb/android0";
        mUEventObserver.startObserving(USB_STATE_MATCH);
        //     private static final String ACCESSORY_START_MATCH = "DEVPATH=/devices/virtual/misc/usb_accessory";
        mUEventObserver.startObserving(ACCESSORY_START_MATCH);

        // register observer to listen for settings changes
        mContentResolver.registerContentObserver(Settings.Global.getUriFor(Settings.Global.ADB_ENABLED),false, new AdbSettingsObserver());
    }

    private static void initRndisAddress() {
        // configure RNDIS ethernet address based on our serial number using the same algorithm
        // we had been previously using in kernel board files
        final int ETH_ALEN = 6;
        int address[] = new int[ETH_ALEN];
        // first byte is 0x02 to signify a locally administered address
        address[0] = 0x02;

        String serial = SystemProperties.get("ro.serialno", "1234567890ABCDEF");
        int serialLength = serial.length();
        // XOR the USB serial across the remaining 5 bytes
        for (int i = 0; i < serialLength; i++) {
            address[i % (ETH_ALEN - 1) + 1] ^= (int) serial.charAt(i);
        }
        String addrString = String.format(Locale.US, "%02X:%02X:%02X:%02X:%02X:%02X",address[0], address[1], address[2], address[3], address[4], address[5]);
        try {
            /**
             * private static final String RNDIS_ETH_ADDR_PATH = "/sys/class/android_usb/android0/f_rndis/ethaddr";
             */
            FileUtils.stringToFile(RNDIS_ETH_ADDR_PATH, addrString);
        } catch (IOException e) {
            Slog.e(TAG, "failed to write to " + RNDIS_ETH_ADDR_PATH);
        }
    }

    


    /** 
     * Called when a user is unlocked. 
     * 
     * UsbService.onUnlockUser
     * --> mDeviceManager.onUnlockUser(user);
     */
    public void onUnlockUser(int userHandle) {
        onKeyguardStateChanged(false);
    }

    @Override
    public void onKeyguardStateChanged(boolean isShowing) {
        int userHandle = ActivityManager.getCurrentUser();
        boolean secure = mContext.getSystemService(KeyguardManager.class).isDeviceSecure(userHandle);
        if (DEBUG) {
            Slog.v(TAG, "onKeyguardStateChanged: isShowing:" + isShowing + " secure:" + secure + " user:" + userHandle);
        }
        // We are unlocked when the keyguard is down or non-secure.
        mHandler.sendMessage(MSG_UPDATE_SCREEN_LOCK, (isShowing && secure));
    }












    /**
     * UsbUEventObserver.onUEvent
     * --> startAccessoryMode()
     */
    private void startAccessoryMode() {
        if (!mHasUsbAccessory) return;
        /**
         *  读取 /dev/usb_accessory 
         */
        mAccessoryStrings = nativeGetAccessoryStrings();
        boolean enableAudio = (nativeGetAudioMode() == AUDIO_MODE_SOURCE);
        // don't start accessory mode if our mandatory strings have not been set
        boolean enableAccessory = (mAccessoryStrings != null &&
                mAccessoryStrings[UsbAccessory.MANUFACTURER_STRING] != null &&
                mAccessoryStrings[UsbAccessory.MODEL_STRING] != null);

        long functions = UsbManager.FUNCTION_NONE;
        if (enableAccessory) {
            functions |= UsbManager.FUNCTION_ACCESSORY;
        }
        if (enableAudio) {
            functions |= UsbManager.FUNCTION_AUDIO_SOURCE;
        }

        if (functions != UsbManager.FUNCTION_NONE) {
            mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_ACCESSORY_MODE_ENTER_TIMEOUT),ACCESSORY_REQUEST_TIMEOUT);
            setCurrentFunctions(functions);
        }
    }


}