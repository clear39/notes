//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/java/android/hardware/radio/ITunerCallback.aidl

/** {@hide} */
oneway interface ITunerCallback {
    void onError(int status);
    void onTuneFailed(int result, in ProgramSelector selector);
    void onConfigurationChanged(in RadioManager.BandConfig config);
    void onCurrentProgramInfoChanged(in RadioManager.ProgramInfo info);
    void onTrafficAnnouncement(boolean active);
    void onEmergencyAnnouncement(boolean active);
    void onAntennaState(boolean connected);
    void onBackgroundScanAvailabilityChange(boolean isAvailable);
    void onBackgroundScanComplete();
    void onProgramListChanged();
    void onProgramListUpdated(in ProgramList.Chunk chunk);

    /**
     * @param parameters Vendor-specific key-value pairs, must be Map<String, String>
     */
    void onParametersUpdated(in Map parameters);
}
