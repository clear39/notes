

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/broadcastradio/hal2/BroadcastRadioService.java
public class BroadcastRadioService {
    /**
     * 
     */
    public @NonNull Collection<RadioManager.ModuleProperties> loadModules(int idx) {
        Slog.v(TAG, "loadModules(" + idx + ")");

        for (String serviceName : listByInterface(IBroadcastRadio.kInterfaceName)) {
            Slog.v(TAG, "checking service: " + serviceName);

            RadioModule module = RadioModule.tryLoadingModule(idx, serviceName);
            if (module != null) {
                Slog.i(TAG, "loaded broadcast radio module " + idx + ": " + serviceName + " (HAL 2.0)");
                mModules.put(idx++, module);
            }
        }

        return mModules.values().stream().map(module -> module.mProperties).collect(Collectors.toList());
    }

    private static @NonNull List<String> listByInterface(@NonNull String fqName) {
        try {
            IServiceManager manager = IServiceManager.getService();
            if (manager == null) {
                Slog.e(TAG, "Failed to get HIDL Service Manager");
                return Collections.emptyList();
            }

            List<String> list = manager.listByInterface(fqName);
            if (list == null) {
                Slog.e(TAG, "Didn't get interface list from HIDL Service Manager");
                return Collections.emptyList();
            }
            return list;
        } catch (RemoteException ex) {
            Slog.e(TAG, "Failed fetching interface list", ex);
            return Collections.emptyList();
        }
    }

    public boolean hasModule(int id) {
        return mModules.containsKey(id);
    }

    public ITuner openSession(int moduleId, @Nullable RadioManager.BandConfig legacyConfig,
        boolean withAudio, @NonNull ITunerCallback callback) throws RemoteException {
        Objects.requireNonNull(callback);

        if (!withAudio) {
            throw new IllegalArgumentException("Non-audio sessions not supported with HAL 2.x");
        }

        RadioModule module = mModules.get(moduleId);
        if (module == null) {
            throw new IllegalArgumentException("Invalid module ID");
        }

        TunerSession session = module.openSession(callback);
        if (legacyConfig != null) {
            session.setConfiguration(legacyConfig);
        }
        return session;
    }

}