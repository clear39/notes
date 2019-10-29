

//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyMix.cpp
/**
 * 
 * status_t AudioPolicyManager::registerPolicyMixes(const Vector<AudioMix>& mixes)
 * ---> 
 * **/
status_t AudioPolicyMixCollection::registerMix(const String8& address, AudioMix mix,sp<SwAudioOutputDescriptor> desc)
{
    ssize_t index = indexOfKey(address);
    if (index >= 0) {
        ALOGE("registerPolicyMixes(): mix for address %s already registered", address.string());
        return BAD_VALUE;
    }
    sp<AudioPolicyMix> policyMix = new AudioPolicyMix();
    policyMix->setMix(mix);
    add(address, policyMix);

    if (desc != 0) {
        desc->mPolicyMix = policyMix->getMix();
        policyMix->setOutput(desc);
    }
    return NO_ERROR;
}