/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPolicyMix.h
*/
class AudioPolicyMix : public RefBase {
public:
    AudioPolicyMix() {}

    const sp<SwAudioOutputDescriptor> &getOutput() const;

    void setOutput(sp<SwAudioOutputDescriptor> &output);

    void clearOutput();

    android::AudioMix *getMix();

    void setMix(AudioMix &mix);

    status_t dump(int fd, int spaces, int index) const;

private:
    AudioMix    mMix;                   // Audio policy mix descriptor
    sp<SwAudioOutputDescriptor> mOutput;  // Corresponding output stream
};

/**
 * @    frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyMix.cpp
*/
void AudioPolicyMix::setOutput(sp<SwAudioOutputDescriptor> &output)
{
    mOutput = output;
}

const sp<SwAudioOutputDescriptor> &AudioPolicyMix::getOutput() const
{
    return mOutput;
}

void AudioPolicyMix::setMix(AudioMix &mix)
{
    mMix = mix;
}

android::AudioMix *AudioPolicyMix::getMix()
{
    return &mMix;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyMix::dump(int fd, int spaces, int index) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%*sAudio Policy Mix %d:\n", spaces, "", index+1);
    result.append(buffer);
    std::string mixTypeLiteral;
    if (!MixTypeConverter::toString(mMix.mMixType, mixTypeLiteral)) {
        ALOGE("%s: failed to convert mix type %d", __FUNCTION__, mMix.mMixType);
        return BAD_VALUE;
    }
    snprintf(buffer, SIZE, "%*s- mix type: %s\n", spaces, "", mixTypeLiteral.c_str());
    result.append(buffer);
    std::string routeFlagLiteral;
    RouteFlagTypeConverter::maskToString(mMix.mRouteFlags, routeFlagLiteral);
    snprintf(buffer, SIZE, "%*s- Route Flags: %s\n", spaces, "", routeFlagLiteral.c_str());
    result.append(buffer);
    std::string deviceLiteral;
    deviceToString(mMix.mDeviceType, deviceLiteral);
    snprintf(buffer, SIZE, "%*s- device type: %s\n", spaces, "", deviceLiteral.c_str());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- device address: %s\n", spaces, "", mMix.mDeviceAddress.string());
    result.append(buffer);

    int indexCriterion = 0;
    for (const auto &criterion : mMix.mCriteria) {
        snprintf(buffer, SIZE, "%*s- Criterion %d:\n", spaces + 2, "", indexCriterion++);
        result.append(buffer);
        std::string usageLiteral;
        if (!UsageTypeConverter::toString(criterion.mValue.mUsage, usageLiteral)) {
            ALOGE("%s: failed to convert usage %d", __FUNCTION__, criterion.mValue.mUsage);
            return BAD_VALUE;
        }
        snprintf(buffer, SIZE, "%*s- Usage:%s\n", spaces + 4, "", usageLiteral.c_str());
        result.append(buffer);
        if (mMix.mMixType == MIX_TYPE_RECORDERS) {
            std::string sourceLiteral;
            if (!SourceTypeConverter::toString(criterion.mValue.mSource, sourceLiteral)) {
                ALOGE("%s: failed to convert source %d", __FUNCTION__, criterion.mValue.mSource);
                return BAD_VALUE;
            }
            snprintf(buffer, SIZE, "%*s- Source:%s\n", spaces + 4, "", sourceLiteral.c_str());
            result.append(buffer);
        }
        snprintf(buffer, SIZE, "%*s- Uid:%d\n", spaces + 4, "", criterion.mValue.mUid);
        result.append(buffer);
        std::string ruleLiteral;
        if (!RuleTypeConverter::toString(criterion.mRule, ruleLiteral)) {
            ALOGE("%s: failed to convert source %d", __FUNCTION__,criterion.mRule);
            return BAD_VALUE;
        }
        snprintf(buffer, SIZE, "%*s- Rule:%s\n", spaces + 4, "", ruleLiteral.c_str());
        result.append(buffer);
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}



