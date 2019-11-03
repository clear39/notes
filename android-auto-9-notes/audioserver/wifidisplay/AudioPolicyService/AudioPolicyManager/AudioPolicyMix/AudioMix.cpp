

//  @   frameworks/av/media/libaudioclient/include/media/AudioPolicy.h
class AudioMix {
public:
    // flag on an AudioMix indicating the activity on this mix (IDLE, MIXING)
    //   must be reported through the AudioPolicyClient interface
    static const uint32_t kCbFlagNotifyActivity = 0x1;

    AudioMix() {}
    AudioMix(Vector<AudioMixMatchCriterion> criteria, uint32_t mixType, audio_config_t format,
             uint32_t routeFlags, String8 registrationId, uint32_t flags) :
        mCriteria(criteria), mMixType(mixType), mFormat(format),
        mRouteFlags(routeFlags), mDeviceAddress(registrationId), mCbFlags(flags){}

    status_t readFromParcel(Parcel *parcel);
    status_t writeToParcel(Parcel *parcel) const;

    /**
     * 
     * AudioMixMatchCriterion @ frameworks/av/media/libaudioclient/include/media/AudioPolicy.h
    */
    Vector<AudioMixMatchCriterion> mCriteria;
    uint32_t        mMixType;
    audio_config_t  mFormat;
    uint32_t        mRouteFlags;
    audio_devices_t mDeviceType;
    String8         mDeviceAddress;
    uint32_t        mCbFlags; // flags indicating which callbacks to use, see kCbFlag*
};


class AudioMixMatchCriterion {
public:
    AudioMixMatchCriterion() {}
    AudioMixMatchCriterion(audio_usage_t usage, audio_source_t source, uint32_t rule);

    status_t readFromParcel(Parcel *parcel);
    status_t writeToParcel(Parcel *parcel) const;

    union {
        audio_usage_t   mUsage;     //  @   system/media/audio/include/system/audio-base.h:407
        audio_source_t  mSource;       //   @   system/media/audio/include/system/audio-base.h:62
        uid_t           mUid;
    } mValue;
    uint32_t        mRule;
};


//  @   frameworks/av/media/libaudioclient/AudioPolicy.cpp













//  
AudioMixMatchCriterion

