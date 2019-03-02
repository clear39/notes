public class PhoneApp extends Application {
    PhoneGlobals mPhoneGlobals;
    TelephonyGlobals mTelephonyGlobals;

    public PhoneApp() {
    }

    @Override
    public void onCreate() {
        if (UserHandle.myUserId() == 0) {
            // We are running as the primary user, so should bring up the
            // global phone state.
            mPhoneGlobals = new PhoneGlobals(this);
            mPhoneGlobals.onCreate();

            mTelephonyGlobals = new TelephonyGlobals(this);
            mTelephonyGlobals.onCreate();
        }
    }
}