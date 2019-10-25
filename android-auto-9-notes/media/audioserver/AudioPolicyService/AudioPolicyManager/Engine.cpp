

//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/enginedefault/src/EngineInstance.cpp
EngineInstance *EngineInstance::getInstance()
{
    static EngineInstance instance;
    return &instance;
}


EngineInstance::EngineInstance()
{
}


template <>
AudioPolicyManagerInterface *EngineInstance::queryInterface() const
{
    return getEngine()->queryInterface<AudioPolicyManagerInterface>();
}



//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/enginedefault/src/Engine.cpp

/**
 * 
 * AudioPolicyManager::AudioPolicyManager 
 * --> AudioPolicyManager::initialize()
 * ---> audio_policy::EngineInstance::getInstance()
 * ----> EngineInstance::getEngine()
 * 
*/
Engine::Engine()
    : mManagerInterface(this),
      mPhoneState(AUDIO_MODE_NORMAL),
      mApmObserver(NULL)
{
    //  @   system/media/audio/include/system/audio_policy.h
    for (int i = 0; i < AUDIO_POLICY_FORCE_USE_CNT; i++) {
        mForceUse[i] = AUDIO_POLICY_FORCE_NONE;
    }
}

template <>
AudioPolicyManagerInterface *Engine::queryInterface()
{
    return &mManagerInterface; // Engine
}