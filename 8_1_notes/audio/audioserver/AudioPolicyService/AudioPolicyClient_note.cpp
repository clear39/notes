//	@frameworks/av/services/audiopolicy/AudioPolicyInterface.h
class AudioPolicyClientInterface{}

//	@frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp
class AudioPolicyClient : public AudioPolicyClientInterface{

	public:
        explicit AudioPolicyClient(AudioPolicyService *service) : mAudioPolicyService(service) {}

}



