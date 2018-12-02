//	@frameworks/av/services/audiopolicy/service/AudioPolicyService.h
class AudioCommandThread : public Thread {

	// commands for tone AudioCommand
	enum {
	    START_TONE,
	    STOP_TONE,
	    SET_VOLUME,
	    SET_PARAMETERS,
	    SET_VOICE_VOLUME,
	    STOP_OUTPUT,
	    RELEASE_OUTPUT,
	    CREATE_AUDIO_PATCH,
	    RELEASE_AUDIO_PATCH,
	    UPDATE_AUDIOPORT_LIST,
	    UPDATE_AUDIOPATCH_LIST,
	    SET_AUDIOPORT_CONFIG,
	    DYN_POLICY_MIX_STATE_UPDATE,
	    RECORDING_CONFIGURATION_UPDATE
	};

	
}



