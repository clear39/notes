
// @ frameworks/base/services/core/java/com/android/server/audio/AudioService.java
public class AudioPolicyProxy extends AudioPolicyConfig implements IBinder.DeathRecipient {

	AudioPolicyProxy(AudioPolicyConfig config, IAudioPolicyCallback token,
            boolean hasFocusListener, boolean isFocusPolicy, boolean isTestFocusPolicy,
            boolean isVolumeController, IMediaProjection projection) {
        super(config);

        setRegistration(new String(config.hashCode() + ":ap:" + mAudioPolicyCounter++));
        mPolicyCallback = token;
        mHasFocusListener = hasFocusListener;
        mIsVolumeController = isVolumeController;
        mProjection = projection;
        if (mHasFocusListener) {
            mMediaFocusControl.addFocusFollower(mPolicyCallback);
            // can only ever be true if there is a focus listener
            if (isFocusPolicy) {
                mIsFocusPolicy = true;
                mIsTestFocusPolicy = isTestFocusPolicy;
                mMediaFocusControl.setFocusPolicy(mPolicyCallback, mIsTestFocusPolicy);
            }
        }
        if (mIsVolumeController) {
            setExtVolumeController(mPolicyCallback);
        }
        if (mProjection != null) {
            mProjectionCallback = new UnregisterOnStopCallback();
            try {
                mProjection.registerCallback(mProjectionCallback);
            } catch (RemoteException e) {
                release();
                throw new IllegalStateException("MediaProjection callback registration failed, "
                        + "could not link to " + projection + " binder death", e);
            }
        }
        int status = connectMixes();
        if (status != AudioSystem.SUCCESS) {
            release();
            throw new IllegalStateException("Could not connect mix, error: " + status);
        }
    }

	@AudioSystem.AudioSystemError 
	int connectMixes() {
        final long identity = Binder.clearCallingIdentity();
        int status = AudioSystem.registerPolicyMixes(mMixes, true);
        Binder.restoreCallingIdentity(identity);
        return status;
    }

    void release() {
        if (mIsFocusPolicy) {
            mMediaFocusControl.unsetFocusPolicy(mPolicyCallback, mIsTestFocusPolicy);
        }
        if (mFocusDuckBehavior == AudioPolicy.FOCUS_POLICY_DUCKING_IN_POLICY) {
            mMediaFocusControl.setDuckingInExtPolicyAvailable(false);
        }
        if (mHasFocusListener) {
            mMediaFocusControl.removeFocusFollower(mPolicyCallback);
        }
        if (mProjectionCallback != null) {
            try {
                mProjection.unregisterCallback(mProjectionCallback);
            } catch (RemoteException e) {
                Log.e(TAG, "Fail to unregister Audiopolicy callback from MediaProjection");
            }
        }
        if (mIsVolumeController) {
            synchronized (mExtVolumeControllerLock) {
                mExtVolumeController = null;
            }
        }
        final long identity = Binder.clearCallingIdentity();
        AudioSystem.registerPolicyMixes(mMixes, false);
        Binder.restoreCallingIdentity(identity);
        synchronized (mAudioPolicies) {
            mAudioPolicies.remove(mPolicyCallback.asBinder());
        }
        try {
            mPolicyCallback.notifyUnregistration();
        } catch (RemoteException e) { }
    }

}