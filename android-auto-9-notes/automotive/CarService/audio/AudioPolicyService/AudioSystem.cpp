

//  @   



/*static*/ void AudioSystem::setDynPolicyCallback(dynamic_policy_callback cb)
{
    Mutex::Autolock _l(gLock);
    gDynPolicyCallback = cb;
}

void AudioSystem::AudioPolicyServiceClient::onDynamicPolicyMixStateUpdate(String8 regId, int32_t state)
{
    ALOGV("AudioPolicyServiceClient::onDynamicPolicyMixStateUpdate(%s, %d)", regId.string(), state);
    dynamic_policy_callback cb = NULL;
    {
        Mutex::Autolock _l(AudioSystem::gLock);
        cb = gDynPolicyCallback;
    }

    if (cb != NULL) {
        cb(DYNAMIC_POLICY_EVENT_MIX_STATE_UPDATE, regId, state);
    }
}