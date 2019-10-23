

/***
 * @    /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/media/java/android/media/audiopolicy/AudioPolicy.java
 * 
 * AudioPolicy 为应用内部使用，只会传递到AudioManager
 * 
 * 
 */


public class AudioPolicy {

    /**
    * The parameter is guaranteed non-null through the Builder
    * 
    * AudioPolicy(new AudioPolicyConfig(mMixes), mContext, mLooper,null, null, false, mVolCb)
    */
   private AudioPolicy(AudioPolicyConfig config, Context context, Looper looper,
           AudioPolicyFocusListener fl, AudioPolicyStatusListener sl, boolean isFocusPolicy,
           AudioPolicyVolumeCallback vc) {
       mConfig = config;
       mStatus = POLICY_STATUS_UNREGISTERED;
       mContext = context;
       if (looper == null) {
           looper = Looper.getMainLooper();
       }
       if (looper != null) {
           mEventHandler = new EventHandler(this, looper);
       } else {
           mEventHandler = null;
           Log.e(TAG, "No event handler due to looper without a thread");
       }
       mFocusListener = fl; // null
       mStatusListener = sl;// null
       mIsFocusPolicy = isFocusPolicy; //false
       mVolCb = vc;
   }


    /** 
     * @hide 
     * 
     * mPolicyCb 实在内部创建的静态接口类
     * 分析请看 IAudioPolicyCallback.java
     * private final IAudioPolicyCallback mPolicyCb = new IAudioPolicyCallback.Stub() {}
     * 
     * */
    public IAudioPolicyCallback cb() {
        return mPolicyCb; 
    }

    public boolean hasFocusListener() { return mFocusListener != null; }


    public boolean isVolumeController() { return mVolCb != null; }

}



public static class AudioPolicy.Builder {

    /**
     * Constructs a new Builder with no audio mixes.
     * @param context the context for the policy
     */
    @SystemApi
    public Builder(Context context) {
        mMixes = new ArrayList<AudioMix>();
        mContext = context;
    }

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



    public Builder addMix(@NonNull AudioMix mix) throws IllegalArgumentException {
        if (mix == null) {
            throw new IllegalArgumentException("Illegal null AudioMix argument");
        }
        mMixes.add(mix);
        return this;
    }

    /**
     * Combines all of the attributes that have been set on this {@code Builder} and returns a
     * new {@link AudioPolicy} object.
     * @return a new {@code AudioPolicy} object.
     * @throws IllegalStateException if there is no
     *     {@link AudioPolicy.AudioPolicyStatusListener} but the policy was configured
     *     as an audio focus policy with {@link #setIsAudioFocusPolicy(boolean)}.
     */
    @SystemApi
    public AudioPolicy build() {
        /***
         * 这里 mStatusListener = null
         */
        if (mStatusListener != null) {
            // the AudioPolicy status listener includes updates on each mix activity state
            for (AudioMix mix : mMixes) {
                mix.mCallbackFlags |= AudioMix.CALLBACK_FLAG_NOTIFY_ACTIVITY;
            }
        }
        /**
         * mIsFocusPolicy = false
         * mFocusListener = null
         */
        if (mIsFocusPolicy && mFocusListener == null) {
            throw new IllegalStateException("Cannot be a focus policy without " + "an AudioPolicyFocusListener");
        }
        /***
         * AudioPolicy(new AudioPolicyConfig(mMixes), mContext, mLooper,null, null, false, mVolCb)
         */
        return new AudioPolicy(new AudioPolicyConfig(mMixes), mContext, mLooper,
                mFocusListener, mStatusListener, mIsFocusPolicy, mVolCb);
    }



}