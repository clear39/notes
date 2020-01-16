
/**
 * @frameworks/av/media/libaudioprocessing/AudioResamplerDyn.cpp
 * 
 * 
 * -----> bool AudioMixer::Track::setResampler(uint32_t trackSampleRate, uint32_t devSampleRate)
 * -------->mResampler.reset(AudioResampler::create(mMixerInFormat,resamplerChannelCount,devSampleRate, quality));
 * ----------->AudioResampler* AudioResampler::create(audio_format_t format, int inChannelCount,int32_t sampleRate, src_quality quality) 
 * ----------------> resampler = new AudioResamplerDyn<float, float, float>(inChannelCount,sampleRate, quality);
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
    mConstants.set(128, 8, mSampleRate, mSampleRate); // mSampleRate = 48000 TODO: set better

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


/**
 * 
*/
template<typename TC, typename TI, typename TO>
void AudioResamplerDyn<TC, TI, TO>::setVolume(float left, float right)
{
    AudioResampler::setVolume(left, right);
    if (is_same<TO, float>::value || is_same<TO, double>::value) {
        mVolumeSimd[0] = static_cast<TO>(left);
        mVolumeSimd[1] = static_cast<TO>(right);
    } else {  // integer requires scaling to U4_28 (rounding down)
        // integer volumes are clamped to 0 to UNITY_GAIN so there
        // are no issues with signed overflow.
        mVolumeSimd[0] = u4_28_from_float(clampFloatVol(left));
        mVolumeSimd[1] = u4_28_from_float(clampFloatVol(right));
    }
}




/**
 * 
*/
template<typename TC, typename TI, typename TO>
void AudioResamplerDyn<TC, TI, TO>::setSampleRate(int32_t inSampleRate)
{
    if (mInSampleRate == inSampleRate) {
        return;
    }
    int32_t oldSampleRate = mInSampleRate;
    /**
     * 
    */
    uint32_t oldPhaseWrapLimit = mConstants.mL << mConstants.mShift;  // 128 << 23
    bool useS32 = false;

    mInSampleRate = inSampleRate; // 44100

    // TODO: Add precalculated Equiripple filters

    if (mFilterQuality != getQuality() ||  !isClose(inSampleRate, oldSampleRate, mFilterSampleRate, mSampleRate)) {
        mFilterSampleRate = inSampleRate;
        mFilterQuality = getQuality();

        double stopBandAtten;
        double tbwCheat = 1.; // how much we "cheat" into aliasing
        int halfLength;
        double fcr = 0.;

        // Begin Kaiser Filter computation
        //
        // The quantization floor for S16 is about 96db - 10*log_10(#length) + 3dB.
        // Keep the stop band attenuation no greater than 84-85dB for 32 length S16 filters
        //
        // For s32 we keep the stop band attenuation at the same as 16b resolution, about
        // 96-98dB
        //

        if (mPropertyEnableAtSampleRate >= 0 && mSampleRate >= mPropertyEnableAtSampleRate) {
            // An alternative method which allows allows a greater fcr
            // at the expense of potential aliasing.
            halfLength = mPropertyHalfFilterLength;
            stopBandAtten = mPropertyStopbandAttenuation;
            useS32 = true;
            fcr = mInSampleRate <= mSampleRate  ? 0.5 : 0.5 * mSampleRate / mInSampleRate;
            fcr *= mPropertyCutoffPercent / 100.;
        } else {
            if (mFilterQuality == DYN_HIGH_QUALITY) {
                // 32b coefficients, 64 length
                useS32 = true;
                stopBandAtten = 98.;
                if (inSampleRate >= mSampleRate * 4) {
                    halfLength = 48;
                } else if (inSampleRate >= mSampleRate * 2) {
                    halfLength = 40;
                } else {
                    halfLength = 32;
                }
            } else if (mFilterQuality == DYN_LOW_QUALITY) {
                // 16b coefficients, 16-32 length
                useS32 = false;
                stopBandAtten = 80.;
                if (inSampleRate >= mSampleRate * 4) {
                    halfLength = 24;
                } else if (inSampleRate >= mSampleRate * 2) {
                    halfLength = 16;
                } else {
                    halfLength = 8;
                }
                if (inSampleRate <= mSampleRate) {
                    tbwCheat = 1.05;
                } else {
                    tbwCheat = 1.03;
                }
            } else { // DYN_MED_QUALITY
                // 16b coefficients, 32-64 length
                // note: > 64 length filters with 16b coefs can have quantization noise problems
                useS32 = false;
                stopBandAtten = 84.;
                if (inSampleRate >= mSampleRate * 4) {
                    halfLength = 32;
                } else if (inSampleRate >= mSampleRate * 2) {
                    halfLength = 24;
                } else {
                    halfLength = 16;
                }
                if (inSampleRate <= mSampleRate) {
                    tbwCheat = 1.03;
                } else {
                    tbwCheat = 1.01;
                }
            }
        }

        // determine the number of polyphases in the filterbank.
        // for 16b, it is desirable to have 2^(16/2) = 256 phases.
        // https://ccrma.stanford.edu/~jos/resample/Relation_Interpolation_Error_Quantization.html
        //
        // We are a bit more lax on this.

        int phases = mSampleRate / gcd(mSampleRate, inSampleRate);

        // TODO: Once dynamic sample rate change is an option, the code below
        // should be modified to execute only when dynamic sample rate change is enabled.
        //
        // as above, #phases less than 63 is too few phases for accurate linear interpolation.
        // we increase the phases to compensate, but more phases means more memory per
        // filter and more time to compute the filter.
        //
        // if we know that the filter will be used for dynamic sample rate changes,
        // that would allow us skip this part for fixed sample rate resamplers.
        //
        while (phases<63) {
            phases *= 2; // this code only needed to support dynamic rate changes
        }

        if (phases>=256) {  // too many phases, always interpolate
            phases = 127;
        }

        // create the filter
        mConstants.set(phases, halfLength, inSampleRate, mSampleRate);
        if (fcr > 0.) {
            createKaiserFir(mConstants, stopBandAtten, fcr);
        } else {
            createKaiserFir(mConstants, stopBandAtten,inSampleRate, mSampleRate, tbwCheat);
        }
    } // End Kaiser filter

    // update phase and state based on the new filter.
    const Constants& c(mConstants);
    /**
     * 构造函数中赋值 mChannelCount = 2
    */
    mInBuffer.resize(mChannelCount, c.mHalfNumCoefs);
    /**
     * 
    */
    const uint32_t phaseWrapLimit = c.mL << c.mShift;
    // try to preserve as much of the phase fraction as possible for on-the-fly changes
    mPhaseFraction = static_cast<unsigned long long>(mPhaseFraction)  * phaseWrapLimit / oldPhaseWrapLimit;

    /**
     * 
    */
    mPhaseFraction %= phaseWrapLimit; // should not do anything, but just in case.
    mPhaseIncrement = static_cast<uint32_t>(static_cast<uint64_t>(phaseWrapLimit) * inSampleRate / mSampleRate);

    // determine which resampler to use
    // check if locked phase (works only if mPhaseIncrement has no "fractional phase bits")
    int locked = (mPhaseIncrement << (sizeof(mPhaseIncrement)*8 - c.mShift))  ==  0;
    if (locked) {
        mPhaseFraction = mPhaseFraction >> c.mShift << c.mShift; // remove fractional phase
    }

    // stride is the minimum number of filter coefficients processed per loop iteration.
    // We currently only allow a stride of 16 to match with SIMD processing.
    // This means that the filter length must be a multiple of 16,
    // or half the filter length (mHalfNumCoefs) must be a multiple of 8.
    //
    // Note: A stride of 2 is achieved with non-SIMD processing.
    int stride = ((c.mHalfNumCoefs & 7) == 0) ? 16 : 2;
    LOG_ALWAYS_FATAL_IF(stride < 16, "Resampler stride must be 16 or more");
    LOG_ALWAYS_FATAL_IF(mChannelCount < 1 || mChannelCount > 8, "Resampler channels(%d) must be between 1 to 8", mChannelCount);
    // stride 16 (falls back to stride 2 for machines that do not support NEON)
    if (locked) {
        switch (mChannelCount) {
        case 1:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<1, true, 16>;
            break;
        case 2:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<2, true, 16>;
            break;
        case 3:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<3, true, 16>;
            break;
        case 4:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<4, true, 16>;
            break;
        case 5:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<5, true, 16>;
            break;
        case 6:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<6, true, 16>;
            break;
        case 7:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<7, true, 16>;
            break;
        case 8:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<8, true, 16>;
            break;
        }
    } else {
        switch (mChannelCount) {
        case 1:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<1, false, 16>;
            break;
        case 2:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<2, false, 16>;
            break;
        case 3:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<3, false, 16>;
            break;
        case 4:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<4, false, 16>;
            break;
        case 5:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<5, false, 16>;
            break;
        case 6:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<6, false, 16>;
            break;
        case 7:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<7, false, 16>;
            break;
        case 8:
            mResampleFunc = &AudioResamplerDyn<TC, TI, TO>::resample<8, false, 16>;
            break;
        }
    }
#ifdef DEBUG_RESAMPLER
    printf("channels:%d  %s  stride:%d  %s  coef:%d  shift:%d\n", mChannelCount, locked ? "locked" : "interpolated", stride, useS32 ? "S32" : "S16", 2*c.mHalfNumCoefs, c.mShift);
#endif
}


/**
 * mResampler->resample((int32_t*)temp, outFrameCount, bufferProvider);
 * 
*/
template<typename TC, typename TI, typename TO>
size_t AudioResamplerDyn<TC, TI, TO>::resample(int32_t* out, size_t outFrameCount, AudioBufferProvider* provider)
{
   /**
    * 
   */
    return (this->*mResampleFunc)(reinterpret_cast<TO*>(out), outFrameCount, provider);
}




template<typename TC, typename TI, typename TO>
template<int CHANNELS, bool LOCKED, int STRIDE>
size_t AudioResamplerDyn<TC, TI, TO>::resample(TO* out, size_t outFrameCount,AudioBufferProvider* provider)
{
    // TODO Mono -> Mono is not supported. OUTPUT_CHANNELS reflects minimum of stereo out.
    const int OUTPUT_CHANNELS = (CHANNELS < 2) ? 2 : CHANNELS;
    const Constants& c(mConstants);
    const TC* const coefs = mConstants.mFirCoefs;
    TI* impulse = mInBuffer.getImpulse();
    size_t inputIndex = 0;
    uint32_t phaseFraction = mPhaseFraction;
    const uint32_t phaseIncrement = mPhaseIncrement;
    size_t outputIndex = 0;
    size_t outputSampleCount = outFrameCount * OUTPUT_CHANNELS;
    const uint32_t phaseWrapLimit = c.mL << c.mShift;
    size_t inFrameCount = (phaseIncrement * (uint64_t)outFrameCount + phaseFraction)  / phaseWrapLimit;
    // sanity check that inFrameCount is in signed 32 bit integer range.
    ALOG_ASSERT(0 <= inFrameCount && inFrameCount < (1U << 31));

    //ALOGV("inFrameCount:%d  outFrameCount:%d"
    //        "  phaseIncrement:%u  phaseFraction:%u  phaseWrapLimit:%u",
    //        inFrameCount, outFrameCount, phaseIncrement, phaseFraction, phaseWrapLimit);

    // NOTE: be very careful when modifying the code here. register
    // pressure is very high and a small change might cause the compiler
    // to generate far less efficient code.
    // Always sanity check the result with objdump or test-resample.

    // the following logic is a bit convoluted to keep the main processing loop
    // as tight as possible with register allocation.
    while (outputIndex < outputSampleCount) {
        //ALOGV("LOOP: inFrameCount:%d  outputIndex:%d  outFrameCount:%d"
        //        "  phaseFraction:%u  phaseWrapLimit:%u",
        //        inFrameCount, outputIndex, outFrameCount, phaseFraction, phaseWrapLimit);

        // check inputIndex overflow
        ALOG_ASSERT(inputIndex <= mBuffer.frameCount, "inputIndex%zu > frameCount%zu", inputIndex, mBuffer.frameCount);
        // Buffer is empty, fetch a new one if necessary (inFrameCount > 0).
        // We may not fetch a new buffer if the existing data is sufficient.
        while (mBuffer.frameCount == 0 && inFrameCount > 0) {
            mBuffer.frameCount = inFrameCount;
            provider->getNextBuffer(&mBuffer);
            if (mBuffer.raw == NULL) {
                // We are either at the end of playback or in an underrun situation.
                // Reset buffer to prevent pop noise at the next buffer.
                mInBuffer.reset();
                goto resample_exit;
            }
            inFrameCount -= mBuffer.frameCount;
            if (phaseFraction >= phaseWrapLimit) { // read in data
                mInBuffer.template readAdvance<CHANNELS>(impulse, c.mHalfNumCoefs,reinterpret_cast<TI*>(mBuffer.raw), inputIndex);
                inputIndex++;
                phaseFraction -= phaseWrapLimit;
                while (phaseFraction >= phaseWrapLimit) {
                    if (inputIndex >= mBuffer.frameCount) {
                        inputIndex = 0;
                        provider->releaseBuffer(&mBuffer);
                        break;
                    }
                    mInBuffer.template readAdvance<CHANNELS>(impulse, c.mHalfNumCoefs, reinterpret_cast<TI*>(mBuffer.raw), inputIndex);
                    inputIndex++;
                    phaseFraction -= phaseWrapLimit;
                }
            }
        }
        const TI* const in = reinterpret_cast<const TI*>(mBuffer.raw);
        const size_t frameCount = mBuffer.frameCount;
        const int coefShift = c.mShift;
        const int halfNumCoefs = c.mHalfNumCoefs;
        const TO* const volumeSimd = mVolumeSimd;

        // main processing loop
        while (CC_LIKELY(outputIndex < outputSampleCount)) {
            // caution: fir() is inlined and may be large.
            // output will be loaded with the appropriate values
            //
            // from the input samples in impulse[-halfNumCoefs+1]... impulse[halfNumCoefs]
            // from the polyphase filter of (phaseFraction / phaseWrapLimit) in coefs.
            //
            //ALOGV("LOOP2: inFrameCount:%d  outputIndex:%d  outFrameCount:%d"
            //        "  phaseFraction:%u  phaseWrapLimit:%u",
            //        inFrameCount, outputIndex, outFrameCount, phaseFraction, phaseWrapLimit);
            ALOG_ASSERT(phaseFraction < phaseWrapLimit);
            fir<CHANNELS, LOCKED, STRIDE>(&out[outputIndex],phaseFraction, phaseWrapLimit,coefShift, halfNumCoefs, coefs, impulse, volumeSimd);

            outputIndex += OUTPUT_CHANNELS;

            phaseFraction += phaseIncrement;
            while (phaseFraction >= phaseWrapLimit) {
                if (inputIndex >= frameCount) {
                    goto done;  // need a new buffer
                }
                mInBuffer.template readAdvance<CHANNELS>(impulse, halfNumCoefs, in, inputIndex);
                inputIndex++;
                phaseFraction -= phaseWrapLimit;
            }
        }
done:
        // We arrive here when we're finished or when the input buffer runs out.
        // Regardless we need to release the input buffer if we've acquired it.
        if (inputIndex > 0) {  // we've acquired a buffer (alternatively could check frameCount)
            ALOG_ASSERT(inputIndex == frameCount, "inputIndex(%zu) != frameCount(%zu)",inputIndex, frameCount);  // must have been fully read.
            inputIndex = 0;
            provider->releaseBuffer(&mBuffer);
            ALOG_ASSERT(mBuffer.frameCount == 0);
        }
    }

resample_exit:
    // inputIndex must be zero in all three cases:
    // (1) the buffer never was been acquired; (2) the buffer was
    // released at "done:"; or (3) getNextBuffer() failed.
    ALOG_ASSERT(inputIndex == 0, "Releasing: inputindex:%zu frameCount:%zu  phaseFraction:%u",inputIndex, mBuffer.frameCount, phaseFraction);
    ALOG_ASSERT(mBuffer.frameCount == 0); // there must be no frames in the buffer
    mInBuffer.setImpulse(impulse);
    mPhaseFraction = phaseFraction;
    return outputIndex / OUTPUT_CHANNELS;
}