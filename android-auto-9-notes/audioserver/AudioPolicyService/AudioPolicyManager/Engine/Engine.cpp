
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/enginedefault/src/Engine.cpp


template <>
AudioPolicyManagerInterface *Engine::queryInterface()
{
    /**
     * mManagerInterface 在 Engine::Engine()赋值
     * */
    return &mManagerInterface; // Engine
}


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
    return &mManagerInterface;
}



void Engine::setObserver(AudioPolicyManagerObserver *observer)
{
    ALOG_ASSERT(observer != NULL, "Invalid Audio Policy Manager observer");
    /**
     * mApmObserver 为 AudioPolicyManager
    */
    mApmObserver = observer;
}


status_t Engine::initCheck()
{
    return (mApmObserver != NULL) ?  NO_ERROR : NO_INIT;
}


