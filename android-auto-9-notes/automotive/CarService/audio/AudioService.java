//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/services/core/java/com/android/server/audio/AudioService.java
public class AudioService extends IAudioService.Stub
        implements AccessibilityManager.TouchExplorationStateChangeListener,
            AccessibilityManager.AccessibilityServicesStateChangeListener {
            
    


    /***
     * 
     * @param policyConfig
     * @param pcb
     * @param hasFocusListener
     * @param isFocusPolicy
     * @param isVolumeController
     * @return
     */
    public String registerAudioPolicy(AudioPolicyConfig policyConfig, IAudioPolicyCallback pcb,
            boolean hasFocusListener, boolean isFocusPolicy, boolean isVolumeController) {

        /***
         * 
         */
        AudioSystem.setDynamicPolicyCallback(mDynPolicyCallback);

        String regId = null;
        // error handling
        boolean hasPermissionForPolicy =
                (PackageManager.PERMISSION_GRANTED == mContext.checkCallingPermission(android.Manifest.permission.MODIFY_AUDIO_ROUTING));


        if (!hasPermissionForPolicy) {
            Slog.w(TAG, "Can't register audio policy for pid " + Binder.getCallingPid() + " / uid " + Binder.getCallingUid() + ", need MODIFY_AUDIO_ROUTING");
            return null;
        }

        mDynPolicyLogger.log((new AudioEventLogger.StringEvent("registerAudioPolicy for " + pcb.asBinder() + " with config:" + policyConfig)).printLog(TAG));
        synchronized (mAudioPolicies) {
            try {
                /***
                 * 判断是否已经注册过
                 */
                if (mAudioPolicies.containsKey(pcb.asBinder())) {
                    Slog.e(TAG, "Cannot re-register policy");
                    return null;
                }
                AudioPolicyProxy app = new AudioPolicyProxy(policyConfig, pcb, hasFocusListener,isFocusPolicy, isVolumeController);
                pcb.asBinder().linkToDeath(app, 0/*flags*/);
                regId = app.getRegistrationId();
                mAudioPolicies.put(pcb.asBinder(), app);
            } catch (RemoteException e) {
                // audio policy owner has already died!
                Slog.w(TAG, "Audio policy registration failed, could not link to " + pcb +" binder death", e);
                return null;
            }
        }
        return regId;
    }




}

