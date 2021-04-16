
// @ frameworks/base/media/java/android/media/audiopolicy/AudioPolicy.java
public class AudioPolicy {

	private AudioPolicy(AudioPolicyConfig config, Context context, Looper looper,
            AudioPolicyFocusListener fl, AudioPolicyStatusListener sl,
            boolean isFocusPolicy, boolean isTestFocusPolicy,
            AudioPolicyVolumeCallback vc, @Nullable MediaProjection projection) {
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
        mFocusListener = fl;
        mStatusListener = sl;
        mIsFocusPolicy = isFocusPolicy;
        mIsTestFocusPolicy = isTestFocusPolicy;
        mVolCb = vc;
        mProjection = projection;
    }

	public static class Builder {
		private ArrayList<AudioMix> mMixes;
        private Context mContext;
        private Looper mLooper;
        private AudioPolicyFocusListener mFocusListener;
        private AudioPolicyStatusListener mStatusListener;
        private boolean mIsFocusPolicy = false;
        private boolean mIsTestFocusPolicy = false;
        private AudioPolicyVolumeCallback mVolCb;
        private MediaProjection mProjection;


	}

}