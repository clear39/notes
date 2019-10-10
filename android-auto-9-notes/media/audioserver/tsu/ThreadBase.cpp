
//  @   frameworks/av/services/audioflinger/Threads.cpp

AudioFlinger::ThreadBase::ThreadBase(const sp<AudioFlinger>& audioFlinger, audio_io_handle_t id,
        audio_devices_t outDevice, audio_devices_t inDevice, type_t type, bool systemReady)
    :   Thread(false /*canCallJava*/),
        mType(type),
        mAudioFlinger(audioFlinger),
        // mSampleRate, mFrameCount, mChannelMask, mChannelCount, mFrameSize, mFormat, mBufferSize
        // are set by PlaybackThread::readOutputParameters_l() or
        // RecordThread::readInputParameters_l()
        //FIXME: mStandby should be true here. Is this some kind of hack?
        mStandby(false), mOutDevice(outDevice), mInDevice(inDevice),
        mPrevOutDevice(AUDIO_DEVICE_NONE), mPrevInDevice(AUDIO_DEVICE_NONE),
        mAudioSource(AUDIO_SOURCE_DEFAULT), mId(id),
        // mName will be set by concrete (non-virtual) subclass
        mDeathRecipient(new PMDeathRecipient(this)),
        mSystemReady(systemReady),
        mSignalPending(false)
{
    m_lpa_enable = property_get_int32("vendor.audio.lpa.enable", 0);
    memset(&mPatch, 0, sizeof(struct audio_patch));
}