

public class RadioManagerExt {
    public RadioManagerExt(@NonNull Context ctx) {
        mRadioManager = (RadioManager)ctx.getSystemService(Context.RADIO_SERVICE);
        Objects.requireNonNull(mRadioManager, "RadioManager could not be loaded");
        mRadioTunerExt = new RadioTunerExt(ctx);
        mCallbackHandlerThread.start();
    }

    public @Nullable RadioTuner openSession(RadioTuner.Callback callback, Handler handler) {
        Log.i(TAG, "Opening broadcast radio session...");

        initModules();
        if (mModules.size() == 0) return null;

        /* We won't need custom default wrapper when we push these proposed extensions to the
         * framework; this is solely to avoid deadlock on onConfigurationChanged callback versus
         * waitForInitialization.
         */
        Handler hwHandler = new Handler(mCallbackHandlerThread.getLooper());

        RadioManager.ModuleProperties module = mModules.get(HARDCODED_MODULE_INDEX);
        TunerCallbackAdapterExt cbExt = new TunerCallbackAdapterExt(callback, handler);

        RadioTuner tuner = mRadioManager.openTuner(
                module.getId(),
                null,  // BandConfig - let the service automatically select one.
                true,  // withAudio
                cbExt, hwHandler);
        mSessions.put(module.getId(), tuner);
        if (tuner == null) return null;
        RadioMetadataExt.setModuleId(module.getId());

        if (module.isInitializationRequired()) {
            if (!cbExt.waitForInitialization()) {
                Log.w(TAG, "Timed out waiting for tuner initialization");
                tuner.close();
                return null;
            }
        }

        return tuner;
    }

    private void initModules() {
        synchronized (mLock) {
            if (mModules != null) return;

            mModules = new ArrayList<>();
            int status = mRadioManager.listModules(mModules);
            if (status != RadioManager.STATUS_OK) {
                Log.w(TAG, "Couldn't get radio module list: " + status);
                return;
            }

            if (mModules.size() == 0) {
                Log.i(TAG, "No radio modules on this device");
                return;
            }

            //  private static final int HARDCODED_MODULE_INDEX = 0;
            RadioManager.ModuleProperties module = mModules.get(HARDCODED_MODULE_INDEX);
            mAmFmRegionConfig = reduceAmFmBands(module.getBands());
        }
    }

    
}