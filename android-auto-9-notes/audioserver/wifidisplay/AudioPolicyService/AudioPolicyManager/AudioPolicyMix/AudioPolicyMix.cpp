//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPolicyMix.h
class AudioPolicyMixCollection : public DefaultKeyedVector<String8, sp<AudioPolicyMix> >{}



//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyMix.cpp
/**
 * 
 * 
*/
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

/**
 * 
 * 
*/
status_t AudioPolicyMixCollection::unregisterMix(const String8& address)
{
    ssize_t index = indexOfKey(address);
    if (index < 0) {
        ALOGE("unregisterPolicyMixes(): mix for address %s not registered", address.string());
        return BAD_VALUE;
    }

    removeItemsAt(index);
    return NO_ERROR;
}


/**
 * IAudioFlinger::createRecord
 * --> AudioFlinger::createRecord
 * ---> AudioSystem::getInputForAttr
 * ----> AudioPolicyManager::getInputForAttr
 * -----> AudioPolicyManager::getDeviceAndMixForInputSource
*/
audio_devices_t AudioPolicyMixCollection::getDeviceAndMixForInputSource(audio_source_t inputSource,
                                    audio_devices_t availDevices,AudioMix **policyMix)
{
    for (size_t i = 0; i < size(); i++) {
        AudioMix *mix = valueAt(i)->getMix();

        if (mix->mMixType != MIX_TYPE_RECORDERS) {
            continue;
        }

        for (size_t j = 0; j < mix->mCriteria.size(); j++) {
            if ((RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET == mix->mCriteria[j].mRule && mix->mCriteria[j].mValue.mSource == inputSource) ||
               (RULE_EXCLUDE_ATTRIBUTE_CAPTURE_PRESET == mix->mCriteria[j].mRule && mix->mCriteria[j].mValue.mSource != inputSource)) {
                /**
                 * availDevices = 0x104
                 * 
                 * system/media/audio/include/system/audio-base.h:337:    AUDIO_DEVICE_IN_REMOTE_SUBMIX              = 0x80000100u, // BIT_IN | 0x100
                */
                if (availDevices & AUDIO_DEVICE_IN_REMOTE_SUBMIX) {
                    if (policyMix != NULL) {
                        *policyMix = mix;
                    }
                    ALOGV("getDeviceAndMixForInputSource %p",policyMix);
                    return AUDIO_DEVICE_IN_REMOTE_SUBMIX;
                }
                break;
            }
        }
    }
    return AUDIO_DEVICE_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/***
 * 
 * AudioFlinger::createRecord 
 * ---> AudioPolicyService::getInputForAttr
 * ----> AudioPolicyManager::getInputForAttr
 * 
 * */
status_t AudioPolicyMixCollection::getInputMixForAttr(audio_attributes_t attr, AudioMix **policyMix)
{
    if (strncmp(attr.tags, "addr=", strlen("addr=")) != 0) {
        return BAD_VALUE;
    }
    String8 address(attr.tags + strlen("addr="));

#ifdef LOG_NDEBUG
    ALOGV("getInputMixForAttr looking for address %s\n  mixes available:", address.string());
    for (size_t i = 0; i < size(); i++) {
            sp<AudioPolicyMix> policyMix = valueAt(i);
            AudioMix *mix = policyMix->getMix();
            ALOGV("\tmix %zu address=%s", i, mix->mDeviceAddress.string());
    }
#endif

    ssize_t index = indexOfKey(address);
    if (index < 0) {
        ALOGW("getInputMixForAttr() no policy for address %s", address.string());
        return BAD_VALUE;
    }
    sp<AudioPolicyMix> audioPolicyMix = valueAt(index);
    AudioMix *mix = audioPolicyMix->getMix();

    if (mix->mMixType != MIX_TYPE_PLAYERS) {
        ALOGW("getInputMixForAttr() bad policy mix type for address %s", address.string());
        return BAD_VALUE;
    }
    *policyMix = mix;
    return NO_ERROR;
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/***
 * 
 * AudioFlinger::createTrack 
 * ---> AudioPolicyService::getOutputForAttr
 * ----> AudioPolicyManager::getOutputForAttr
 * 
 * */
status_t AudioPolicyMixCollection::getOutputForAttr(audio_attributes_t attributes, uid_t uid,sp<SwAudioOutputDescriptor> &desc)
{
    ALOGV("getOutputForAttr() querying %zu mixes:", size());
    desc = 0;
    for (size_t i = 0; i < size(); i++) {
        sp<AudioPolicyMix> policyMix = valueAt(i);
        AudioMix *mix = policyMix->getMix();

        if (mix->mMixType == MIX_TYPE_PLAYERS) {
            // TODO if adding more player rules (currently only 2), make rule handling "generic"
            //      as there is no difference in the treatment of usage- or uid-based rules
            bool hasUsageMatchRules = false;
            bool hasUsageExcludeRules = false;
            bool usageMatchFound = false;
            bool usageExclusionFound = false;

            bool hasUidMatchRules = false;
            bool hasUidExcludeRules = false;
            bool uidMatchFound = false;
            bool uidExclusionFound = false;

            bool hasAddrMatch = false;

            // iterate over all mix criteria to list what rules this mix contains
            for (size_t j = 0; j < mix->mCriteria.size(); j++) {
                ALOGV(" getOutputForAttr: mix %zu: inspecting mix criteria %zu of %zu",i, j, mix->mCriteria.size());

                // if there is an address match, prioritize that match
                if (strncmp(attributes.tags, "addr=", strlen("addr=")) == 0 &&
                        strncmp(attributes.tags + strlen("addr="),
                                mix->mDeviceAddress.string(),
                                AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - strlen("addr=") - 1) == 0) {
                    hasAddrMatch = true;
                    break;
                }

                switch (mix->mCriteria[j].mRule) {
                case RULE_MATCH_ATTRIBUTE_USAGE:
                    ALOGV("\tmix has RULE_MATCH_ATTRIBUTE_USAGE for usage %d",mix->mCriteria[j].mValue.mUsage);
                    hasUsageMatchRules = true;
                    if (mix->mCriteria[j].mValue.mUsage == attributes.usage) {
                        // found one match against all allowed usages
                        usageMatchFound = true;
                    }
                    break;
                case RULE_EXCLUDE_ATTRIBUTE_USAGE:
                    ALOGV("\tmix has RULE_EXCLUDE_ATTRIBUTE_USAGE for usage %d",mix->mCriteria[j].mValue.mUsage);
                    hasUsageExcludeRules = true;
                    if (mix->mCriteria[j].mValue.mUsage == attributes.usage) {
                        // found this usage is to be excluded
                        usageExclusionFound = true;
                    }
                    break;
                case RULE_MATCH_UID:
                    ALOGV("\tmix has RULE_MATCH_UID for uid %d", mix->mCriteria[j].mValue.mUid);
                    hasUidMatchRules = true;
                    if (mix->mCriteria[j].mValue.mUid == uid) {
                        // found one UID match against all allowed UIDs
                        uidMatchFound = true;
                    }
                    break;
                case RULE_EXCLUDE_UID:
                    ALOGV("\tmix has RULE_EXCLUDE_UID for uid %d", mix->mCriteria[j].mValue.mUid);
                    hasUidExcludeRules = true;
                    if (mix->mCriteria[j].mValue.mUid == uid) {
                        // found this UID is to be excluded
                        uidExclusionFound = true;
                    }
                    break;
                default:
                    break;
                }

                // consistency checks: for each "dimension" of rules (usage, uid...), we can
                // only have MATCH rules, or EXCLUDE rules in each dimension, not a combination
                if (hasUsageMatchRules && hasUsageExcludeRules) {
                    ALOGE("getOutputForAttr: invalid combination of RULE_MATCH_ATTRIBUTE_USAGE"
                            " and RULE_EXCLUDE_ATTRIBUTE_USAGE in mix %zu", i);
                    return BAD_VALUE;
                }
                if (hasUidMatchRules && hasUidExcludeRules) {
                    ALOGE("getOutputForAttr: invalid combination of RULE_MATCH_UID"
                            " and RULE_EXCLUDE_UID in mix %zu", i);
                    return BAD_VALUE;
                }

                if ((hasUsageExcludeRules && usageExclusionFound)
                        || (hasUidExcludeRules && uidExclusionFound)) {
                    break; // stop iterating on criteria because an exclusion was found (will fail)
                }

            }//iterate on mix criteria

            // determine if exiting on success (or implicit failure as desc is 0)
            if (hasAddrMatch ||
                    !((hasUsageExcludeRules && usageExclusionFound) ||
                      (hasUsageMatchRules && !usageMatchFound)  ||
                      (hasUidExcludeRules && uidExclusionFound) ||
                      (hasUidMatchRules && !uidMatchFound))) {
                ALOGV("\tgetOutputForAttr will use mix %zu", i);
                desc = policyMix->getOutput();
            }

        } else if (mix->mMixType == MIX_TYPE_RECORDERS) {
            if (attributes.usage == AUDIO_USAGE_VIRTUAL_SOURCE &&
                    strncmp(attributes.tags, "addr=", strlen("addr=")) == 0 &&
                    strncmp(attributes.tags + strlen("addr="),mix->mDeviceAddress.string(),AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - strlen("addr=") - 1) == 0) {
                desc = policyMix->getOutput();
            }
        }
        if (desc != 0) {
            desc->mPolicyMix = mix;
            return NO_ERROR;
        }
    }
    return BAD_VALUE;
}


