

public class AudioPolicyConfig implements Parcelable {

    protected final ArrayList<AudioMix> mMixes;

    protected int mDuckingPolicy = AudioPolicy.FOCUS_POLICY_DUCKING_IN_APP;

    private String mRegistrationId = null;
    // 在 java 层 AudioPolicy 类中 构建，将构建的 AudioPolicyConfig 传递给 AudioPolicy
    AudioPolicyConfig(ArrayList<AudioMix> mixes) {
        mMixes = mixes;
    }

    
}