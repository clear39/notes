


//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/media/java/android/media/audiopolicy/AudioPolicy.java

 private final IAudioPolicyCallback mPolicyCb = new IAudioPolicyCallback.Stub() {

        public void notifyAudioFocusGrant(AudioFocusInfo afi, int requestResult) {
            sendMsg(MSG_FOCUS_GRANT, afi, requestResult);
            if (DEBUG) {
                Log.v(TAG, "notifyAudioFocusGrant: pack=" + afi.getPackageName() + " client=" + afi.getClientId() + "reqRes=" + requestResult);
            }
        }

        public void notifyAudioFocusLoss(AudioFocusInfo afi, boolean wasNotified) {
            sendMsg(MSG_FOCUS_LOSS, afi, wasNotified ? 1 : 0);
            if (DEBUG) {
                Log.v(TAG, "notifyAudioFocusLoss: pack=" + afi.getPackageName() + " client="  + afi.getClientId() + "wasNotified=" + wasNotified);
            }
        }

        public void notifyAudioFocusRequest(AudioFocusInfo afi, int requestResult) {
            sendMsg(MSG_FOCUS_REQUEST, afi, requestResult);
            if (DEBUG) {
                Log.v(TAG, "notifyAudioFocusRequest: pack=" + afi.getPackageName() + " client=" + afi.getClientId() + " gen=" + afi.getGen());
            }
        }

        public void notifyAudioFocusAbandon(AudioFocusInfo afi) {
            sendMsg(MSG_FOCUS_ABANDON, afi, 0 /* ignored */);
            if (DEBUG) {
                Log.v(TAG, "notifyAudioFocusAbandon: pack=" + afi.getPackageName() + " client=" + afi.getClientId());
            }
        }

        public void notifyMixStateUpdate(String regId, int state) {
            for (AudioMix mix : mConfig.getMixes()) {
                if (mix.getRegistration().equals(regId)) {
                    mix.mMixState = state;
                    sendMsg(MSG_MIX_STATE_UPDATE, mix, 0/*ignored*/);
                    if (DEBUG) {
                        Log.v(TAG, "notifyMixStateUpdate: regId=" + regId + " state=" + state);
                    }
                }
            }
        }

        public void notifyVolumeAdjust(int adjustment) {
            sendMsg(MSG_VOL_ADJUST, null /* ignored */, adjustment);
            if (DEBUG) {
                Log.v(TAG, "notifyVolumeAdjust: " + adjustment);
            }
        }
    };