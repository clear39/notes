

public class BroadcastRadioService extends SystemService {

    @Override
    public ITuner openTuner(int moduleId, RadioManager.BandConfig bandConfig,
            boolean withAudio, ITunerCallback callback) throws RemoteException {

        if (DEBUG) Slog.i(TAG, "Opening module " + moduleId);
        enforcePolicyAccess();
        if (callback == null) {
            throw new IllegalArgumentException("Callback must not be empty");
        }
        synchronized (mLock) {
            if (mHal2.hasModule(moduleId)) {
                return mHal2.openSession(moduleId, bandConfig, withAudio, callback);
            } else {
                return mHal1.openTuner(moduleId, bandConfig, withAudio, callback);
            }
        }

    }

    private void enforcePolicyAccess() {
        if (PackageManager.PERMISSION_GRANTED != getContext().checkCallingPermission(Manifest.permission.ACCESS_BROADCAST_RADIO)) {
            throw new SecurityException("ACCESS_BROADCAST_RADIO permission not granted");
        }
    }

}


private class ServiceImpl extends IRadioService.Stub {

    @Override
    public List<RadioManager.ModuleProperties> listModules() {
        enforcePolicyAccess();
        synchronized (mLock) {
            if (mModules != null) return mModules;
            /***
             * private final com.android.server.broadcastradio.hal1.BroadcastRadioService mHal1 = new com.android.server.broadcastradio.hal1.BroadcastRadioService();
             */
            mModules = mHal1.loadModules();
             /***
             * private final com.android.server.broadcastradio.hal2.BroadcastRadioService mHal2 = new com.android.server.broadcastradio.hal2.BroadcastRadioService();
             */
            mModules.addAll(mHal2.loadModules(getNextId(mModules)));

            return mModules;
        }
    }


}