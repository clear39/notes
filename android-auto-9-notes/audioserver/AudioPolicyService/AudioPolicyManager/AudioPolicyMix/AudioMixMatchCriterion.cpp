

/**
 * /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libaudioclient/include/media/AudioPolicy.h
*/

// Keep in sync with AudioMix.java, AudioMixingRule.java, AudioPolicyConfig.java
#define RULE_EXCLUSION_MASK 0x8000

#define RULE_MATCH_ATTRIBUTE_USAGE           0x1
#define RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET (0x1 << 1)  //0x2
#define RULE_MATCH_UID                      (0x1 << 2)  //0x4

#define RULE_EXCLUDE_ATTRIBUTE_USAGE                     (RULE_EXCLUSION_MASK|RULE_MATCH_ATTRIBUTE_USAGE)       // 0x8001
#define RULE_EXCLUDE_ATTRIBUTE_CAPTURE_PRESET   (RULE_EXCLUSION_MASK|RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET) // 0x8002
#define RULE_EXCLUDE_UID              (RULE_EXCLUSION_MASK|RULE_MATCH_UID)      // 0x8004


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
    } mValue;
    uint32_t        mRule;
};