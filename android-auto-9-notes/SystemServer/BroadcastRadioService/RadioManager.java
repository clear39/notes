
//  @   frameworks/base/core/java/android/hardware/radio/RadioManager.java
public class RadioManager {

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