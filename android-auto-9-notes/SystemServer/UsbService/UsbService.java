//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbService.java
public class UsbService extends IUsbManager.Stub {

    public UsbService(Context context) {
        mContext = context;

        mUserManager = context.getSystemService(UserManager.class);
        mSettingsManager = new UsbSettingsManager(context);
        mAlsaManager = new UsbAlsaManager(context);

        final PackageManager pm = mContext.getPackageManager();
        if (pm.hasSystemFeature(PackageManager.FEATURE_USB_HOST)) {
            mHostManager = new UsbHostManager(context, mAlsaManager, mSettingsManager);
        }
        if (new File("/sys/class/android_usb").exists()) {
            mDeviceManager = new UsbDeviceManager(context, mAlsaManager, mSettingsManager);
        }
        if (mHostManager != null || mDeviceManager != null) {
            mPortManager = new UsbPortManager(context);
        }

        onSwitchUser(UserHandle.USER_SYSTEM);

        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                final String action = intent.getAction();
                if (DevicePolicyManager.ACTION_DEVICE_POLICY_MANAGER_STATE_CHANGED.equals(action)) {
                    if (mDeviceManager != null) {
                        mDeviceManager.updateUserRestrictions();
                    }
                }
            }
        };

        final IntentFilter filter = new IntentFilter();
        filter.setPriority(IntentFilter.SYSTEM_HIGH_PRIORITY);
        filter.addAction(DevicePolicyManager.ACTION_DEVICE_POLICY_MANAGER_STATE_CHANGED);
        mContext.registerReceiver(receiver, filter, null, null);
    }

        /**
     * Set new {@link #mCurrentUserId} and propagate it to other modules.
     *
     * @param newUserId The user id of the new current user.
     */
    private void onSwitchUser(@UserIdInt int newUserId) {
        synchronized (mLock) {
            mCurrentUserId = newUserId;

            // The following two modules need to know about the current profile group. If they need
            // to distinguish(区别) by profile of the user, the id has to be passed in the call to the
            // module.
            UsbProfileGroupSettingsManager settings =  mSettingsManager.getSettingsForProfileGroup(UserHandle.of(newUserId));
            if (mHostManager != null) {
                mHostManager.setCurrentUserSettings(settings);
            }
            if (mDeviceManager != null) {
                mDeviceManager.setCurrentUser(newUserId, settings);
            }
        }
    }


}