

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/services/core/java/com/android/server/audio/AudioService.java
public class AudioPolicyProxy extends AudioPolicyConfig implements IBinder.DeathRecipient {


    AudioPolicyProxy(AudioPolicyConfig config, IAudioPolicyCallback token,
        boolean hasFocusListener, boolean isFocusPolicy, boolean isVolumeController) {
        
        /**
         * mMixes = conf.mMixes;
         */
        super(config);

        setRegistration(new String(config.hashCode() + ":ap:" + mAudioPolicyCounter++));

        mPolicyCallback = token;
        mHasFocusListener = hasFocusListener;  // false
        mIsVolumeController = isVolumeController; // true

        if (mHasFocusListener) {  // false
            mMediaFocusControl.addFocusFollower(mPolicyCallback);
            // can only ever be true if there is a focus listener
            if (isFocusPolicy) {
                mIsFocusPolicy = true;
                mMediaFocusControl.setFocusPolicy(mPolicyCallback);
            }
        }

        if (mIsVolumeController) { // true
            setExtVolumeController(mPolicyCallback);
        }
        connectMixes();
    }

    protected void setRegistration(String regId) {
        final boolean currentRegNull = (mRegistrationId == null) || mRegistrationId.isEmpty();
        final boolean newRegNull = (regId == null) || regId.isEmpty();
        if (!currentRegNull && !newRegNull && !mRegistrationId.equals(regId)) {
            Log.e(TAG, "Invalid registration transition from " + mRegistrationId + " to " + regId);
            return;
        }
        mRegistrationId = regId == null ? "" : regId;
        for (AudioMix mix : mMixes) {
            setMixRegistration(mix);
        }
    }

    private void setMixRegistration(@NonNull final AudioMix mix) {
        if (!mRegistrationId.isEmpty()) {
            if ((mix.getRouteFlags() & AudioMix.ROUTE_FLAG_LOOP_BACK) == AudioMix.ROUTE_FLAG_LOOP_BACK) {
                mix.setRegistration(mRegistrationId + "mix" + mixTypeId(mix.getMixType()) + ":" + mMixCounter);
            } else if ((mix.getRouteFlags() & AudioMix.ROUTE_FLAG_RENDER) == AudioMix.ROUTE_FLAG_RENDER) {
                mix.setRegistration(mix.mDeviceAddress);
            }
        } else {
            mix.setRegistration("");
        }
        mMixCounter++;
    }


    private void setExtVolumeController(IAudioPolicyCallback apc) {
        if (!mContext.getResources().getBoolean(
                com.android.internal.R.bool.config_handleVolumeKeysInWindowManager)) {
            Log.e(TAG, "Cannot set external volume controller: device not set for volume keys" +
                    " handled in PhoneWindowManager");
            return;
        }
        synchronized (mExtVolumeControllerLock) {
            if (mExtVolumeController != null && !mExtVolumeController.asBinder().pingBinder()) {
                Log.e(TAG, "Cannot set external volume controller: existing controller");
            }
            mExtVolumeController = apc;
        }
    }


    void connectMixes() {
        final long identity = Binder.clearCallingIdentity();
        /***
         * 最终直接调到 registration 为 true 表示注册，否则为 注销 
         * status_t AudioPolicyService::registerPolicyMixes(const Vector<AudioMix>& mixes, bool registration)
         * --> status_t AudioPolicyManager::registerPolicyMixes(const Vector<AudioMix>& mixes)
         * 
         */
        AudioSystem.registerPolicyMixes(mMixes, true);
        Binder.restoreCallingIdentity(identity);
    }


}

