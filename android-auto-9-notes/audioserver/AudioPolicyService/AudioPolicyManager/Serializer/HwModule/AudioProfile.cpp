class AudioProfile : public virtual RefBase{

}

}

/***
 * 这里封装 mixPort中的子标签　profile
 * 
    <mixPort name="mixport_bus0_media_out" role="source" flags="AUDIO_OUTPUT_FLAG_PRIMARY">
        <profile name="" format="AUDIO_FORMAT_PCM_16_BIT" samplingRates="48000"  channelMasks="AUDIO_CHANNEL_OUT_STEREO"/>
    </mixPort>
 */ 
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioProfile.cpp
// audio_format_t @   system/media/audio/include/system/audio-base.h
AudioProfile::AudioProfile(audio_format_t format,
                const ChannelsVector &channelMasks,
                const SampleRateVector &samplingRateCollection) :
    mName(String8("")),
    mFormat(format),
    mChannelMasks(channelMasks),
    mSamplingRates(samplingRateCollection)
{}


void AudioProfile::setDynamicChannels(bool dynamic) { 
    mIsDynamicChannels = dynamic; 
}

bool AudioProfile::isDynamicChannels() const { 
    return mIsDynamicChannels; 
}

void AudioProfile::setDynamicRate(bool dynamic) { 
    mIsDynamicRate = dynamic; 
}

bool AudioProfile::isDynamicRate() const { 
    return mIsDynamicRate; 
}

void AudioProfile::setDynamicFormat(bool dynamic) { 
    mIsDynamicFormat = dynamic; 
}

bool AudioProfile::isDynamicFormat() const { 
    return mIsDynamicFormat; 
}

bool AudioProfile::isDynamic() { 
    return mIsDynamicFormat || mIsDynamicChannels || mIsDynamicRate; 
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
*/
void AudioProfileVector::dump(int fd, int spaces) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];

    snprintf(buffer, SIZE, "%*s- Profiles:\n", spaces, "");
    write(fd, buffer, strlen(buffer));
    for (size_t i = 0; i < size(); i++) {
        snprintf(buffer, SIZE, "%*sProfile %zu:", spaces + 4, "", i);
        write(fd, buffer, strlen(buffer));
        itemAt(i)->dump(fd, spaces + 8);
    }
}


/**
 * 这里在 AudioProfileVector dump中调用
 * dump分析
 * */
void AudioProfile::dump(int fd, int spaces) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, SIZE, "%s%s%s\n", mIsDynamicFormat ? "[dynamic format]" : "",
             mIsDynamicChannels ? "[dynamic channels]" : "",
             mIsDynamicRate ? "[dynamic rates]" : "");



    result.append(buffer);
    if (mName.length() != 0) {
        snprintf(buffer, SIZE, "%*s- name: %s\n", spaces, "", mName.string());
        result.append(buffer);
    }
    std::string formatLiteral;
    if (FormatConverter::toString(mFormat, formatLiteral)) {
        snprintf(buffer, SIZE, "%*s- format: %s\n", spaces, "", formatLiteral.c_str());
        result.append(buffer);
    }
    if (!mSamplingRates.isEmpty()) {
        snprintf(buffer, SIZE, "%*s- sampling rates:", spaces, "");
        result.append(buffer);
        for (size_t i = 0; i < mSamplingRates.size(); i++) {
            snprintf(buffer, SIZE, "%d", mSamplingRates[i]);
            result.append(buffer);
            result.append(i == (mSamplingRates.size() - 1) ? "" : ", ");
        }
        result.append("\n");
    }

    if (!mChannelMasks.isEmpty()) {
        snprintf(buffer, SIZE, "%*s- channel masks:", spaces, "");
        result.append(buffer);
        for (size_t i = 0; i < mChannelMasks.size(); i++) {
            snprintf(buffer, SIZE, "0x%04x", mChannelMasks[i]);
            result.append(buffer);
            result.append(i == (mChannelMasks.size() - 1) ? "" : ", ");
        }
        result.append("\n");
    }
    write(fd, result.string(), result.size());
}