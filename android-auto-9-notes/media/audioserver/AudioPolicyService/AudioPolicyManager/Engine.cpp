

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/enginedefault/src/Engine.cpp
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