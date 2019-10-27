

routing_strategy Engine::getStrategyForStream(audio_stream_type_t stream)
{
    // stream to strategy mapping
    switch (stream) {
    case AUDIO_STREAM_VOICE_CALL:
    case AUDIO_STREAM_BLUETOOTH_SCO:
        return STRATEGY_PHONE;
    case AUDIO_STREAM_RING:
    case AUDIO_STREAM_ALARM:
        return STRATEGY_SONIFICATION;
    case AUDIO_STREAM_NOTIFICATION:
        return STRATEGY_SONIFICATION_RESPECTFUL;
    case AUDIO_STREAM_DTMF:
        return STRATEGY_DTMF;
    default:
        ALOGE("unknown stream type %d", stream);
    case AUDIO_STREAM_SYSTEM:
        // NOTE: SYSTEM stream uses MEDIA strategy because muting music and switching outputs
        // while key clicks are played produces a poor result
    case AUDIO_STREAM_MUSIC:
        return STRATEGY_MEDIA;
    case AUDIO_STREAM_ENFORCED_AUDIBLE:
        return STRATEGY_ENFORCED_AUDIBLE;
    case AUDIO_STREAM_TTS:
        return STRATEGY_TRANSMITTED_THROUGH_SPEAKER;
    case AUDIO_STREAM_ACCESSIBILITY:
        return STRATEGY_ACCESSIBILITY;
    case AUDIO_STREAM_REROUTING:
        return STRATEGY_REROUTING;
    }
}


routing_strategy Engine::getStrategyForUsage(audio_usage_t usage)
{
    // usage to strategy mapping
    switch (usage) {
    case AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY:
        return STRATEGY_ACCESSIBILITY;

    case AUDIO_USAGE_MEDIA:
    case AUDIO_USAGE_GAME:
    case AUDIO_USAGE_ASSISTANT:
    case AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE:
    case AUDIO_USAGE_ASSISTANCE_SONIFICATION:
        return STRATEGY_MEDIA;

    case AUDIO_USAGE_VOICE_COMMUNICATION:
        return STRATEGY_PHONE;

    case AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING:
        return STRATEGY_DTMF;

    case AUDIO_USAGE_ALARM:
    case AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE:
        return STRATEGY_SONIFICATION;

    case AUDIO_USAGE_NOTIFICATION:
    case AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST:
    case AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT:
    case AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED:
    case AUDIO_USAGE_NOTIFICATION_EVENT:
        return STRATEGY_SONIFICATION_RESPECTFUL;

    case AUDIO_USAGE_UNKNOWN:
    default:
        return STRATEGY_MEDIA;
    }
}