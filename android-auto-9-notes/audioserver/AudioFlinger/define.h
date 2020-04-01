



typedef uint32_t audio_flags_mask_t;

/* Do not change these values without updating their counterparts
 * in frameworks/base/media/java/android/media/AudioAttributes.java
 */
enum {
    AUDIO_FLAG_NONE                       = 0x0,
    AUDIO_FLAG_AUDIBILITY_ENFORCED        = 0x1,
    AUDIO_FLAG_SECURE                     = 0x2,
    AUDIO_FLAG_SCO                        = 0x4,
    AUDIO_FLAG_BEACON                     = 0x8,
    AUDIO_FLAG_HW_AV_SYNC                 = 0x10,
    AUDIO_FLAG_HW_HOTWORD                 = 0x20,
    AUDIO_FLAG_BYPASS_INTERRUPTION_POLICY = 0x40,
    AUDIO_FLAG_BYPASS_MUTE                = 0x80,
    AUDIO_FLAG_LOW_LATENCY                = 0x100,
    AUDIO_FLAG_DEEP_BUFFER                = 0x200,
};

//  @   /work/workcodes/aosp-p9.x-auto-ga/system/media/audio/include/system/audio-base.h
typedef enum {
    AUDIO_STREAM_DEFAULT = -1, // (-1)
    AUDIO_STREAM_MIN = 0,
    AUDIO_STREAM_VOICE_CALL = 0,
    AUDIO_STREAM_SYSTEM = 1,
    AUDIO_STREAM_RING = 2,
    AUDIO_STREAM_MUSIC = 3,
    AUDIO_STREAM_ALARM = 4,
    AUDIO_STREAM_NOTIFICATION = 5,
    AUDIO_STREAM_BLUETOOTH_SCO = 6,
    AUDIO_STREAM_ENFORCED_AUDIBLE = 7,
    AUDIO_STREAM_DTMF = 8,
    AUDIO_STREAM_TTS = 9,
    AUDIO_STREAM_ACCESSIBILITY = 10,
#ifndef AUDIO_NO_SYSTEM_DECLARATIONS
    /** For dynamic policy output mixes. Only used by the audio policy */
    AUDIO_STREAM_REROUTING = 11,//rerouting
    /** For audio flinger tracks volume. Only used by the audioflinger */
    AUDIO_STREAM_PATCH = 12,
#endif // AUDIO_NO_SYSTEM_DECLARATIONS
} audio_stream_type_t;

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

typedef enum {
    AUDIO_CONTENT_TYPE_UNKNOWN = 0u,
    AUDIO_CONTENT_TYPE_SPEECH = 1u,
    AUDIO_CONTENT_TYPE_MUSIC = 2u,
    AUDIO_CONTENT_TYPE_MOVIE = 3u,
    AUDIO_CONTENT_TYPE_SONIFICATION = 4u,
} audio_content_type_t;


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

/* Audio attributes */
#define AUDIO_ATTRIBUTES_TAGS_MAX_SIZE 256
typedef struct {
    audio_content_type_t content_type;//    
    audio_usage_t        usage;         //  
    audio_source_t       source;        //  
    audio_flags_mask_t   flags;
    char                 tags[AUDIO_ATTRIBUTES_TAGS_MAX_SIZE]; /* UTF8 */
} __attribute__((packed)) audio_attributes_t; // sent through Binder;


