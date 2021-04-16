
/*
frameworks/base/services/core/java/com/android/server/audio/AudioService.java:922:        
mDeviceBroker = new AudioDeviceBroker(mContext, this);
*/

/*package*/ public final class AudioDeviceBroker {

	/*package*/ AudioDeviceBroker(@NonNull Context context, @NonNull AudioService service) {
        mContext = context;
        mAudioService = service;
        mBtHelper = new BtHelper(this);
        // 
        mDeviceInventory = new AudioDeviceInventory(this);
        //主要封装来个广播接口
        mSystemServer = SystemServerAdapter.getDefaultAdapter(mContext);

        init();
    }

    private void init() {
        setupMessaging(mContext);

        mForcedUseForComm = AudioSystem.FORCE_NONE;
        mForcedUseForCommExt = mForcedUseForComm;
    }

    private void setupMessaging(Context ctxt) {
        final PowerManager pm = (PowerManager) ctxt.getSystemService(Context.POWER_SERVICE);
        mBrokerEventWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                "handleAudioDeviceEvent");
        mBrokerThread = new BrokerThread();
        mBrokerThread.start();
        waitForBrokerHandlerCreation();
    }

    private void waitForBrokerHandlerCreation() {
        synchronized (this) {
            while (mBrokerHandler == null) {
                try {
                    wait();
                } catch (InterruptedException e) {
                    Log.e(TAG, "Interruption while waiting on BrokerHandler");
                }
            }
        }
    }


    /*package*/ void onSystemReady() {
        synchronized (mSetModeLock) {
            synchronized (mDeviceStateLock) {
                mModeOwnerPid = mAudioService.getModeOwnerPid();
                mBtHelper.onSystemReady();
            }
        }
    }



}