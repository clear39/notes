
/**
 *   @   frameworks/av/media/libaudioprocessing/AudioMixer.cpp
 * 传入参数：
 *      sampleRate这个表示线程当前采样率 41800，也是物理硬件支持的采样率
 * */
AudioMixer::AudioMixer(size_t frameCount, uint32_t sampleRate)
    : mSampleRate(sampleRate), mFrameCount(frameCount) {
    pthread_once(&sOnceControl, &sInitRoutine);
}


/*static*/ pthread_once_t AudioMixer::sOnceControl = PTHREAD_ONCE_INIT;

/*static*/ void AudioMixer::sInitRoutine()
{
    DownmixerBufferProvider::init(); // for the downmixer
}


bool  AudioMixer::exists(int name) const {
    // track smart pointers, by name, in increasing order of name.  std::map<int /* name */, std::shared_ptr<Track>> mTracks;
    return mTracks.count(name) > 0;
}

/**
 * 
*/
status_t AudioMixer::create(int name, audio_channel_mask_t channelMask, audio_format_t format, int sessionId)
{
    LOG_ALWAYS_FATAL_IF(exists(name), "name %d already exists", name);

    /**
     * 
    */
    if (!isValidChannelMask(channelMask)) {
        ALOGE("%s invalid channelMask: %#x", __func__, channelMask);
        return BAD_VALUE;
    }

    /**
     * 
    */
    if (!isValidFormat(format)) {
        ALOGE("%s invalid format: %#x", __func__, format);
        return BAD_VALUE;
    }

    auto t = std::make_shared<Track>();
    {
        // TODO: move initialization to the Track constructor.
        // assume default parameters for the track, except where noted below
        t->needs = 0;

        // Integer volume.
        // Currently integer volume is kept for the legacy integer mixer.
        // Will be removed when the legacy mixer path is removed.
        /**
         * frameworks/av/media/libaudioclient/include/media/AudioMixer.h:56:    static const uint16_t UNITY_GAIN_INT = 0x1000;
        */
        t->volume[0] = UNITY_GAIN_INT;
        t->volume[1] = UNITY_GAIN_INT;
        t->prevVolume[0] = UNITY_GAIN_INT << 16;
        t->prevVolume[1] = UNITY_GAIN_INT << 16;
        t->volumeInc[0] = 0;
        t->volumeInc[1] = 0;
        t->auxLevel = 0;
        t->auxInc = 0;
        t->prevAuxLevel = 0;

        /**
         * frameworks/av/media/libaudioclient/include/media/AudioMixer.h:57:    static const CONSTEXPR float UNITY_GAIN_FLOAT = 1.0f;
        */
        // Floating point volume.
        t->mVolume[0] = UNITY_GAIN_FLOAT;
        t->mVolume[1] = UNITY_GAIN_FLOAT;
        t->mPrevVolume[0] = UNITY_GAIN_FLOAT;
        t->mPrevVolume[1] = UNITY_GAIN_FLOAT;
        t->mVolumeInc[0] = 0.;
        t->mVolumeInc[1] = 0.;
        t->mAuxLevel = 0.;
        t->mAuxInc = 0.;
        t->mPrevAuxLevel = 0.;

        // no initialization needed
        // t->frameCount
        t->channelCount = audio_channel_count_from_out_mask(channelMask);
        t->enabled = false;

        ALOGV_IF(audio_channel_mask_get_bits(channelMask) != AUDIO_CHANNEL_OUT_STEREO,"Non-stereo channel mask: %d\n", channelMask);

        t->channelMask = channelMask;
        t->sessionId = sessionId;
        // setBufferProvider(name, AudioBufferProvider *) is required before enable(name)
        t->bufferProvider = NULL;
        t->buffer.raw = NULL;
        // no initialization needed
        // t->buffer.frameCount
        t->hook = NULL;
        t->mIn = NULL;
        t->sampleRate = mSampleRate;
        // setParameter(name, TRACK, MAIN_BUFFER, mixBuffer) is required before enable(name)
        t->mainBuffer = NULL;
        t->auxBuffer = NULL;
        t->mInputBufferProvider = NULL;
        t->mMixerFormat = AUDIO_FORMAT_PCM_16_BIT;
        t->mFormat = format;            //      
        /**
         * frameworks/av/media/libaudioprocessing/AudioMixer.cpp:72:static constexpr bool kUseFloat = true;
         * frameworks/av/media/libaudioprocessing/AudioMixer.cpp:67:static constexpr bool kUseNewMixer = true;
         * 
         * return kUseFloat && kUseNewMixer ? AUDIO_FORMAT_PCM_FLOAT : AUDIO_FORMAT_PCM_16_BIT;
        */
        t->mMixerInFormat = selectMixerInFormat(format);// AUDIO_FORMAT_PCM_FLOAT
        t->mDownmixRequiresFormat = AUDIO_FORMAT_INVALID; // no format required
        /**
         * audio_channel_mask_from_representation_and_bits @    system/media/audio/include/system/audio.h
         * system/media/audio/include/system/audio-base.h:186:    AUDIO_CHANNEL_REPRESENTATION_POSITION   = 0x0u,
         * system/media/audio/include/system/audio-base.h:212:    AUDIO_CHANNEL_OUT_STEREO                = 0x3u,     // OUT_FRONT_LEFT | OUT_FRONT_RIGHT
         * 将 AUDIO_CHANNEL_REPRESENTATION_POSITION 左移 30位 再和 AUDIO_CHANNEL_OUT_STEREO 相与，这里返回值为 0x3
        */
        t->mMixerChannelMask = audio_channel_mask_from_representation_and_bits(AUDIO_CHANNEL_REPRESENTATION_POSITION, AUDIO_CHANNEL_OUT_STEREO);
        /**
         * 
        */
        t->mMixerChannelCount = audio_channel_count_from_out_mask(t->mMixerChannelMask);
        /**
        static const AudioPlaybackRate AUDIO_PLAYBACK_RATE_DEFAULT = {
                AUDIO_TIMESTRETCH_SPEED_NORMAL,//frameworks/av/media/libaudioprocessing/include/media/AudioResamplerPublic.h:49:#define AUDIO_TIMESTRETCH_SPEED_NORMAL 1.0f
                AUDIO_TIMESTRETCH_PITCH_NORMAL,//frameworks/av/media/libaudioprocessing/include/media/AudioResamplerPublic.h:61:#define AUDIO_TIMESTRETCH_PITCH_NORMAL 1.0f
                AUDIO_TIMESTRETCH_STRETCH_DEFAULT,//frameworks/av/media/libaudioprocessing/include/media/AudioResamplerPublic.h:67:    AUDIO_TIMESTRETCH_STRETCH_DEFAULT = 0,
                AUDIO_TIMESTRETCH_FALLBACK_DEFAULT,//frameworks/av/media/libaudioprocessing/include/media/AudioResamplerPublic.h:84:    AUDIO_TIMESTRETCH_FALLBACK_DEFAULT= 0,
        };

        */
        t->mPlaybackRate = AUDIO_PLAYBACK_RATE_DEFAULT;

        // Check the downmixing (or upmixing) requirements.
        status_t status = t->prepareForDownmix();
        if (status != OK) {
            ALOGE("AudioMixer::getTrackName invalid channelMask (%#x)", channelMask);
            return BAD_VALUE;
        }

        //      01-15 03:25:54.894  1809  1844 V AudioMixer: mMixerFormat:0x1  mMixerInFormat:0x5
        // prepareForDownmix() may change mDownmixRequiresFormat
        ALOGVV("mMixerFormat:%#x  mMixerInFormat:%#x\n", t->mMixerFormat, t->mMixerInFormat);
        t->prepareForReformat();

        mTracks[name] = t;
        return OK;
    }
}



void AudioMixer::setParameter(int name, int target, int param, void *value)
{
    LOG_ALWAYS_FATAL_IF(!exists(name), "invalid name: %d", name);
    
    const std::shared_ptr<Track> &track = mTracks[name];

    int valueInt = static_cast<int>(reinterpret_cast<uintptr_t>(value));
    int32_t *valueBuf = reinterpret_cast<int32_t*>(value);

    switch (target) {


    case TRACK::////////////////////////////////////////////////////////////////////////
        switch (param) {
        case CHANNEL_MASK: {
            const audio_channel_mask_t trackChannelMask = static_cast<audio_channel_mask_t>(valueInt);
            if (setChannelMasks(name, trackChannelMask, track->mMixerChannelMask)) {
                ALOGV("setParameter(TRACK, CHANNEL_MASK, %x)", trackChannelMask);
                invalidate();
            }
            } break;
        case MAIN_BUFFER:
            if (track->mainBuffer != valueBuf) {
                /**
                 * track->mainBuffer  为 mMixerBuffer
                */
                track->mainBuffer = valueBuf;
                ALOGV("setParameter(TRACK, MAIN_BUFFER, %p)", valueBuf);
                invalidate();
            }
            break;
        case AUX_BUFFER:
            if (track->auxBuffer != valueBuf) {
                track->auxBuffer = valueBuf;
                ALOGV("setParameter(TRACK, AUX_BUFFER, %p)", valueBuf);
                invalidate();
            }
            break;
        case FORMAT: {
            audio_format_t format = static_cast<audio_format_t>(valueInt);
            if (track->mFormat != format) {
                ALOG_ASSERT(audio_is_linear_pcm(format), "Invalid format %#x", format);
                track->mFormat = format;
                ALOGV("setParameter(TRACK, FORMAT, %#x)", format);
                track->prepareForReformat();
                invalidate();
            }
            } break;
        // FIXME do we want to support setting the downmix type from AudioFlinger?
        //         for a specific track? or per mixer?
        /* case DOWNMIX_TYPE:
            break          */
        case MIXER_FORMAT: {
            audio_format_t format = static_cast<audio_format_t>(valueInt);
            if (track->mMixerFormat != format) {
                track->mMixerFormat = format;
                ALOGV("setParameter(TRACK, MIXER_FORMAT, %#x)", format);
            }
            } break;
        case MIXER_CHANNEL_MASK: {
            const audio_channel_mask_t mixerChannelMask = static_cast<audio_channel_mask_t>(valueInt);
            if (setChannelMasks(name, track->channelMask, mixerChannelMask)) {
                ALOGV("setParameter(TRACK, MIXER_CHANNEL_MASK, %#x)", mixerChannelMask);
                invalidate();
            }
            } break;
        default:
            LOG_ALWAYS_FATAL("setParameter track: bad param %d", param);
        }
        break;

    case RESAMPLE:////////////////////////////////////////////////////////////////////////
        switch (param) {
        case SAMPLE_RATE:
            ALOG_ASSERT(valueInt > 0, "bad sample rate %d", valueInt);
            if (track->setResampler(uint32_t(valueInt), mSampleRate)) {
                //01-15 03:25:54.930  1809  1844 V AudioMixer: setParameter(RESAMPLE, SAMPLE_RATE, 44100)
                ALOGV("setParameter(RESAMPLE, SAMPLE_RATE, %u)",uint32_t(valueInt));
                invalidate();
            }
            break;
        case RESET:
            track->resetResampler();
            invalidate();
            break;
        case REMOVE:
            track->mResampler.reset(nullptr);
            track->sampleRate = mSampleRate;
            invalidate();
            break;
        default:
            LOG_ALWAYS_FATAL("setParameter resample: bad param %d", param);
        }
        break;

    case RAMP_VOLUME: ////////////////////////////////////////////////////////////////////////
    case VOLUME:                ////////////////////////////////////////////////////////////////////////
        switch (param) {
        case AUXLEVEL:
            if (setVolumeRampVariables(*reinterpret_cast<float*>(value),
                                                                        target == RAMP_VOLUME ? mFrameCount : 0,
                                                                        &track->auxLevel, 
                                                                        &track->prevAuxLevel,
                                                                        &track->auxInc,  
                                                                        &track->mAuxLevel, 
                                                                        &track->mPrevAuxLevel, 
                                                                        &track->mAuxInc)) {
                ALOGV("setParameter(%s, AUXLEVEL: %04x)",target == VOLUME ? "VOLUME" : "RAMP_VOLUME", track->auxLevel);
                invalidate();
            }
            break;
        default:
            if ((unsigned)param >= VOLUME0 && (unsigned)param < VOLUME0 + MAX_NUM_VOLUMES) {

                if (setVolumeRampVariables(*reinterpret_cast<float*>(value),
                        target == RAMP_VOLUME ? mFrameCount : 0,
                        &track->volume[param - VOLUME0],
                        &track->prevVolume[param - VOLUME0],
                        &track->volumeInc[param - VOLUME0],
                        &track->mVolume[param - VOLUME0],
                        &track->mPrevVolume[param - VOLUME0],
                        &track->mVolumeInc[param - VOLUME0])) {
                    ALOGV("setParameter(%s, VOLUME%d: %04x)", target == VOLUME ? "VOLUME" : "RAMP_VOLUME", param - VOLUME0,track->volume[param - VOLUME0]);
                    invalidate();
                }

            } else {
                LOG_ALWAYS_FATAL("setParameter volume: bad param %d", param);
            }
        }
        break;



        case TIMESTRETCH:////////////////////////////////////////////////////////////////////////
            switch (param) {
            case PLAYBACK_RATE: {
                const AudioPlaybackRate *playbackRate = reinterpret_cast<AudioPlaybackRate*>(value);
                ALOGW_IF(!isAudioPlaybackRateValid(*playbackRate), "bad parameters speed %f, pitch %f",playbackRate->mSpeed, playbackRate->mPitch);
                if (track->setPlaybackRate(*playbackRate)) {
                    ALOGV("setParameter(TIMESTRETCH, PLAYBACK_RATE, STRETCH_MODE, FALLBACK_MODE "  "%f %f %d %d",
                            playbackRate->mSpeed,
                            playbackRate->mPitch,
                            playbackRate->mStretchMode,
                            playbackRate->mFallbackMode);
                    // invalidate();  (should not require reconfigure)
                }
            } break;
            default:
                LOG_ALWAYS_FATAL("setParameter timestretch: bad param %d", param);
            }
            break;

    default:
        LOG_ALWAYS_FATAL("setParameter: bad target %d", target);
    }

}

/**
 * AudioFlinger::PlaybackThread::mixer_state AudioFlinger::MixerThread::prepareTracks_l( Vector< sp<Track> > *tracksToRemove)
 * --> mAudioMixer->setBufferProvider(name, track);
*/
void AudioMixer::setBufferProvider(int name, AudioBufferProvider* bufferProvider)
{
    LOG_ALWAYS_FATAL_IF(!exists(name), "invalid name: %d", name);
    const std::shared_ptr<Track> &track = mTracks[name];

    if (track->mInputBufferProvider == bufferProvider) {
        return; // don't reset any buffer providers if identical.
    }

    if (track->mReformatBufferProvider.get() != nullptr) {
        track->mReformatBufferProvider->reset();
    } else if (track->mDownmixerBufferProvider != nullptr) {
        track->mDownmixerBufferProvider->reset();
    } else if (track->mPostDownmixReformatBufferProvider.get() != nullptr) {
        track->mPostDownmixReformatBufferProvider->reset();
    } else if (track->mTimestretchBufferProvider.get() != nullptr) {
        track->mTimestretchBufferProvider->reset();
    }

    track->mInputBufferProvider = bufferProvider;
    track->reconfigureBufferProviders();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * AudioFlinger::PlaybackThread::mixer_state AudioFlinger::MixerThread::prepareTracks_l( Vector< sp<Track> > *tracksToRemove)
 * --->void AudioMixer::enable(int name)
 * ----->
*/
// Called when track info changes and a new process hook should be determined.
void AudioMixer::invalidate() {
mHook = &AudioMixer::process__validate;
}

/**
 * 
 * void AudioFlinger::MixerThread::threadLoop_mix()
 * --->
 * 
 * process_hook_t mHook = &AudioMixer::process__nop;   // one of process__*, never nullptr
*/
void    AudioMixer::process() {
        (this->*mHook)();
}


void AudioMixer::process__validate()
{
    // TODO: fix all16BitsStereNoResample logic to
    // either properly handle muted tracks (it should ignore them)
    // or remove altogether as an obsolete optimization.
    bool all16BitsStereoNoResample = true;
    bool resampling = false;
    bool volumeRamp = false;
    /**
     * // track names that are enabled, in increasing order (by construction).   
     * std::vector<int> mEnabled;
     * 
     * // track smart pointers, by name, in increasing order of name.
     * std::map<int, std::shared_ptr<Track>> mTracks;
    */
    mEnabled.clear();

    /**
     * // track names grouped by main buffer, in no particular order of main buffer.
     *  // however names for a particular main buffer are in order (by construction).
     * std::unordered_map<void *, std::vector<int>> mGroups;
    */
    mGroups.clear();

    for (const auto &pair : mTracks) {
        const int name = pair.first;
        const std::shared_ptr<Track> &t = pair.second;
        if (!t->enabled) continue;

        mEnabled.emplace_back(name);  // we add to mEnabled in order of name.
        /**
         * std::unordered_map<void *, std::vector<int>> mGroups;
         * 
         * t->mainBuffer 对应 thread 中 mMixerBuffer 
         * 
        */
        mGroups[t->mainBuffer].emplace_back(name); // mGroups also in order of name.

        /*
        enum {
                NEEDS_CHANNEL_1             = 0x00000000,   // mono(单通道)
                NEEDS_CHANNEL_2             = 0x00000001,   // stereo（立体声/双通道）

                // sample format is not explicitly specified, and is assumed to be AUDIO_FORMAT_PCM_16_BIT

                NEEDS_MUTE                  = 0x00000100,
                NEEDS_RESAMPLE              = 0x00001000,
                NEEDS_AUX                   = 0x00010000,
       };

        */
        uint32_t n = 0;
        // FIXME can overflow (mask is only 3 bits)
        n |= NEEDS_CHANNEL_1 + t->channelCount - 1;
        /**
         *doesResample 判断 mResampler.get() 是否为空，不为空则返回true
         * 这里 doesResample 返回 true
        */
        if (t->doesResample()) {
            n |= NEEDS_RESAMPLE;
        }
        /**
         * t->auxBuffer = NULL
        */
        if (t->auxLevel != 0 && t->auxBuffer != NULL) {
            n |= NEEDS_AUX;
        }

        if (t->volumeInc[0] | t->volumeInc[1]) {
            volumeRamp = true;
        } else if (!t->doesResample() && t->volumeRL == 0) {
            n |= NEEDS_MUTE;
        }
        /**
         * 
        */
        t->needs = n;

        if (n & NEEDS_MUTE) {
            t->hook = &Track::track__nop;
        } else {
            // 执行这里

            
            if (n & NEEDS_AUX) {
                all16BitsStereoNoResample = false;
            }

            if (n & NEEDS_RESAMPLE) {
                 // 执行这里
                all16BitsStereoNoResample = false;
                resampling = true;
                /**
                 * 
                */
                t->hook = Track::getTrackHook(TRACKTYPE_RESAMPLE, t->mMixerChannelCount,t->mMixerInFormat, t->mMixerFormat);
                ALOGV_IF((n & NEEDS_CHANNEL_COUNT__MASK) > NEEDS_CHANNEL_2,"Track %d needs downmix + resample", i);
            } else {

                if ((n & NEEDS_CHANNEL_COUNT__MASK) == NEEDS_CHANNEL_1){
                    t->hook = Track::getTrackHook((t->mMixerChannelMask == AUDIO_CHANNEL_OUT_STEREO  // TODO: MONO_HACK
                                    && t->channelMask == AUDIO_CHANNEL_OUT_MONO)? TRACKTYPE_NORESAMPLEMONO : TRACKTYPE_NORESAMPLE,
                                    t->mMixerChannelCount,t->mMixerInFormat, t->mMixerFormat);
                    all16BitsStereoNoResample = false;
                }

                if ((n & NEEDS_CHANNEL_COUNT__MASK) >= NEEDS_CHANNEL_2){
                    t->hook = Track::getTrackHook(TRACKTYPE_NORESAMPLE, t->mMixerChannelCount, t->mMixerInFormat, t->mMixerFormat);
                    ALOGV_IF((n & NEEDS_CHANNEL_COUNT__MASK) > NEEDS_CHANNEL_2, "Track %d needs downmix", i);
                }

            }
        }
    }

    // select the processing hooks
    mHook = &AudioMixer::process__nop;
    if (mEnabled.size() > 0) {
        if (resampling) {
            if (mOutputTemp.get() == nullptr) {
                mOutputTemp.reset(new int32_t[MAX_NUM_CHANNELS * mFrameCount]);
            }
            if (mResampleTemp.get() == nullptr) {
                mResampleTemp.reset(new int32_t[MAX_NUM_CHANNELS * mFrameCount]);
            }
            // 执行这里
            mHook = &AudioMixer::process__genericResampling;

        } else {
            // we keep temp arrays around.
            mHook = &AudioMixer::process__genericNoResampling;
            if (all16BitsStereoNoResample && !volumeRamp) {
                if (mEnabled.size() == 1) {
                    const std::shared_ptr<Track> &t = mTracks[mEnabled[0]];
                    if ((t->needs & NEEDS_MUTE) == 0) {
                        // The check prevents a muted track from acquiring a process hook.
                        //
                        // This is dangerous if the track is MONO as that requires
                        // special case handling due to implicit channel duplication.
                        // Stereo or Multichannel should actually be fine here.
                        mHook = getProcessHook(PROCESSTYPE_NORESAMPLEONETRACK,t->mMixerChannelCount, t->mMixerInFormat, t->mMixerFormat);
                    }
                }
            }
        }
    }
   //   01-15 03:25:54.930  1809  1844 V AudioMixer: mixer configuration change: 1 all16BitsStereoNoResample=0, resampling=1, volumeRamp=0
   //   01-15 03:25:55.713  1809  1844 V AudioMixer: mixer configuration change: 1 all16BitsStereoNoResample=0, resampling=1, volumeRamp=1
    ALOGV("mixer configuration change: %zu " "all16BitsStereoNoResample=%d, resampling=%d, volumeRamp=%d",mEnabled.size(), all16BitsStereoNoResample, resampling, volumeRamp);

   process();  //mHook = &AudioMixer::process__genericResampling;

    // Now that the volume ramp has been done, set optimal state and
    // track hooks for subsequent mixer process
    if (mEnabled.size() > 0) {
        bool allMuted = true;

        for (const int name : mEnabled) {
            const std::shared_ptr<Track> &t = mTracks[name];
            if (!t->doesResample() && t->volumeRL == 0) {
                t->needs |= NEEDS_MUTE;
                t->hook = &Track::track__nop;
            } else {
                allMuted = false;
            }
        }


        if (allMuted) {
            mHook = &AudioMixer::process__nop;
        } else if (all16BitsStereoNoResample) {
            if (mEnabled.size() == 1) {
                //const int i = 31 - __builtin_clz(enabledTracks);
                const std::shared_ptr<Track> &t = mTracks[mEnabled[0]];
                // Muted single tracks handled by allMuted above.
                mHook = getProcessHook(PROCESSTYPE_NORESAMPLEONETRACK,t->mMixerChannelCount, t->mMixerInFormat, t->mMixerFormat);
            }
        }
    }


}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// generic code with resampling
void AudioMixer::process__genericResampling()
{
    ALOGV("process__genericResampling\n");

    int32_t * const outTemp = mOutputTemp.get(); // naked ptr
    size_t numFrames = mFrameCount;

        /**
         * std::unordered_map<void *, std::vector<int>> mGroups;
         * 
         * t->mainBuffer 对应 thread 中 mMixerBuffer 为 key
         * 
        */
    for (const auto &pair : mGroups) {

        const auto &group = pair.second;
        const std::shared_ptr<Track> &t1 = mTracks[group[0]];

        // clear temp buffer
        memset(outTemp, 0, sizeof(*outTemp) * t1->mMixerChannelCount * mFrameCount);

        for (const int name : group) {

            const std::shared_ptr<Track> &t = mTracks[name];
            int32_t *aux = NULL;
            if (CC_UNLIKELY(t->needs & NEEDS_AUX)) {
                aux = t->auxBuffer;
            }

            // this is a little goofy, on the resampling case we don't
            // acquire/release the buffers because it's done by
            // the resampler.
            if (t->needs & NEEDS_RESAMPLE) {
                /***
                 *  
                 * */
                //执行这里   (AudioMixer::hook_t) &Track::track__Resample< MIXTYPE_MULTI, float /*TO*/, float /*TI*/, TYPE_AUX>;
                (t.get()->*t->hook)(outTemp, numFrames, mResampleTemp.get() /* naked ptr */, aux);

            } else {
               。。。。。。
            }
        }//for (const int name : group) {

        /**
         * 将 outTemp 拷贝到 t1->mainBuffer (为thread中的 mMixerBuffer ) 中
        */
        convertMixerFormat(t1->mainBuffer, t1->mMixerFormat, outTemp, t1->mMixerInFormat, numFrames * t1->mMixerChannelCount);

    }
}



/* The Mixer engine generates either int32_t (Q4_27) or float data.
 * We use this function to convert the engine buffers
 * to the desired mixer output format, either int16_t (Q.15) or float.
 */
/* static */
void AudioMixer::convertMixerFormat(void *out, audio_format_t mixerOutFormat,void *in, audio_format_t mixerInFormat, size_t sampleCount)
{
    switch (mixerInFormat) {

    case AUDIO_FORMAT_PCM_FLOAT://0x5 执行这里
        switch (mixerOutFormat) {
        case AUDIO_FORMAT_PCM_FLOAT:
            memcpy(out, in, sampleCount * sizeof(float)); // MEMCPY. TODO optimize out
            break;
        case AUDIO_FORMAT_PCM_16_BIT://0x1 执行这里
        /**
         * system/media/audio_utils/primitives.c
        */
            memcpy_to_i16_from_float((int16_t*)out, (float*)in, sampleCount);
            break;
        default:
            LOG_ALWAYS_FATAL("bad mixerOutFormat: %#x", mixerOutFormat);
            break;
        }
        break;

    case AUDIO_FORMAT_PCM_16_BIT:
        switch (mixerOutFormat) {
        case AUDIO_FORMAT_PCM_FLOAT:
            memcpy_to_float_from_q4_27((float*)out, (const int32_t*)in, sampleCount);
            break;
        case AUDIO_FORMAT_PCM_16_BIT:
            memcpy_to_i16_from_q4_27((int16_t*)out, (const int32_t*)in, sampleCount);
            break;
        default:
            LOG_ALWAYS_FATAL("bad mixerOutFormat: %#x", mixerOutFormat);
            break;
        }
        break;
    default:
        LOG_ALWAYS_FATAL("bad mixerInFormat: %#x", mixerInFormat);
        break;
    }
}