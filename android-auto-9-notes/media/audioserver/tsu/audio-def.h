/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   system/media/audio/include/system/audio.h
static inline audio_channel_mask_t audio_channel_in_mask_from_count(uint32_t channel_count)
{
    uint32_t bits;
    switch (channel_count) {
    case 0:
        return AUDIO_CHANNEL_NONE;
    case 1:
        bits = AUDIO_CHANNEL_IN_MONO;
        break;
    case 2:
        bits = AUDIO_CHANNEL_IN_STEREO; //  system/media/audio/include/system/audio-base.h:254:    AUDIO_CHANNEL_IN_STEREO                 = 0xCu,      // IN_LEFT | IN_RIGHT
        break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        // FIXME FCC_8
        return audio_channel_mask_for_index_assignment_from_count(channel_count);
    default:
        return AUDIO_CHANNEL_INVALID;
    }
    return audio_channel_mask_from_representation_and_bits(AUDIO_CHANNEL_REPRESENTATION_POSITION, bits);
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   frameworks/av/services/audiopolicy/AudioPolicyInterface.h
typedef enum {
    API_INPUT_INVALID = -1,
    API_INPUT_LEGACY  = 0,// e.g. audio recording from a microphone
    API_INPUT_MIX_CAPTURE,// used for "remote submix", capture of the media to play it remotely
    API_INPUT_MIX_EXT_POLICY_REROUTE,// used for platform audio rerouting, where mixes are
                                        // handled by external and dynamically installed
                                        // policies which reroute audio mixes
    API_INPUT_TELEPHONY_RX, // used for capture from telephony RX path
} input_type_t;


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_SESSION_OUTPUT_STAGE = -1, // (-1)
    AUDIO_SESSION_OUTPUT_MIX = 0,
    AUDIO_SESSION_ALLOCATE = 0,
    AUDIO_SESSION_NONE = 0,
} audio_session_t;


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   system/media/audio/include/system/audio.h

#define AUDIO_ATTRIBUTES_TAGS_MAX_SIZE 256
typedef struct {
    audio_content_type_t content_type;  //  指定播放来源的类型（音乐、电影、语音、发音、未知）。
    audio_usage_t        usage;     //  指定播放来源的原因，并控制导向、焦点和音量决策。
    audio_source_t       source;    //  提取到 Audio HAL 的用法值。
    //指定来源的播放方式。包括对可听性强制执行（一些国家/地区要求发出相机快门提示音）和硬件音频/视频同步的支持。
    audio_flags_mask_t   flags;     //  @   system/media/audio/include/system/audio.h:47:typedef uint32_t audio_flags_mask_t;
    char                 tags[AUDIO_ATTRIBUTES_TAGS_MAX_SIZE]; /* UTF8 */
} __attribute__((packed)) audio_attributes_t; // sent through Binder;


//  @   system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_CONTENT_TYPE_UNKNOWN = 0u,
    AUDIO_CONTENT_TYPE_SPEECH = 1u,
    AUDIO_CONTENT_TYPE_MUSIC = 2u,
    AUDIO_CONTENT_TYPE_MOVIE = 3u,
    AUDIO_CONTENT_TYPE_SONIFICATION = 4u,
} audio_content_type_t;

//  @   system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_USAGE_UNKNOWN = 0,
    AUDIO_USAGE_MEDIA = 1,
    AUDIO_USAGE_VOICE_COMMUNICATION = 2,
    AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING = 3,
    AUDIO_USAGE_ALARM = 4,
    AUDIO_USAGE_NOTIFICATION = 5,
    AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE = 6,
#ifndef AUDIO_NO_SYSTEM_DECLARATIONS
    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST = 7,
    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT = 8,
    AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED = 9,
    AUDIO_USAGE_NOTIFICATION_EVENT = 10,
#endif // AUDIO_NO_SYSTEM_DECLARATIONS
    AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY = 11,
    AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE = 12,
    AUDIO_USAGE_ASSISTANCE_SONIFICATION = 13,
    AUDIO_USAGE_GAME = 14,
    AUDIO_USAGE_VIRTUAL_SOURCE = 15,
    AUDIO_USAGE_ASSISTANT = 16,
} audio_usage_t;

//  @   system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_SOURCE_DEFAULT = 0,
    AUDIO_SOURCE_MIC = 1,
    AUDIO_SOURCE_VOICE_UPLINK = 2,
    AUDIO_SOURCE_VOICE_DOWNLINK = 3,
    AUDIO_SOURCE_VOICE_CALL = 4,
    AUDIO_SOURCE_CAMCORDER = 5,
    AUDIO_SOURCE_VOICE_RECOGNITION = 6,
    AUDIO_SOURCE_VOICE_COMMUNICATION = 7,
    AUDIO_SOURCE_REMOTE_SUBMIX = 8,
    AUDIO_SOURCE_UNPROCESSED = 9,
    AUDIO_SOURCE_FM_TUNER = 1998,
#ifndef AUDIO_NO_SYSTEM_DECLARATIONS
    /**
     * A low-priority, preemptible audio source for for background software
     * hotword detection. Same tuning as VOICE_RECOGNITION.
     * Used only internally by the framework.
     */
    AUDIO_SOURCE_HOTWORD = 1999,
#endif // AUDIO_NO_SYSTEM_DECLARATIONS
} audio_source_t;





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  @   system/media/audio/include/system/audio.h
struct audio_config_base {
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;  //  system/media/audio/include/system/audio.h:137:typedef uint32_t audio_channel_mask_t;
    audio_format_t  format;
};

//  @   system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_FORMAT_INVALID             = 0xFFFFFFFFu,
    AUDIO_FORMAT_DEFAULT             = 0,
    AUDIO_FORMAT_PCM                 = 0x00000000u,
    AUDIO_FORMAT_MP3                 = 0x01000000u,
    AUDIO_FORMAT_AMR_NB              = 0x02000000u,
    AUDIO_FORMAT_AMR_WB              = 0x03000000u,
    AUDIO_FORMAT_AAC                 = 0x04000000u,
    AUDIO_FORMAT_HE_AAC_V1           = 0x05000000u,
    AUDIO_FORMAT_HE_AAC_V2           = 0x06000000u,
    AUDIO_FORMAT_VORBIS              = 0x07000000u,
    AUDIO_FORMAT_OPUS                = 0x08000000u,
    AUDIO_FORMAT_AC3                 = 0x09000000u,
    AUDIO_FORMAT_E_AC3               = 0x0A000000u,
    AUDIO_FORMAT_DTS                 = 0x0B000000u,
    AUDIO_FORMAT_DTS_HD              = 0x0C000000u,
    AUDIO_FORMAT_IEC61937            = 0x0D000000u,
    AUDIO_FORMAT_DOLBY_TRUEHD        = 0x0E000000u,
    AUDIO_FORMAT_EVRC                = 0x10000000u,
    AUDIO_FORMAT_EVRCB               = 0x11000000u,
    AUDIO_FORMAT_EVRCWB              = 0x12000000u,
    AUDIO_FORMAT_EVRCNW              = 0x13000000u,
    AUDIO_FORMAT_AAC_ADIF            = 0x14000000u,
    AUDIO_FORMAT_WMA                 = 0x15000000u,
    AUDIO_FORMAT_WMA_PRO             = 0x16000000u,
    AUDIO_FORMAT_AMR_WB_PLUS         = 0x17000000u,
    AUDIO_FORMAT_MP2                 = 0x18000000u,
    AUDIO_FORMAT_QCELP               = 0x19000000u,
    AUDIO_FORMAT_DSD                 = 0x1A000000u,
    AUDIO_FORMAT_FLAC                = 0x1B000000u,
    AUDIO_FORMAT_ALAC                = 0x1C000000u,
    AUDIO_FORMAT_APE                 = 0x1D000000u,
    AUDIO_FORMAT_AAC_ADTS            = 0x1E000000u,
    AUDIO_FORMAT_SBC                 = 0x1F000000u,
    AUDIO_FORMAT_APTX                = 0x20000000u,
    AUDIO_FORMAT_APTX_HD             = 0x21000000u,
    AUDIO_FORMAT_AC4                 = 0x22000000u,
    AUDIO_FORMAT_LDAC                = 0x23000000u,
    AUDIO_FORMAT_MAT                 = 0x24000000u,
    AUDIO_FORMAT_MAIN_MASK           = 0xFF000000u,
    AUDIO_FORMAT_SUB_MASK            = 0x00FFFFFFu,

    /* Subformats */
    AUDIO_FORMAT_PCM_SUB_16_BIT        = 0x1u,
    AUDIO_FORMAT_PCM_SUB_8_BIT         = 0x2u,
    AUDIO_FORMAT_PCM_SUB_32_BIT        = 0x3u,
    AUDIO_FORMAT_PCM_SUB_8_24_BIT      = 0x4u,
    AUDIO_FORMAT_PCM_SUB_FLOAT         = 0x5u,
    AUDIO_FORMAT_PCM_SUB_24_BIT_PACKED = 0x6u,
 AUDIO_FORMAT_AAC_SUB_MAIN          = 0x1u,
    AUDIO_FORMAT_AAC_SUB_LC            = 0x2u,
    AUDIO_FORMAT_AAC_SUB_SSR           = 0x4u,
    AUDIO_FORMAT_AAC_SUB_LTP           = 0x8u,
    AUDIO_FORMAT_AAC_SUB_HE_V1         = 0x10u,
    AUDIO_FORMAT_AAC_SUB_SCALABLE      = 0x20u,
    AUDIO_FORMAT_AAC_SUB_ERLC          = 0x40u,
    AUDIO_FORMAT_AAC_SUB_LD            = 0x80u,
    AUDIO_FORMAT_AAC_SUB_HE_V2         = 0x100u,
    AUDIO_FORMAT_AAC_SUB_ELD           = 0x200u,
    AUDIO_FORMAT_AAC_SUB_XHE           = 0x300u,

    AUDIO_FORMAT_VORBIS_SUB_NONE       = 0x0u,

    AUDIO_FORMAT_E_AC3_SUB_JOC         = 0x1u,

    AUDIO_FORMAT_MAT_SUB_1_0           = 0x1u,
    AUDIO_FORMAT_MAT_SUB_2_0           = 0x2u,
    AUDIO_FORMAT_MAT_SUB_2_1           = 0x3u,

    /* Aliases */
    AUDIO_FORMAT_PCM_16_BIT            = 0x1u,        // (PCM | PCM_SUB_16_BIT)
    AUDIO_FORMAT_PCM_8_BIT             = 0x2u,        // (PCM | PCM_SUB_8_BIT)
    AUDIO_FORMAT_PCM_32_BIT            = 0x3u,        // (PCM | PCM_SUB_32_BIT)
    AUDIO_FORMAT_PCM_8_24_BIT          = 0x4u,        // (PCM | PCM_SUB_8_24_BIT)
    AUDIO_FORMAT_PCM_FLOAT             = 0x5u,        // (PCM | PCM_SUB_FLOAT)
    AUDIO_FORMAT_PCM_24_BIT_PACKED     = 0x6u,        // (PCM | PCM_SUB_24_BIT_PACKED)
    AUDIO_FORMAT_AAC_MAIN              = 0x4000001u,  // (AAC | AAC_SUB_MAIN)
    AUDIO_FORMAT_AAC_LC                = 0x4000002u,  // (AAC | AAC_SUB_LC)
    AUDIO_FORMAT_AAC_SSR               = 0x4000004u,  // (AAC | AAC_SUB_SSR)
    AUDIO_FORMAT_AAC_LTP               = 0x4000008u,  // (AAC | AAC_SUB_LTP)
    AUDIO_FORMAT_AAC_HE_V1             = 0x4000010u,  // (AAC | AAC_SUB_HE_V1)
    AUDIO_FORMAT_AAC_SCALABLE          = 0x4000020u,  // (AAC | AAC_SUB_SCALABLE)
    AUDIO_FORMAT_AAC_ERLC              = 0x4000040u,  // (AAC | AAC_SUB_ERLC)
    AUDIO_FORMAT_AAC_LD                = 0x4000080u,  // (AAC | AAC_SUB_LD)
    AUDIO_FORMAT_AAC_HE_V2             = 0x4000100u,  // (AAC | AAC_SUB_HE_V2)
    AUDIO_FORMAT_AAC_ELD               = 0x4000200u,  // (AAC | AAC_SUB_ELD)
    AUDIO_FORMAT_AAC_XHE               = 0x4000300u,  // (AAC | AAC_SUB_XHE)
    AUDIO_FORMAT_AAC_ADTS_MAIN         = 0x1e000001u, // (AAC_ADTS | AAC_SUB_MAIN)
    AUDIO_FORMAT_AAC_ADTS_LC           = 0x1e000002u, // (AAC_ADTS | AAC_SUB_LC)
    AUDIO_FORMAT_AAC_ADTS_SSR          = 0x1e000004u, // (AAC_ADTS | AAC_SUB_SSR)
    AUDIO_FORMAT_AAC_ADTS_LTP          = 0x1e000008u, // (AAC_ADTS | AAC_SUB_LTP)
    AUDIO_FORMAT_AAC_ADTS_HE_V1        = 0x1e000010u, // (AAC_ADTS | AAC_SUB_HE_V1)
    AUDIO_FORMAT_AAC_ADTS_SCALABLE     = 0x1e000020u, // (AAC_ADTS | AAC_SUB_SCALABLE)
    AUDIO_FORMAT_AAC_ADTS_ERLC         = 0x1e000040u, // (AAC_ADTS | AAC_SUB_ERLC)
    AUDIO_FORMAT_AAC_ADTS_LD           = 0x1e000080u, // (AAC_ADTS | AAC_SUB_LD)
    AUDIO_FORMAT_AAC_ADTS_HE_V2        = 0x1e000100u, // (AAC_ADTS | AAC_SUB_HE_V2)
    AUDIO_FORMAT_AAC_ADTS_ELD          = 0x1e000200u, // (AAC_ADTS | AAC_SUB_ELD)
    AUDIO_FORMAT_AAC_ADTS_XHE          = 0x1e000300u, // (AAC_ADTS | AAC_SUB_XHE)
    AUDIO_FORMAT_E_AC3_JOC             = 0xA000001u,  // (E_AC3 | E_AC3_SUB_JOC)
    AUDIO_FORMAT_MAT_1_0               = 0x24000001u, // (MAT | MAT_SUB_1_0)
    AUDIO_FORMAT_MAT_2_0               = 0x24000002u, // (MAT | MAT_SUB_2_0)
    AUDIO_FORMAT_MAT_2_1               = 0x24000003u, // (MAT | MAT_SUB_2_1)
} audio_format_t;
