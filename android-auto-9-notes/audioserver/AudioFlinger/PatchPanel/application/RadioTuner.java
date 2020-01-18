//  apk路径： packages/apps/Car/Radio

public class RadioTunerExt {

    private boolean isSourceAvailableLocked(@NonNull String address) throws CarNotConnectedException {
        /**
         * 
         */
        String[] sources = mCarAudioManager.getExternalSources();
        return Stream.of(sources).anyMatch(source -> address.equals(source));
    }
   
    public boolean setMuted(boolean muted) {
       synchronized (mLock) {
            if (mCarAudioManager == null) {
                Log.i(TAG, "Car not connected yet, postponing operation: "   + (muted ? "mute" : "unmute"));
                mPendingMuteOperation = muted;
                return true;
            }        
                        
            // if it's already (not) muted - no need to (un)mute again
            if ((mAudioPatch == null) == muted) return true;
                
            try {    
               if (!muted) {
                  if (!isSourceAvailableLocked(HARDCODED_TUNER_ADDRESS)) {
                     Log.e(TAG, "Tuner source \"" + HARDCODED_TUNER_ADDRESS + "\" is not available");
                     return false;
                  }
                  Log.d(TAG, "Creating audio patch for " + HARDCODED_TUNER_ADDRESS);
                /**
                 * packages/apps/Car/Radio/src/com/android/car/radio/platform/RadioTunerExt.java:43:    
                 * private static final String HARDCODED_TUNER_ADDRESS = "tuner0";
                 */
                  mAudioPatch = mCarAudioManager.createAudioPatch(HARDCODED_TUNER_ADDRESS, AudioAttributes.USAGE_MEDIA, 0);
               } else {
                  Log.d(TAG, "Releasing audio patch");
                  mCarAudioManager.releaseAudioPatch(mAudioPatch);
                  mAudioPatch = null;
               }    
               return true;
            } catch (CarNotConnectedException e) {
               Log.e(TAG, "Can't (un)mute - car is not connected", e);
               return false;
            }        
        }            
    }           


}
