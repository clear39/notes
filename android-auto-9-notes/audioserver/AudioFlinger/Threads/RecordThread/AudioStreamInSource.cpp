
//  @   frameworks/av/media/libnbaio/AudioStreamInSource.cpp

AudioStreamInSource::AudioStreamInSource(sp<StreamInHalInterface> stream) :
        NBAIO_Source(),
        mStream(stream),
        mStreamBufferSizeBytes(0),
        mFramesOverrun(0),
        mOverruns(0)
{
    ALOG_ASSERT(stream != 0);
}