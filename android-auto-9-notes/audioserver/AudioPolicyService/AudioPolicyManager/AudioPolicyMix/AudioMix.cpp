/**
 *      @       frameworks/av/media/libaudioclient/include/media/AudioPolicy.h
*/

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

#define MIX_TYPE_INVALID (-1)
#define MIX_TYPE_PLAYERS 0
#define MIX_TYPE_RECORDERS 1


#define MIX_ROUTE_FLAG_RENDER 0x1
#define MIX_ROUTE_FLAG_LOOP_BACK (0x1 << 1)
#define MIX_ROUTE_FLAG_ALL (MIX_ROUTE_FLAG_RENDER | MIX_ROUTE_FLAG_LOOP_BACK)

class AudioMix {
public:
    // flag on an AudioMix indicating the activity on this mix (IDLE, MIXING)
    //   must be reported through the AudioPolicyClient interface
    static const uint32_t kCbFlagNotifyActivity = 0x1;

    AudioMix() {}
    AudioMix(Vector<AudioMixMatchCriterion> criteria, uint32_t mixType, audio_config_t format,uint32_t routeFlags, String8 registrationId, uint32_t flags) :
        mCriteria(criteria), mMixType(mixType), mFormat(format),mRouteFlags(routeFlags), mDeviceAddress(registrationId), mCbFlags(flags){

        }

    status_t readFromParcel(Parcel *parcel);
    status_t writeToParcel(Parcel *parcel) const;

    Vector<AudioMixMatchCriterion> mCriteria;
    uint32_t        mMixType;   //      MIX_TYPE_INVALID                MIX_TYPE_PLAYERS                MIX_TYPE_RECORDERS
    audio_config_t  mFormat;
    uint32_t        mRouteFlags;
    audio_devices_t mDeviceType;
    String8         mDeviceAddress;
    uint32_t        mCbFlags; // flags indicating which callbacks to use, see kCbFlag*
};


/**
 *   @   frameworks/av/media/libaudioclient/AudioPolicy.cpp
 */ 