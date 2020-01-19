

//      @       system/media/audio/include/system/audio-base.h

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

//      @       system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_PORT_ROLE_NONE = 0,
    AUDIO_PORT_ROLE_SOURCE = 1, // (::android::hardware::audio::common::V4_0::AudioPortRole.NONE implicitly + 1)
    AUDIO_PORT_ROLE_SINK = 2, // (::android::hardware::audio::common::V4_0::AudioPortRole.SOURCE implicitly + 1)
} audio_port_role_t;


//      @       system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_PORT_TYPE_NONE = 0,
    AUDIO_PORT_TYPE_DEVICE = 1, // (::android::hardware::audio::common::V4_0::AudioPortType.NONE implicitly + 1)
    AUDIO_PORT_TYPE_MIX = 2, // (::android::hardware::audio::common::V4_0::AudioPortType.DEVICE implicitly + 1)
    AUDIO_PORT_TYPE_SESSION = 3, // (::android::hardware::audio::common::V4_0::AudioPortType.MIX implicitly + 1)
} audio_port_type_t;




/**
 * @    system/media/audio/include/system/audio.h
*/
struct audio_port {
    audio_port_handle_t      id;                /* port unique ID */
    audio_port_role_t        role;              /* sink or source */
    audio_port_type_t        type;              /* device, mix ... */
    char                     name[AUDIO_PORT_MAX_NAME_LEN];
    unsigned int             num_sample_rates;  /* number of sampling rates in following array */
    unsigned int             sample_rates[AUDIO_PORT_MAX_SAMPLING_RATES];
    unsigned int             num_channel_masks; /* number of channel masks in following array */
    audio_channel_mask_t     channel_masks[AUDIO_PORT_MAX_CHANNEL_MASKS];
    unsigned int             num_formats;       /* number of formats in following array */
    audio_format_t           formats[AUDIO_PORT_MAX_FORMATS];
    unsigned int             num_gains;         /* number of gains in following array */
    struct audio_gain        gains[AUDIO_PORT_MAX_GAINS];
    struct audio_port_config active_config;     /* current audio port configuration */
    union {
        struct audio_port_device_ext  device;
        struct audio_port_mix_ext     mix;
        struct audio_port_session_ext session;
    } ext;
};





/**
 * @    frameworks/av/services/audiopolicy/common/include/Volume.h
*/
/**
 * device categories used for volume curve management.
 */
enum device_category {
    DEVICE_CATEGORY_HEADSET,
    DEVICE_CATEGORY_SPEAKER,
    DEVICE_CATEGORY_EARPIECE,
    DEVICE_CATEGORY_EXT_MEDIA,
    DEVICE_CATEGORY_HEARING_AID,
    DEVICE_CATEGORY_CNT
};



/**
 * @    system/media/audio/include/system/audio.h
*/
/* An audio patch represents a connection between one or more source ports and
 * one or more sink ports. Patches are connected and disconnected by audio policy manager or by
 * applications via framework APIs.
 * Each patch is identified by a handle at the interface used to create that patch. For instance,
 * when a patch is created by the audio HAL, the HAL allocates and returns a handle.
 * This handle is unique to a given audio HAL hardware module.
 * But the same patch receives another system wide unique handle allocated by the framework.
 * This unique handle is used for all transactions inside the framework.
 */
typedef int audio_patch_handle_t;

#define AUDIO_PATCH_PORTS_MAX   16

struct audio_patch {
    audio_patch_handle_t id;            /* patch unique ID */
    unsigned int      num_sources;      /* number of sources in following array */
    struct audio_port_config sources[AUDIO_PATCH_PORTS_MAX];
    unsigned int      num_sinks;        /* number of sinks in following array */
    struct audio_port_config sinks[AUDIO_PATCH_PORTS_MAX];
};


/**
 * audio port configuration structure used to specify a particular configuration of an audio port 
 * 
 * system/media/audio/include/system/audio.h
 * 
 * */
struct audio_port_config {
    audio_port_handle_t      id;           /* port unique ID */
    audio_port_role_t        role;         /* sink or source */
    audio_port_type_t        type;         /* device, mix ... */
    unsigned int             config_mask;  /* e.g AUDIO_PORT_CONFIG_ALL */
    unsigned int             sample_rate;  /* sampling rate in Hz */
    audio_channel_mask_t     channel_mask; /* channel mask if applicable */
    audio_format_t           format;       /* format if applicable */
    struct audio_gain_config gain;         /* gain to apply if applicable */
    union {
        struct audio_port_config_device_ext  device;  /* device specific info */
        struct audio_port_config_mix_ext     mix;     /* mix specific info */
        struct audio_port_config_session_ext session; /* session specific info */
    } ext;
};

