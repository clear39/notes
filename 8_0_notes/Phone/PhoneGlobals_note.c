public class PhoneGlobals extends ContextWrapper {


     public void onCreate() {
        if (VDBG) Log.v(LOG_TAG, "onCreate()...");

        ContentResolver resolver = getContentResolver();

        // Cache the "voice capable" flag.
        // This flag currently comes from a resource (which is
        // overrideable on a per-product basis):
        sVoiceCapable = getResources().getBoolean(com.android.internal.R.bool.config_voice_capable);
        // ...but this might eventually become a PackageManager "system
        // feature" instead, in which case we'd do something like:
        // sVoiceCapable =  getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY_VOICE_CALLS);

        if (mCM == null) {
            // Initialize the telephony framework
            PhoneFactory.makeDefaultPhones(this);

            // Start TelephonyDebugService After the default phone is created.
            Intent intent = new Intent(this, TelephonyDebugService.class);
            startService(intent);

            mCM = CallManager.getInstance();
            for (Phone phone : PhoneFactory.getPhones()) {
                mCM.registerPhone(phone);
            }

            // Create the NotificationMgr singleton, which is used to display
            // status bar icons and control other status bar behavior.
            notificationMgr = NotificationMgr.init(this);

            // If PhoneGlobals has crashed and is being restarted, then restart.
            mHandler.sendEmptyMessage(EVENT_RESTART_SIP);

            // Create an instance of CdmaPhoneCallState and initialize it to IDLE
            cdmaPhoneCallState = new CdmaPhoneCallState();
            cdmaPhoneCallState.CdmaPhoneCallStateInit();

            // before registering for phone state changes
            mPowerManager = (PowerManager) getSystemService(Context.POWER_SERVICE);
            mWakeLock = mPowerManager.newWakeLock(PowerManager.FULL_WAKE_LOCK, LOG_TAG);
            // lock used to keep the processor awake, when we don't care for the display.
            mPartialWakeLock = mPowerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK
                    | PowerManager.ON_AFTER_RELEASE, LOG_TAG);

            mKeyguardManager = (KeyguardManager) getSystemService(Context.KEYGUARD_SERVICE);

            // Get UpdateLock to suppress system-update related events (e.g. dialog show-up)
            // during phone calls.
            mUpdateLock = new UpdateLock("phone");

            if (DBG) Log.d(LOG_TAG, "onCreate: mUpdateLock: " + mUpdateLock);

            CallLogger callLogger = new CallLogger(this, new CallLogAsync());

            callGatewayManager = CallGatewayManager.getInstance();

            // Create the CallController singleton, which is the interface
            // to the telephony layer for user-initiated telephony functionality
            // (like making outgoing calls.)
            callController = CallController.init(this, callLogger, callGatewayManager);

            // Create the CallerInfoCache singleton, which remembers custom ring tone and
            // send-to-voicemail settings.
            //
            // The asynchronous caching will start just after this call.
            callerInfoCache = CallerInfoCache.init(this);

            phoneMgr = PhoneInterfaceManager.init(this, PhoneFactory.getDefaultPhone());

            configLoader = CarrierConfigLoader.init(this);

            // Create the CallNotifer singleton, which handles
            // asynchronous events from the telephony layer (like
            // launching the incoming-call UI when an incoming call comes
            // in.)
            notifier = CallNotifier.init(this);

            PhoneUtils.registerIccStatus(mHandler, EVENT_SIM_NETWORK_LOCKED);

            // register for MMI/USSD
            mCM.registerForMmiComplete(mHandler, MMI_COMPLETE, null);

            // register connection tracking to PhoneUtils
            PhoneUtils.initializeConnectionHandler(mCM);

            // Register for misc other intent broadcasts.
            IntentFilter intentFilter =
                    new IntentFilter(Intent.ACTION_AIRPLANE_MODE_CHANGED);
            intentFilter.addAction(TelephonyIntents.ACTION_ANY_DATA_CONNECTION_STATE_CHANGED);
            intentFilter.addAction(TelephonyIntents.ACTION_SIM_STATE_CHANGED);
            intentFilter.addAction(TelephonyIntents.ACTION_RADIO_TECHNOLOGY_CHANGED);
            intentFilter.addAction(TelephonyIntents.ACTION_SERVICE_STATE_CHANGED);
            intentFilter.addAction(TelephonyIntents.ACTION_EMERGENCY_CALLBACK_MODE_CHANGED);
            registerReceiver(mReceiver, intentFilter);

            mCarrierVvmPackageInstalledReceiver.register(this);

            //set the default values for the preferences in the phone.
            PreferenceManager.setDefaultValues(this, R.xml.network_setting_fragment, false);

            PreferenceManager.setDefaultValues(this, R.xml.call_feature_setting, false);

            // Make sure the audio mode (along with some
            // audio-mode-related state of our own) is initialized
            // correctly, given the current state of the phone.
            PhoneUtils.setAudioMode(mCM);
        }

        // XXX pre-load the SimProvider so that it's ready
        resolver.getType(Uri.parse("content://icc/adn"));

        // TODO: Register for Cdma Information Records
        // phone.registerCdmaInformationRecord(mHandler, EVENT_UNSOL_CDMA_INFO_RECORD, null);

        // Read HAC settings and configure audio hardware
        if (getResources().getBoolean(R.bool.hac_enabled)) {
            int hac = android.provider.Settings.System.getInt(
                    getContentResolver(),
                    android.provider.Settings.System.HEARING_AID,
                    0);
            AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            audioManager.setParameter(SettingsConstants.HAC_KEY,
                    hac == SettingsConstants.HAC_ENABLED
                            ? SettingsConstants.HAC_VAL_ON : SettingsConstants.HAC_VAL_OFF);
        }
    }
}
