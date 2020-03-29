
/**
 * @    frameworks/av/include/media/AudioBufferProvider.h
 * AudioBufferProvider 为一个抽象类
*/

class PassthruBufferProvider : public AudioBufferProvider {

}

class CopyBufferProvider : public PassthruBufferProvider {

}


class ReformatBufferProvider : public CopyBufferProvider {

}

/**
 * @    frameworks/av/media/libaudioprocessing/BufferProviders.cpp
*/

ReformatBufferProvider::ReformatBufferProvider(int32_t channelCount,audio_format_t inputFormat, audio_format_t outputFormat, size_t bufferFrameCount) :
        CopyBufferProvider(
                channelCount * audio_bytes_per_sample(inputFormat),
                channelCount * audio_bytes_per_sample(outputFormat),
                bufferFrameCount),
        mChannelCount(channelCount),
        mInputFormat(inputFormat),
        mOutputFormat(outputFormat)
{
    ALOGV("ReformatBufferProvider(%p)(%u, %#x, %#x)",this, channelCount, inputFormat, outputFormat);
}

CopyBufferProvider::CopyBufferProvider(size_t inputFrameSize,size_t outputFrameSize, size_t bufferFrameCount) :
        mInputFrameSize(inputFrameSize),
        mOutputFrameSize(outputFrameSize),
        mLocalBufferFrameCount(bufferFrameCount),
        mLocalBufferData(NULL),
        mConsumed(0)
{
    ALOGV("CopyBufferProvider(%p)(%zu, %zu, %zu)", this,  inputFrameSize, outputFrameSize, bufferFrameCount);
    LOG_ALWAYS_FATAL_IF(inputFrameSize < outputFrameSize && bufferFrameCount == 0, "Requires local buffer if inputFrameSize(%zu) < outputFrameSize(%zu)", inputFrameSize, outputFrameSize);
    if (mLocalBufferFrameCount) {
        (void)posix_memalign(&mLocalBufferData, 32, mLocalBufferFrameCount * mOutputFrameSize);
    }
    mBuffer.frameCount = 0;
}

virtual void PassthruBufferProvider::setBufferProvider(AudioBufferProvider *p) {
        /**
         * AudioBufferProvider *mTrackBufferProvider; 这里其实就AudioMixer::Track
        */
        mTrackBufferProvider = p;
}       