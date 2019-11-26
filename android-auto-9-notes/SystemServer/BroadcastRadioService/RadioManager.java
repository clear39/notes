
//  @   frameworks/base/core/java/android/hardware/radio/RadioManager.java
public class RadioManager {


     /**
     * Returns a list of descriptors for all broadcast radio modules present on the device.
     * @param modules An List of {@link ModuleProperties} where the list will be returned.
     * @return
     * <ul>
     *  <li>{@link #STATUS_OK} in case of success, </li>
     *  <li>{@link #STATUS_ERROR} in case of unspecified error, </li>
     *  <li>{@link #STATUS_NO_INIT} if the native service cannot be reached, </li>
     *  <li>{@link #STATUS_BAD_VALUE} if modules is null, </li>
     *  <li>{@link #STATUS_DEAD_OBJECT} if the binder transaction to the native service fails, </li>
     * </ul>
     * 
     * 
     * RadioManagerExt.initModules()
     * --> mRadioManager.listModules(mModules);
     */
    @RequiresPermission(Manifest.permission.ACCESS_BROADCAST_RADIO)
    public int listModules(List<ModuleProperties> modules) {
        if (modules == null) {
            Log.e(TAG, "the output list must not be empty");
            return STATUS_BAD_VALUE;
        }

        Log.d(TAG, "Listing available tuners...");
        List<ModuleProperties> returnedList;
        try {
            returnedList = mService.listModules();
        } catch (RemoteException e) {
            Log.e(TAG, "Failed listing available tuners", e);
            return STATUS_DEAD_OBJECT;
        }

        if (returnedList == null) {
            Log.e(TAG, "Returned list was a null");
            return STATUS_ERROR;
        }

        modules.addAll(returnedList);
        return STATUS_OK;
    }


    public RadioTuner openTuner(int moduleId, BandConfig config, boolean withAudio,RadioTuner.Callback callback, Handler handler) {
        if (callback == null) {
            throw new IllegalArgumentException("callback must not be empty");
        }

        Log.d(TAG, "Opening tuner " + moduleId + "...");

        ITuner tuner;
        TunerCallbackAdapter halCallback = new TunerCallbackAdapter(callback, handler);
        try {
            tuner = mService.openTuner(moduleId, config, withAudio, halCallback);
        } catch (RemoteException | IllegalArgumentException ex) {
            Log.e(TAG, "Failed to open tuner", ex);
            return null;
        }
        if (tuner == null) {
            Log.e(TAG, "Failed to open tuner");
            return null;
        }
        return new TunerAdapter(tuner, halCallback,config != null ? config.getType() : BAND_INVALID);
    }

}