

/***
 * @    /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/media/java/android/media/audiopolicy/AudioPolicy.java
 * 
 * AudioPolicy 为应用内部使用，只会传递到AudioManager
 * 
 * 
 */


public class AudioPolicy {
 
   

  



    /** 
     * @hide 
     * 
     * mPolicyCb 实在内部创建的静态接口类
     * 分析请看 IAudioPolicyCallback.java
     * 
     * 
     * */
    public IAudioPolicyCallback cb() { 
        return mPolicyCb; 
    }




}














public static class AudioPolicy.Builder {
     /**
     * Sets the {@link Looper} on which to run the event loop.
     * @param looper a non-null specific Looper.
     * @return the same Builder instance.
     * @throws IllegalArgumentException
     * 
     * 
     * CarAudioService.init --> CarAudioService.setupDynamicRouting --> CarAudioService.getDynamicAudioPolicy
     * 
     */
    @SystemApi
    public Builder setLooper(@NonNull Looper looper) throws IllegalArgumentException {
        if (looper == null) {
            throw new IllegalArgumentException("Illegal null Looper argument");
        }
        mLooper = looper;
        return this;
    }

      @SystemApi
    /**
     * Sets the callback to receive all volume key-related events.
     * The callback will only be called if the device is configured to handle volume events
     * in the PhoneWindowManager (see config_handleVolumeKeysInWindowManager)
     * @param vc
     * @return the same Builder instance.
     */
    public Builder setAudioPolicyVolumeCallback(@NonNull AudioPolicyVolumeCallback vc) {
        if (vc == null) {
            throw new IllegalArgumentException("Invalid null volume callback");
        }
        mVolCb = vc;
        return this;
    }







}