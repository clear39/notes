
class NBAIO_Port : public RefBase {
    
}

class NBAIO_Sink : public NBAIO_Port {

}

class AudioStreamOutSink : public NBAIO_Sink {

}

/**
 * @    frameworks/av/media/libnbaio/AudioStreamOutSink.cpp
*/
AudioStreamOutSink::AudioStreamOutSink(sp<StreamOutHalInterface> stream) :
        NBAIO_Sink(),
        mStream(stream),
        mStreamBufferSizeBytes(0)
{
    ALOG_ASSERT(stream != 0);
}

//  @   frameworks/av/media/libnbaio/NBAIO.cpp
NBAIO_Port::NBAIO_Port(const NBAIO_Format& format) : mNegotiated(false), mFormat(format),
                                             mFrameSize(Format_frameSize(format)) { }

//  @   frameworks/av/media/libnbaio/NBAIO.cpp
NBAIO_Sink::NBAIO_Sink(const NBAIO_Format& format = Format_Invalid) : NBAIO_Port(format), mFramesWritten(0)
{

}




ssize_t AudioStreamOutSink::negotiate(const NBAIO_Format offers[], size_t numOffers,NBAIO_Format counterOffers[], size_t& numCounterOffers)
{
    if (!Format_isValid(mFormat)) {
        status_t result;
        result = mStream->getBufferSize(&mStreamBufferSizeBytes);
        if (result != OK) return result;
        audio_format_t streamFormat;
        uint32_t sampleRate;
        audio_channel_mask_t channelMask;
        result = mStream->getAudioProperties(&sampleRate, &channelMask, &streamFormat);
        if (result != OK) return result;
        mFormat = Format_from_SR_C(sampleRate,audio_channel_count_from_out_mask(channelMask), streamFormat);
        mFrameSize = Format_frameSize(mFormat);
    }
    return NBAIO_Sink::negotiate(offers, numOffers, counterOffers, numCounterOffers);
}

//  @   frameworks/av/media/libnbaio/NBAIO.cpp
bool Format_isValid(const NBAIO_Format& format)                                                                                                                                                                
{
    return format.mSampleRate != 0 && format.mChannelCount != 0 && format.mFormat != AUDIO_FORMAT_INVALID && format.mFrameSize != 0;
}

//  @   frameworks/av/media/libnbaio/NBAIO.cpp
// Default implementation that only accepts my mFormat
ssize_t NBAIO_Port::negotiate(const NBAIO_Format offers[], size_t numOffers,NBAIO_Format counterOffers[], size_t& numCounterOffers)
{
    ALOGV("negotiate offers=%p numOffers=%zu countersOffers=%p numCounterOffers=%zu",offers, numOffers, counterOffers, numCounterOffers);
    if (Format_isValid(mFormat)) {
        for (size_t i = 0; i < numOffers; ++i) {
            if (Format_isEqual(offers[i], mFormat)) {
                mNegotiated = true;
                return i;
            }
        }
        if (numCounterOffers > 0) {
            counterOffers[0] = mFormat;
        }
        numCounterOffers = 1;
    } else {
        numCounterOffers = 0;
    }
    return (ssize_t) NEGOTIATE;
}

//  @   frameworks/av/media/libnbaio/NBAIO.cpp
bool Format_isEqual(const NBAIO_Format& format1, const NBAIO_Format& format2)
{
    return format1.mSampleRate == format2.mSampleRate &&
            format1.mChannelCount == format2.mChannelCount && format1.mFormat == format2.mFormat &&
            format1.mFrameSize == format2.mFrameSize;
}



virtual NBAIO_Format NBAIO_Port::format() const { return mNegotiated ? mFormat : Format_Invalid; }