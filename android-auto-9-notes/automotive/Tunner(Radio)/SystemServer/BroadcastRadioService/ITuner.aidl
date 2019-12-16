/** {@hide} */
interface ITuner {
    void close();

    boolean isClosed();

    /**
     * @throws IllegalArgumentException if config is not valid or null
     */
    void setConfiguration(in RadioManager.BandConfig config);

    RadioManager.BandConfig getConfiguration();

    /**
     * @throws IllegalStateException if tuner was opened without audio
     */
    void setMuted(boolean mute);

    boolean isMuted();

    /**
     * @throws IllegalStateException if called out of sequence
     */
    void step(boolean directionDown, boolean skipSubChannel);

    /**
     * @throws IllegalStateException if called out of sequence
     */
    void scan(boolean directionDown, boolean skipSubChannel);

    /**
     * @throws IllegalArgumentException if invalid arguments are passed
     * @throws IllegalStateException if called out of sequence
     */
    void tune(in ProgramSelector selector);

    /**
     * @throws IllegalStateException if called out of sequence
     */
    void cancel();

    void cancelAnnouncement();

    Bitmap getImage(int id);

    /**
     * @return {@code true} if the scan was properly scheduled,
     *          {@code false} if the scan feature is unavailable
     */
    boolean startBackgroundScan();

    void startProgramListUpdates(in ProgramList.Filter filter);
    void stopProgramListUpdates();

    boolean isConfigFlagSupported(int flag);
    boolean isConfigFlagSet(int flag);
    void setConfigFlag(int flag, boolean value);

    /**
     * @param parameters Vendor-specific key-value pairs, must be Map<String, String>
     * @return Vendor-specific key-value pairs, must be Map<String, String>
     */
    Map setParameters(in Map parameters);

    /**
     * @param keys Parameter keys to fetch
     * @return Vendor-specific key-value pairs, must be Map<String, String>
     */
    Map getParameters(in List<String> keys);
}