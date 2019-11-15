

//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbDebuggingManager.java
public class UsbDebuggingManager {

    public UsbDebuggingManager(Context context) {
        mHandler = new UsbDebuggingHandler(FgThread.get().getLooper());
        mContext = context;
    }


    public void setAdbEnabled(boolean enabled) {
        /***
         * 这里会启动 UsbDebuggingThread 线程
         */
        mHandler.sendEmptyMessage(enabled ? UsbDebuggingHandler.MESSAGE_ADB_ENABLED : UsbDebuggingHandler.MESSAGE_ADB_DISABLED);
    }


    private void startConfirmation(String key, String fingerprints) {
        int currentUserId = ActivityManager.getCurrentUser();
        UserInfo userInfo = UserManager.get(mContext).getUserInfo(currentUserId);
        String componentString;
        if (userInfo.isAdmin()) {
            componentString = Resources.getSystem().getString(com.android.internal.R.string.config_customAdbPublicKeyConfirmationComponent);
        } else {
            // If the current foreground user is not the admin user we send a different
            // notification specific to secondary users.
            componentString = Resources.getSystem().getString(R.string.config_customAdbPublicKeyConfirmationSecondaryUserComponent);
        }
        ComponentName componentName = ComponentName.unflattenFromString(componentString);
        if (startConfirmationActivity(componentName, userInfo.getUserHandle(), key, fingerprints)
                || startConfirmationService(componentName, userInfo.getUserHandle(),key, fingerprints)) {
            return;
        }
        Slog.e(TAG, "unable to start customAdbPublicKeyConfirmation[SecondaryUser]Component " + componentString + " as an Activity or a Service");
    }

}
