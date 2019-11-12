
//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbSettingsManager.java


class UsbSettingsManager {

    public UsbSettingsManager(@NonNull Context context) {
        mContext = context;
        mUserManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
    }
    
}