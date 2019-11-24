//  @   frameworks/base/services/core/java/com/android/server/broadcastradio/hal2/RadioModule.java


class RadioModule {

    /**
     * loadModules
     * --> RadioModule.tryLoadingModule
     */
    public static @Nullable RadioModule tryLoadingModule(int idx, @NonNull String fqName) {
        try {
            IBroadcastRadio service = IBroadcastRadio.getService(fqName);
            if (service == null) return null;

            Mutable<AmFmRegionConfig> amfmConfig = new Mutable<>();
            service.getAmFmRegionConfig(false, (result, config) -> {
                if (result == Result.OK) amfmConfig.value = config;
            });

            Mutable<List<DabTableEntry>> dabConfig = new Mutable<>();
            service.getDabRegionConfig((result, config) -> {
                if (result == Result.OK) dabConfig.value = config;
            });

            RadioManager.ModuleProperties prop = Convert.propertiesFromHal(idx, fqName,service.getProperties(), amfmConfig.value, dabConfig.value);

            return new RadioModule(service, prop);
        } catch (RemoteException ex) {
            Slog.e(TAG, "failed to load module " + fqName, ex);
            return null;
        }
    }



    private RadioModule(@NonNull IBroadcastRadio service,@NonNull RadioManager.ModuleProperties properties) {
        mProperties = Objects.requireNonNull(properties);
        mService = Objects.requireNonNull(service);
    }

    public @NonNull TunerSession openSession(@NonNull android.hardware.radio.ITunerCallback userCb)
        throws RemoteException {

        TunerCallback cb = new TunerCallback(Objects.requireNonNull(userCb));
        Mutable<ITunerSession> hwSession = new Mutable<>();
        MutableInt halResult = new MutableInt(Result.UNKNOWN_ERROR);

        synchronized (mService) {
        mService.openSession(cb, (result, session) -> {
            hwSession.value = session;
            halResult.value = result;
        });
        }

        Convert.throwOnError("openSession", halResult.value);
        Objects.requireNonNull(hwSession.value);

        return new TunerSession(this, hwSession.value, cb);
    }
}
