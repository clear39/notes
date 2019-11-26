

public class BroadcastRadioService extends SystemService {

    @Override
    public ITuner openTuner(int moduleId, RadioManager.BandConfig bandConfig,
            boolean withAudio, ITunerCallback callback) throws RemoteException {

        if (DEBUG) Slog.i(TAG, "Opening module " + moduleId);
        /**
         * 权限确认
         */
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

}


private class ServiceImpl extends IRadioService.Stub {

    /**
     * RadioManagerExt.initModules()
     * --> mRadioManager.listModules(mModules);
     */
    @Override
    public List<RadioManager.ModuleProperties> listModules() {
        enforcePolicyAccess();
        synchronized (mLock) {
            if (mModules != null) return mModules;

            mModules = mHal1.loadModules();
            mModules.addAll(mHal2.loadModules(getNextId(mModules)));

            return mModules;
        }
    }

}