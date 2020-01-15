
/**
 * @frameworks/av/media/libaudioprocessing/AudioResamplerDyn.cpp
 * 
 * resampler = new AudioResamplerDyn<float, float, float>(inChannelCount,sampleRate, quality);
*/
template<typename TC, typename TI, typename TO>
AudioResamplerDyn<TC, TI, TO>::AudioResamplerDyn( int inChannelCount, int32_t sampleRate, src_quality quality)
    : AudioResampler(inChannelCount, sampleRate, quality),
      mResampleFunc(0), mFilterSampleRate(0), mFilterQuality(DEFAULT_QUALITY),mCoefBuffer(NULL)
{
    mVolumeSimd[0] = mVolumeSimd[1] = 0;
    // The AudioResampler base class assumes we are always ready for 1:1 resampling.
    // We reset mInSampleRate to 0, so setSampleRate() will calculate filters for
    // setSampleRate() for 1:1. (May be removed if precalculated filters are used.)
    mInSampleRate = 0;
    mConstants.set(128, 8, mSampleRate, mSampleRate); // TODO: set better

    // fetch property based resampling parameters
    mPropertyEnableAtSampleRate = property_get_int32("ro.audio.resampler.psd.enable_at_samplerate", mPropertyEnableAtSampleRate);
    mPropertyHalfFilterLength = property_get_int32("ro.audio.resampler.psd.halflength", mPropertyHalfFilterLength);
    mPropertyStopbandAttenuation = property_get_int32("ro.audio.resampler.psd.stopband", mPropertyStopbandAttenuation);
    mPropertyCutoffPercent = property_get_int32("ro.audio.resampler.psd.cutoff_percent", mPropertyCutoffPercent);
}

template<typename TC, typename TI, typename TO>
void AudioResamplerDyn<TC, TI, TO>::init()
{
    mFilterSampleRate = 0; // always trigger new filter generation
    /**
     * 
    */
    mInBuffer.init();
}

template<typename TC, typename TI, typename TO>
void AudioResamplerDyn<TC, TI, TO>::InBuffer::init()
{
    free(mState);
    mState = NULL;
    mImpulse = NULL;
    mRingFull = NULL;
    mStateCount = 0;
}