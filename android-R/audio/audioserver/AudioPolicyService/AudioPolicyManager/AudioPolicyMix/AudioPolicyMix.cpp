
//	@ frameworks\av\services\audiopolicy\common\managerdefinitions\src\AudioPolicyMix.cpp

class AudioPolicyMix : public AudioMix, public RefBase {
public:
    AudioPolicyMix(const AudioMix &mix) : AudioMix(mix) {}
    AudioPolicyMix(const AudioPolicyMix&) = delete;
    AudioPolicyMix& operator=(const AudioPolicyMix&) = delete;

    const sp<SwAudioOutputDescriptor> &getOutput() const { return mOutput; }
    void setOutput(const sp<SwAudioOutputDescriptor> &output) { mOutput = output; }
    void clearOutput() { mOutput.clear(); }

    void dump(String8 *dst, int spaces, int index) const;

private:
    sp<SwAudioOutputDescriptor> mOutput;  // Corresponding output stream
};