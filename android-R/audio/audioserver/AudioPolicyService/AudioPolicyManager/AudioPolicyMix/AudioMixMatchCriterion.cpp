// @ frameworks/av/media/libaudioclient/include/media/AudioPolicy.h


class AudioMixMatchCriterion {
public:
    AudioMixMatchCriterion() {}
    AudioMixMatchCriterion(audio_usage_t usage, audio_source_t source, uint32_t rule);

    status_t readFromParcel(Parcel *parcel);
    status_t writeToParcel(Parcel *parcel) const;

    union {
        audio_usage_t   mUsage;
        audio_source_t  mSource;
        uid_t           mUid;
        int        mUserId;
    } mValue;
    uint32_t        mRule;
};