

abstract static class UsbHandler extends Handler {

    UsbHandler(Looper looper, Context context, UsbDeviceManager deviceManager,
                UsbDebuggingManager debuggingManager, UsbAlsaManager alsaManager,
                UsbSettingsManager settingsManager) {
        super(looper);
        mContext = context;
        mDebuggingManager = debuggingManager;
        mUsbDeviceManager = deviceManager;
        mUsbAlsaManager = alsaManager;
        mSettingsManager = settingsManager;
        mContentResolver = context.getContentResolver();

        mCurrentUser = ActivityManager.getCurrentUser();
        mScreenLocked = true;

        /**
        * Use the normal bootmode persistent prop to maintain state of adb across
        * all boot modes.
        * protected static final String USB_PERSISTENT_CONFIG_PROPERTY = "persist.sys.usb.config";
        */
        mAdbEnabled = UsbHandlerLegacy.containsFunction(getSystemProperty(USB_PERSISTENT_CONFIG_PROPERTY, ""), UsbManager.USB_FUNCTION_ADB);
        
        /***
         * /data/data/android/
         */
        mSettings = getPinnedSharedPrefs(mContext);
        if (mSettings == null) {
            Slog.e(TAG, "Couldn't load shared preferences");
        } else {
            mScreenUnlockedFunctions = UsbManager.usbFunctionsFromString(mSettings.getString(String.format(Locale.ENGLISH, UNLOCKED_CONFIG_PREF, mCurrentUser),""));
        }

        // We do not show the USB notification if the primary volume supports mass storage.
        // The legacy mass storage UI will be used instead.
        final StorageManager storageManager = StorageManager.from(mContext);
        final StorageVolume primary = storageManager.getPrimaryVolume();

        boolean massStorageSupported = primary != null && primary.allowMassStorage();
        mUseUsbNotification = !massStorageSupported && mContext.getResources().getBoolean(com.android.internal.R.bool.config_usbChargingMessage);
    }



    @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_UPDATE_STATE:
                    mConnected = (msg.arg1 == 1);
                    mConfigured = (msg.arg2 == 1);

                    updateUsbNotification(false);
                    updateAdbNotification(false);
                    if (mBootCompleted) {
                        updateUsbStateBroadcastIfNeeded(getAppliedFunctions(mCurrentFunctions));
                    }
                    if ((mCurrentFunctions & UsbManager.FUNCTION_ACCESSORY) != 0) {
                        updateCurrentAccessory();
                    }
                    if (mBootCompleted) {
                        if (!mConnected && !hasMessages(MSG_ACCESSORY_MODE_ENTER_TIMEOUT)
                                && !hasMessages(MSG_FUNCTION_SWITCH_TIMEOUT)) {
                            // restore defaults when USB is disconnected
                            if (!mScreenLocked
                                    && mScreenUnlockedFunctions != UsbManager.FUNCTION_NONE) {
                                setScreenUnlockedFunctions();
                            } else {
                                setEnabledFunctions(UsbManager.FUNCTION_NONE, false);
                            }
                        }
                        updateUsbFunctions();
                    } else {
                        mPendingBootBroadcast = true;
                    }
                    break;
                case MSG_UPDATE_PORT_STATE:
                    SomeArgs args = (SomeArgs) msg.obj;
                    boolean prevHostConnected = mHostConnected;
                    UsbPort port = (UsbPort) args.arg1;
                    UsbPortStatus status = (UsbPortStatus) args.arg2;
                    mHostConnected = status.getCurrentDataRole() == UsbPort.DATA_ROLE_HOST;
                    mSourcePower = status.getCurrentPowerRole() == UsbPort.POWER_ROLE_SOURCE;
                    mSinkPower = status.getCurrentPowerRole() == UsbPort.POWER_ROLE_SINK;
                    mAudioAccessoryConnected =
                            (status.getCurrentMode() == UsbPort.MODE_AUDIO_ACCESSORY);
                    mAudioAccessorySupported = port.isModeSupported(UsbPort.MODE_AUDIO_ACCESSORY);
                    // Ideally we want to see if PR_SWAP and DR_SWAP is supported.
                    // But, this should be suffice, since, all four combinations are only supported
                    // when PR_SWAP and DR_SWAP are supported.
                    mSupportsAllCombinations = status.isRoleCombinationSupported(
                            UsbPort.POWER_ROLE_SOURCE, UsbPort.DATA_ROLE_HOST)
                            && status.isRoleCombinationSupported(UsbPort.POWER_ROLE_SINK,
                            UsbPort.DATA_ROLE_HOST)
                            && status.isRoleCombinationSupported(UsbPort.POWER_ROLE_SOURCE,
                            UsbPort.DATA_ROLE_DEVICE)
                            && status.isRoleCombinationSupported(UsbPort.POWER_ROLE_SINK,
                            UsbPort.DATA_ROLE_HOST);

                    args.recycle();
                    updateUsbNotification(false);
                    if (mBootCompleted) {
                        if (mHostConnected || prevHostConnected) {
                            updateUsbStateBroadcastIfNeeded(getAppliedFunctions(mCurrentFunctions));
                        }
                    } else {
                        mPendingBootBroadcast = true;
                    }
                    break;
                case MSG_UPDATE_CHARGING_STATE:
                    mUsbCharging = (msg.arg1 == 1);
                    updateUsbNotification(false);
                    break;
                case MSG_UPDATE_HOST_STATE:
                    Iterator devices = (Iterator) msg.obj;
                    boolean connected = (msg.arg1 == 1);

                    if (DEBUG) {
                        Slog.i(TAG, "HOST_STATE connected:" + connected);
                    }

                    mHideUsbNotification = false;
                    while (devices.hasNext()) {
                        Map.Entry pair = (Map.Entry) devices.next();
                        if (DEBUG) {
                            Slog.i(TAG, pair.getKey() + " = " + pair.getValue());
                        }
                        UsbDevice device = (UsbDevice) pair.getValue();
                        int configurationCount = device.getConfigurationCount() - 1;
                        while (configurationCount >= 0) {
                            UsbConfiguration config = device.getConfiguration(configurationCount);
                            configurationCount--;
                            int interfaceCount = config.getInterfaceCount() - 1;
                            while (interfaceCount >= 0) {
                                UsbInterface intrface = config.getInterface(interfaceCount);
                                interfaceCount--;
                                if (sBlackListedInterfaces.contains(intrface.getInterfaceClass())) {
                                    mHideUsbNotification = true;
                                    break;
                                }
                            }
                        }
                    }
                    updateUsbNotification(false);
                    break;
                case MSG_ENABLE_ADB:
                    /**
                     * 监听adb设备开关
                     */
                    setAdbEnabled(msg.arg1 == 1);
                    break;
                case MSG_SET_CURRENT_FUNCTIONS:
                    long functions = (Long) msg.obj;
                    setEnabledFunctions(functions, false);
                    break;
                case MSG_SET_SCREEN_UNLOCKED_FUNCTIONS:
                    mScreenUnlockedFunctions = (Long) msg.obj;
                    if (mSettings != null) {
                        SharedPreferences.Editor editor = mSettings.edit();
                        editor.putString(String.format(Locale.ENGLISH, UNLOCKED_CONFIG_PREF,
                                mCurrentUser),
                                UsbManager.usbFunctionsToString(mScreenUnlockedFunctions));
                        editor.commit();
                    }
                    if (!mScreenLocked && mScreenUnlockedFunctions != UsbManager.FUNCTION_NONE) {
                        // If the screen is unlocked, also set current functions.
                        setScreenUnlockedFunctions();
                    }
                    break;
                case MSG_UPDATE_SCREEN_LOCK:
                    if (msg.arg1 == 1 == mScreenLocked) {
                        break;
                    }
                    mScreenLocked = msg.arg1 == 1;
                    if (!mBootCompleted) {
                        break;
                    }
                    if (mScreenLocked) {
                        if (!mConnected) {
                            setEnabledFunctions(UsbManager.FUNCTION_NONE, false);
                        }
                    } else {
                        if (mScreenUnlockedFunctions != UsbManager.FUNCTION_NONE
                                && mCurrentFunctions == UsbManager.FUNCTION_NONE) {
                            // Set the screen unlocked functions if current function is charging.
                            setScreenUnlockedFunctions();
                        }
                    }
                    break;
                case MSG_UPDATE_USER_RESTRICTIONS:
                    // Restart the USB stack if USB transfer is enabled but no longer allowed.
                    if (isUsbDataTransferActive(mCurrentFunctions) && !isUsbTransferAllowed()) {
                        setEnabledFunctions(UsbManager.FUNCTION_NONE, true);
                    }
                    break;
                case MSG_SYSTEM_READY:
                    mNotificationManager = (NotificationManager)
                            mContext.getSystemService(Context.NOTIFICATION_SERVICE);

                    // Ensure that the notification channels are set up
                    if (isTv()) {
                        // TV-specific notification channel
                        mNotificationManager.createNotificationChannel(
                                new NotificationChannel(ADB_NOTIFICATION_CHANNEL_ID_TV,
                                        mContext.getString(
                                                com.android.internal.R.string
                                                        .adb_debugging_notification_channel_tv),
                                        NotificationManager.IMPORTANCE_HIGH));
                    }
                    mSystemReady = true;
                    finishBoot();
                    break;
                case MSG_LOCALE_CHANGED:
                    updateAdbNotification(true);
                    updateUsbNotification(true);
                    break;
                case MSG_BOOT_COMPLETED:
                    mBootCompleted = true;
                    finishBoot();
                    break;
                case MSG_USER_SWITCHED: {
                    if (mCurrentUser != msg.arg1) {
                        if (DEBUG) {
                            Slog.v(TAG, "Current user switched to " + msg.arg1);
                        }
                        mCurrentUser = msg.arg1;
                        mScreenLocked = true;
                        mScreenUnlockedFunctions = UsbManager.FUNCTION_NONE;
                        if (mSettings != null) {
                            mScreenUnlockedFunctions = UsbManager.usbFunctionsFromString(
                                    mSettings.getString(String.format(Locale.ENGLISH,
                                            UNLOCKED_CONFIG_PREF, mCurrentUser), ""));
                        }
                        setEnabledFunctions(UsbManager.FUNCTION_NONE, false);
                    }
                    break;
                }
                case MSG_ACCESSORY_MODE_ENTER_TIMEOUT: {
                    if (DEBUG) {
                        Slog.v(TAG, "Accessory mode enter timeout: " + mConnected);
                    }
                    if (!mConnected || (mCurrentFunctions & UsbManager.FUNCTION_ACCESSORY) == 0) {
                        notifyAccessoryModeExit();
                    }
                    break;
                }
            }
        }




        private void setAdbEnabled(boolean enable) {
            if (DEBUG) Slog.d(TAG, "setAdbEnabled: " + enable);
            if (enable != mAdbEnabled) {
                mAdbEnabled = enable;

                if (enable) {
                    /**
                     * public static final String USB_FUNCTION_ADB = "adb";
                     * protected static final String USB_PERSISTENT_CONFIG_PROPERTY = "persist.sys.usb.config";
                     * 
                     */
                    setSystemProperty(USB_PERSISTENT_CONFIG_PROPERTY, UsbManager.USB_FUNCTION_ADB);
                } else {
                    setSystemProperty(USB_PERSISTENT_CONFIG_PROPERTY, "");
                }

                setEnabledFunctions(mCurrentFunctions, true);
                updateAdbNotification(false);
            }

            if (mDebuggingManager != null) {
                mDebuggingManager.setAdbEnabled(mAdbEnabled);
            }
        }


}