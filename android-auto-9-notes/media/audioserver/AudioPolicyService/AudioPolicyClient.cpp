

// AudioPolicyClientInterface @ /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/AudioPolicyInterface.h
class AudioPolicyClient : public AudioPolicyClientInterface{

}



/***
 * 在 AudioPolicyService::onFirstRef() 中调用创建
 * @    frameworks/av/services/audiopolicy/service/AudioPolicyClientImpl.cpp
 * 
 */
explicit AudioPolicyClient(AudioPolicyService *service) : mAudioPolicyService(service) {

}

/**
 * 
 * 
*/
audio_module_handle_t AudioPolicyService::AudioPolicyClient::loadHwModule(const char *name)
{
    sp<IAudioFlinger> af = AudioSystem::get_audio_flinger();
    if (af == 0) {
        ALOGW("%s: could not get AudioFlinger", __func__);
        return AUDIO_MODULE_HANDLE_NONE;
    }

    return af->loadHwModule(name);
}



