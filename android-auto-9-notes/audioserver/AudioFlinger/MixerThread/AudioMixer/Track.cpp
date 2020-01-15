/***
 *   @ frameworks/av/media/libaudioprocessing/AudioMixer.cpp
 */   
Track::Track(): bufferProvider(nullptr)
{
    // TODO: move additional initialization here.
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * status_t AudioMixer::create(int name, audio_channel_mask_t channelMask, audio_format_t format, int sessionId)
 * --->  status_t status = t->prepareForDownmix();
*/
status_t AudioMixer::Track::prepareForDownmix()
{
    ALOGV("AudioMixer::prepareForDownmix(%p) with mask 0x%x",this, channelMask);

    // discard the previous downmixer if there was one
    unprepareForDownmix();
    // MONO_HACK Only remix (upmix or downmix) if the track and mixer/device channel masks
    // are not the same and not handled internally, as mono -> stereo currently is.
    if (channelMask == mMixerChannelMask  || (channelMask == AUDIO_CHANNEL_OUT_MONO  && mMixerChannelMask == AUDIO_CHANNEL_OUT_STEREO)) {
        return NO_ERROR;
    }
    // DownmixerBufferProvider is only used for position masks.
    if (audio_channel_mask_get_representation(channelMask)  == AUDIO_CHANNEL_REPRESENTATION_POSITION && DownmixerBufferProvider::isMultichannelCapable()) {
        mDownmixerBufferProvider.reset(new DownmixerBufferProvider(channelMask,mMixerChannelMask, AUDIO_FORMAT_PCM_16_BIT /* TODO: use mMixerInFormat, now only PCM 16 */,sampleRate, sessionId, kCopyBufferFrameCount));
        if (static_cast<DownmixerBufferProvider *>(mDownmixerBufferProvider.get())->isValid()) {
            mDownmixRequiresFormat = AUDIO_FORMAT_PCM_16_BIT; // PCM 16 bit required for downmix
            reconfigureBufferProviders();
            return NO_ERROR;
        }
        // mDownmixerBufferProvider reset below.
    }

    // Effect downmixer does not accept the channel conversion.  Let's use our remixer.
    mDownmixerBufferProvider.reset(new RemixBufferProvider(channelMask,mMixerChannelMask, mMixerInFormat, kCopyBufferFrameCount));
    // Remix always finds a conversion whereas Downmixer effect above may fail.
    reconfigureBufferProviders();

    return NO_ERROR;
}


void AudioMixer::Track::unprepareForDownmix() {
    ALOGV("AudioMixer::unprepareForDownmix(%p)", this);

    if (mPostDownmixReformatBufferProvider.get() != nullptr) {
        // release any buffers held by the mPostDownmixReformatBufferProvider
        // before deallocating the mDownmixerBufferProvider.
        mPostDownmixReformatBufferProvider->reset();
    }

    mDownmixRequiresFormat = AUDIO_FORMAT_INVALID;
    if (mDownmixerBufferProvider.get() != nullptr) {
        // this track had previously been configured with a downmixer, delete it
        mDownmixerBufferProvider.reset(nullptr);
        reconfigureBufferProviders();
    } else {
        ALOGV(" nothing to do, no downmixer to delete");
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * status_t AudioMixer::create(int name, audio_channel_mask_t channelMask, audio_format_t format, int sessionId)
 * --->  status_t status = t->prepareForReformat();
*/
status_t AudioMixer::Track::prepareForReformat()
{
    ALOGV("AudioMixer::prepareForReformat(%p) with format %#x", this, mFormat);
    // discard previous reformatters
    unprepareForReformat();
    // only configure reformatters as needed
    const audio_format_t targetFormat = mDownmixRequiresFormat != AUDIO_FORMAT_INVALID  ? mDownmixRequiresFormat : mMixerInFormat;
    bool requiresReconfigure = false;
    if (mFormat != targetFormat) {
        mReformatBufferProvider.reset(new ReformatBufferProvider(audio_channel_count_from_out_mask(channelMask),mFormat,targetFormat,kCopyBufferFrameCount));
        requiresReconfigure = true;
    } else if (mFormat == AUDIO_FORMAT_PCM_FLOAT) {
        // Input and output are floats, make sure application did not provide > 3db samples
        // that would break volume application (b/68099072)
        // TODO: add a trusted source flag to avoid the overhead
        mReformatBufferProvider.reset(new ClampFloatBufferProvider(audio_channel_count_from_out_mask(channelMask),kCopyBufferFrameCount));
        requiresReconfigure = true;
    }

    if (targetFormat != mMixerInFormat) {
        mPostDownmixReformatBufferProvider.reset(new ReformatBufferProvider(audio_channel_count_from_out_mask(mMixerChannelMask),targetFormat,mMixerInFormat,kCopyBufferFrameCount));
        requiresReconfigure = true;
    }
    if (requiresReconfigure) {
        reconfigureBufferProviders();
    }
    return NO_ERROR;
}

/**
 * 
*/
void AudioMixer::Track::reconfigureBufferProviders()
{
    bufferProvider = mInputBufferProvider;

    if (mReformatBufferProvider.get() != nullptr) {
        mReformatBufferProvider->setBufferProvider(bufferProvider);
        bufferProvider = mReformatBufferProvider.get();
    }

    if (mDownmixerBufferProvider.get() != nullptr) {
        mDownmixerBufferProvider->setBufferProvider(bufferProvider);
        bufferProvider = mDownmixerBufferProvider.get();
    }

    if (mPostDownmixReformatBufferProvider.get() != nullptr) {
        mPostDownmixReformatBufferProvider->setBufferProvider(bufferProvider);
        bufferProvider = mPostDownmixReformatBufferProvider.get();
    }

    if (mTimestretchBufferProvider.get() != nullptr) {
        mTimestretchBufferProvider->setBufferProvider(bufferProvider);
        bufferProvider = mTimestretchBufferProvider.get();
    }

}



bool AudioMixer::Track::setResampler(uint32_t trackSampleRate, uint32_t devSampleRate)
{
    ALOGV("%s",__func__);
    if (trackSampleRate != devSampleRate || mResampler.get() != nullptr) {
        if (sampleRate != trackSampleRate) {
            sampleRate = trackSampleRate;
            if (mResampler.get() == nullptr) {
                ALOGV("Creating resampler from track %d Hz to device %d Hz", trackSampleRate, devSampleRate);
                AudioResampler::src_quality quality;
                // force lowest quality level resampler if use case isn't music or video
                // FIXME this is flawed for dynamic sample rates, as we choose the resampler
                // quality level based on the initial ratio, but that could change later.
                // Should have a way to distinguish tracks with static ratios vs. dynamic ratios.
                if (isMusicRate(trackSampleRate)) {
                    quality = AudioResampler::DEFAULT_QUALITY;
                } else {
                    quality = AudioResampler::DYN_LOW_QUALITY;
                }

                // TODO: Remove MONO_HACK. Resampler sees #channels after the downmixer
                // but if none exists, it is the channel count (1 for mono).
                const int resamplerChannelCount = mDownmixerBufferProvider.get() != nullptr ? mMixerChannelCount : channelCount;
                //      01-15 03:25:54.930  1809  1844 V AudioMixer: Creating resampler: format(0x5) channels(2) devSampleRate(48000) quality(0)
                ALOGV("Creating resampler:" " format(%#x) channels(%d) devSampleRate(%u) quality(%d)\n", mMixerInFormat, resamplerChannelCount, devSampleRate, quality);
                mResampler.reset(AudioResampler::create(mMixerInFormat,resamplerChannelCount,devSampleRate, quality));
            }
            return true;
        }
    }
    return false;
}


bool AudioMixer::Track::doesResample() const { 
         return mResampler.get() != nullptr; 
}



/* Returns the proper track hook to use for mixing the track into the output buffer.
 */
/* static */
AudioMixer::hook_t AudioMixer::Track::getTrackHook(int trackType, uint32_t channelCount,audio_format_t mixerInFormat, audio_format_t mixerOutFormat __unused)
{
    if (!kUseNewMixer && channelCount == FCC_2 && mixerInFormat == AUDIO_FORMAT_PCM_16_BIT) {
        switch (trackType) {
        case TRACKTYPE_NOP:
            return &Track::track__nop;
        case TRACKTYPE_RESAMPLE:
            return &Track::track__genericResample;
        case TRACKTYPE_NORESAMPLEMONO:
            return &Track::track__16BitsMono;
        case TRACKTYPE_NORESAMPLE:
            return &Track::track__16BitsStereo;
        default:
            LOG_ALWAYS_FATAL("bad trackType: %d", trackType);
            break;
        }
    }

    LOG_ALWAYS_FATAL_IF(channelCount > MAX_NUM_CHANNELS);

    switch (trackType) {
    case TRACKTYPE_NOP:
        return &Track::track__nop;
    case TRACKTYPE_RESAMPLE:
        switch (mixerInFormat) {
        case AUDIO_FORMAT_PCM_FLOAT://  system/media/audio/include/system/audio-base.h:150:    AUDIO_FORMAT_PCM_FLOAT             = 0x5u,        // (PCM | PCM_SUB_FLOAT)
            return (AudioMixer::hook_t) &Track::track__Resample< MIXTYPE_MULTI, float /*TO*/, float /*TI*/, TYPE_AUX>;
        case AUDIO_FORMAT_PCM_16_BIT: //        system/media/audio/include/system/audio-base.h:146:    AUDIO_FORMAT_PCM_16_BIT            = 0x1u,        // (PCM | PCM_SUB_16_BIT)
            return (AudioMixer::hook_t) &Track::track__Resample<MIXTYPE_MULTI, int32_t /*TO*/, int16_t /*TI*/, TYPE_AUX>;
        default:
            LOG_ALWAYS_FATAL("bad mixerInFormat: %#x", mixerInFormat);
            break;
        }
        break;
    case TRACKTYPE_NORESAMPLEMONO:
        switch (mixerInFormat) {
        case AUDIO_FORMAT_PCM_FLOAT:
            return (AudioMixer::hook_t) &Track::track__NoResample< MIXTYPE_MONOEXPAND, float /*TO*/, float /*TI*/, TYPE_AUX>;
        case AUDIO_FORMAT_PCM_16_BIT:
            return (AudioMixer::hook_t) &Track::track__NoResample<MIXTYPE_MONOEXPAND, int32_t /*TO*/, int16_t /*TI*/, TYPE_AUX>;
        default:
            LOG_ALWAYS_FATAL("bad mixerInFormat: %#x", mixerInFormat);
            break;
        }
        break;
    case TRACKTYPE_NORESAMPLE:
        switch (mixerInFormat) {
        case AUDIO_FORMAT_PCM_FLOAT:
            return (AudioMixer::hook_t) &Track::track__NoResample<MIXTYPE_MULTI, float /*TO*/, float /*TI*/, TYPE_AUX>;
        case AUDIO_FORMAT_PCM_16_BIT:
            return (AudioMixer::hook_t) &Track::track__NoResample<MIXTYPE_MULTI, int32_t /*TO*/, int16_t /*TI*/, TYPE_AUX>;
        default:
            LOG_ALWAYS_FATAL("bad mixerInFormat: %#x", mixerInFormat);
            break;
        }
        break;
    default:
        LOG_ALWAYS_FATAL("bad trackType: %d", trackType);
        break;
    }
    return NULL;
}


/* This track hook is called to do resampling then mixing, pulling from the track's upstream AudioBufferProvider.
 *
 * MIXTYPE     (see AudioMixerOps.h MIXTYPE_* enumeration)
 * TO: int32_t (Q4.27) or float
 * TI: int32_t (Q4.27) or int16_t (Q0.15) or float
 * TA: int32_t (Q4.27) or float
 */
template <int MIXTYPE, typename TO, typename TI, typename TA>
void AudioMixer::Track::track__Resample(TO* out, size_t outFrameCount, TO* temp, TA* aux)
{
    ALOGV("track__Resample\n");
    mResampler->setSampleRate(sampleRate);
    /**
     * bool        needsRamp() { return (volumeInc[0] | volumeInc[1] | auxInc) != 0; }
    */
    const bool ramp = needsRamp();// true
    if (ramp || aux != NULL) {
        // if ramp:        resample with unity gain to temp buffer and scale/mix in 2nd step.
        // if aux != NULL: resample with unity gain to temp buffer then apply send level.

        mResampler->setVolume(UNITY_GAIN_FLOAT, UNITY_GAIN_FLOAT);
        memset(temp, 0, outFrameCount * mMixerChannelCount * sizeof(TO));
        /**
         * 
        */
        mResampler->resample((int32_t*)temp, outFrameCount, bufferProvider);

        volumeMix<MIXTYPE, is_same<TI, float>::value /* USEFLOATVOL */, true /* ADJUSTVOL */>(out, outFrameCount, temp, aux, ramp);

    } else { // constant volume gain
        mResampler->setVolume(mVolume[0], mVolume[1]);
        mResampler->resample((int32_t*)out, outFrameCount, bufferProvider);
    }
}



