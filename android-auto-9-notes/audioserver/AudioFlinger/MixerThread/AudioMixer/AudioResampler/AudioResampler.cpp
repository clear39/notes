


AudioResampler* AudioResampler::create(audio_format_t format, int inChannelCount,int32_t sampleRate, src_quality quality) 
{
    bool atFinalQuality;
    if (quality == DEFAULT_QUALITY) { //        DEFAULT_QUALITY=0
        // read the resampler default quality property the first time it is needed
        int ok = pthread_once(&once_control, init_routine);
        if (ok != 0) {
            ALOGE("%s pthread_once failed: %d", __func__, ok);
        }
        quality = defaultQuality;  // 0
        atFinalQuality = false;
    } else {
        atFinalQuality = true;
    }

    /* if the caller requests DEFAULT_QUALITY and af.resampler.property
     * has not been set, the target resampler quality is set to DYN_MED_QUALITY,
     * and allowed to "throttle" down to DYN_LOW_QUALITY if necessary
     * due to estimated CPU load of having too many active resamplers
     * (the code below the if).
     */
    if (quality == DEFAULT_QUALITY) {
        quality = DYN_MED_QUALITY;  // 6
    }

    // naive implementation of CPU load throttling doesn't account for whether resampler is active
    pthread_mutex_lock(&mutex);
    for (;;) {
        uint32_t deltaMHz = qualityMHz(quality);  // 6
        uint32_t newMHz = currentMHz + deltaMHz;// 0 + 6
        if ((qualityIsSupported(quality) && newMHz <= maxMHz /*130*/) || atFinalQuality) {
            ALOGV("resampler load %u -> %u MHz due to delta +%u MHz from quality %d",currentMHz, newMHz, deltaMHz, quality);
            currentMHz = newMHz;//6
            break;
        }
        // not enough CPU available for proposed quality level, so try next lowest level
        switch (quality) {
        default:
        case LOW_QUALITY:
            atFinalQuality = true;
            break;
        case MED_QUALITY:
            quality = LOW_QUALITY;
            break;
        case HIGH_QUALITY:
            quality = MED_QUALITY;
            break;
        case VERY_HIGH_QUALITY:
            quality = HIGH_QUALITY;
            break;
        case DYN_LOW_QUALITY:
            atFinalQuality = true;
            break;
        case DYN_MED_QUALITY:
            quality = DYN_LOW_QUALITY;
            break;
        case DYN_HIGH_QUALITY:
            quality = DYN_MED_QUALITY;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);

    AudioResampler* resampler;

    switch (quality) {
    default:
    case LOW_QUALITY:
        ALOGV("Create linear Resampler");
        LOG_ALWAYS_FATAL_IF(format != AUDIO_FORMAT_PCM_16_BIT);
        resampler = new AudioResamplerOrder1(inChannelCount, sampleRate);
        break;
    case MED_QUALITY:
        ALOGV("Create cubic Resampler");
        LOG_ALWAYS_FATAL_IF(format != AUDIO_FORMAT_PCM_16_BIT);
        resampler = new AudioResamplerCubic(inChannelCount, sampleRate);
        break;
    case HIGH_QUALITY:
        ALOGV("Create HIGH_QUALITY sinc Resampler");
        LOG_ALWAYS_FATAL_IF(format != AUDIO_FORMAT_PCM_16_BIT);
        resampler = new AudioResamplerSinc(inChannelCount, sampleRate);
        break;
    case VERY_HIGH_QUALITY:
        ALOGV("Create VERY_HIGH_QUALITY sinc Resampler = %d", quality);
        LOG_ALWAYS_FATAL_IF(format != AUDIO_FORMAT_PCM_16_BIT);
        resampler = new AudioResamplerSinc(inChannelCount, sampleRate, quality);
        break;
    case DYN_LOW_QUALITY:
    case DYN_MED_QUALITY://这里
    case DYN_HIGH_QUALITY:
        ALOGV("Create dynamic Resampler = %d", quality);
        if (format == AUDIO_FORMAT_PCM_FLOAT) {//这里
            resampler = new AudioResamplerDyn<float, float, float>(inChannelCount,sampleRate, quality);
        } else {
            LOG_ALWAYS_FATAL_IF(format != AUDIO_FORMAT_PCM_16_BIT);
            if (quality == DYN_HIGH_QUALITY) {
                resampler = new AudioResamplerDyn<int32_t, int16_t, int32_t>(inChannelCount,sampleRate, quality);
            } else {
                resampler = new AudioResamplerDyn<int16_t, int16_t, int32_t>(inChannelCount,sampleRate, quality);
            }
        }
        break;
    }

    // initialize resampler
    resampler->init();
    return resampler;
}


void AudioResampler::init_routine()
{
    char value[PROPERTY_VALUE_MAX];
    /**
     * 目前 af.resampler.quality 没有设置值
    */
    if (property_get("af.resampler.quality", value, NULL) > 0) {
        char *endptr;
        unsigned long l = strtoul(value, &endptr, 0);
        if (*endptr == '\0') {
            defaultQuality = (src_quality) l;
            ALOGD("forcing AudioResampler quality to %d", defaultQuality);
            if (defaultQuality < DEFAULT_QUALITY || defaultQuality > DYN_HIGH_QUALITY) {
                defaultQuality = DEFAULT_QUALITY;
            }
        }
    }
}