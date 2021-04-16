//	@	frameworks/av/media/libaudioclient/AudioPolicy.cpp
class AudioMix {

	mutable Vector<AudioMixMatchCriterion> mCriteria;
    uint32_t        mMixType;
    audio_config_t  mFormat;
    uint32_t        mRouteFlags;
    audio_devices_t mDeviceType;
    String8         mDeviceAddress;
    uint32_t        mCbFlags; // flags indicating which callbacks to use, see kCbFlag*
    /** Ignore the AUDIO_FLAG_NO_MEDIA_PROJECTION */
    bool            mAllowPrivilegedPlaybackCapture = false;
    /** Indicates if the caller can capture voice communication output */
    bool            mVoiceCommunicationCaptureAllowed = false;

}


























/////////////////////////////////////////////////////////////////////////////////////////////
void AudioPolicyMix::dump(String8 *dst, int spaces, int index) const
{
    dst->appendFormat("%*sAudio Policy Mix %d (%p):\n", spaces, "", index + 1, this);
    std::string mixTypeLiteral;
    if (!MixTypeConverter::toString(mMixType, mixTypeLiteral)) {
        ALOGE("%s: failed to convert mix type %d", __FUNCTION__, mMixType);
        return;
    }
    dst->appendFormat("%*s- mix type: %s\n", spaces, "", mixTypeLiteral.c_str());

    std::string routeFlagLiteral;
    RouteFlagTypeConverter::maskToString(mRouteFlags, routeFlagLiteral);
    dst->appendFormat("%*s- Route Flags: %s\n", spaces, "", routeFlagLiteral.c_str());

    dst->appendFormat("%*s- device type: %s\n", spaces, "", toString(mDeviceType).c_str());

    dst->appendFormat("%*s- device address: %s\n", spaces, "", mDeviceAddress.string());

    dst->appendFormat("%*s- output: %d\n", spaces, "",
            mOutput == nullptr ? 0 : mOutput->mIoHandle);

    int indexCriterion = 0;
    for (const auto &criterion : mCriteria) {
        dst->appendFormat("%*s- Criterion %d: ", spaces + 2, "", indexCriterion++);

        std::string ruleType, ruleValue;
        bool unknownRule = !RuleTypeConverter::toString(criterion.mRule, ruleType);
        switch (criterion.mRule & ~RULE_EXCLUSION_MASK) { // no need to match RULE_EXCLUDE_...
        case RULE_MATCH_ATTRIBUTE_USAGE:
            UsageTypeConverter::toString(criterion.mValue.mUsage, ruleValue);
            break;
        case RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET:
            SourceTypeConverter::toString(criterion.mValue.mSource, ruleValue);
            break;
        case RULE_MATCH_UID:
            ruleValue = std::to_string(criterion.mValue.mUid);
            break;
        case RULE_MATCH_USERID:
            ruleValue = std::to_string(criterion.mValue.mUserId);
            break;
        default:
            unknownRule = true;
        }

        if (!unknownRule) {
            dst->appendFormat("%s %s\n", ruleType.c_str(), ruleValue.c_str());
        } else {
            dst->appendFormat("Unknown rule type value 0x%x\n", criterion.mRule);
        }
    }
}