public class UsbHostManager {
    /*
     * UsbHostManager
     */
    public UsbHostManager(Context context, UsbAlsaManager alsaManager,UsbSettingsManager settingsManager) {
        mContext = context;

        /**
         * 格式 /dev/bus/usb/001/
         * config_usbHostBlacklist 为空
         */
        mHostBlacklist = context.getResources().getStringArray(com.android.internal.R.array.config_usbHostBlacklist);
        mUsbAlsaManager = alsaManager;
        mSettingsManager = settingsManager;
        /**
         * config_UsbDeviceConnectionHandling_component 为空
         * 格式 "com.foo/.Blah" becomes package="com.foo" class="com.foo.Blah".
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

}