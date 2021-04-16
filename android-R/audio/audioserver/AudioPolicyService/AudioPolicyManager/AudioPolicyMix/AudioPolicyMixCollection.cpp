
//	@ frameworks\av\services\audiopolicy\common\managerdefinitions\src\AudioPolicyMix.cpp









status_t AudioPolicyMixCollection::registerMix(AudioMix mix, sp<SwAudioOutputDescriptor> desc)
{
    for (size_t i = 0; i < size(); i++) {
        const sp<AudioPolicyMix>& registeredMix = itemAt(i);
        if (mix.mDeviceType == registeredMix->mDeviceType
                && mix.mDeviceAddress.compare(registeredMix->mDeviceAddress) == 0) {
            ALOGE("registerMix(): mix already registered for dev=0x%x addr=%s",
                    mix.mDeviceType, mix.mDeviceAddress.string());
            return BAD_VALUE;
        }
    }
    sp<AudioPolicyMix> policyMix = new AudioPolicyMix(mix);
    add(policyMix);
    ALOGD("registerMix(): adding mix for dev=0x%x addr=%s",
            policyMix->mDeviceType, policyMix->mDeviceAddress.string());

    if (desc != 0) {
        desc->mPolicyMix = policyMix;
        policyMix->setOutput(desc);
    }
    return NO_ERROR;
}

status_t AudioPolicyMixCollection::unregisterMix(const AudioMix& mix)
{
    for (size_t i = 0; i < size(); i++) {
        const sp<AudioPolicyMix>& registeredMix = itemAt(i);
        if (mix.mDeviceType == registeredMix->mDeviceType
                && mix.mDeviceAddress.compare(registeredMix->mDeviceAddress) == 0) {
            ALOGD("unregisterMix(): removing mix for dev=0x%x addr=%s",
                    mix.mDeviceType, mix.mDeviceAddress.string());
            removeAt(i);
            return NO_ERROR;
        }
    }

    ALOGE("unregisterMix(): mix not registered for dev=0x%x addr=%s",
            mix.mDeviceType, mix.mDeviceAddress.string());
    return BAD_VALUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyMixCollection::getOutputForAttr(
        const audio_attributes_t& attributes, uid_t uid,
        audio_output_flags_t flags,
        sp<AudioPolicyMix> &primaryMix,
        std::vector<sp<AudioPolicyMix>> *secondaryMixes)
{
    ALOGV("getOutputForAttr() querying %zu mixes:", size());
    primaryMix.clear();
    for (size_t i = 0; i < size(); i++) {
        sp<AudioPolicyMix> policyMix = itemAt(i);
        // frameworks/av/media/libaudioclient/include/media/AudioPolicy.h
        const bool primaryOutputMix = !is_mix_loopback_render(policyMix->mRouteFlags);
        if (!primaryOutputMix && (flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) {
            // AAudio does not support MMAP_NO_IRQ loopback render, and there is no way with
            // the current MmapStreamInterface::start to reject a specific client added to a shared
            // mmap stream.
            // As a result all MMAP_NOIRQ requests have to be rejected when an loopback render
            // policy is present. That ensures no shared mmap stream is used when an loopback
            // render policy is registered.
            ALOGD("%s: Rejecting MMAP_NOIRQ request due to LOOPBACK|RENDER mix present.", __func__);
            return INVALID_OPERATION;
        }

        if (primaryOutputMix && primaryMix != nullptr) {
            ALOGV("%s: Skiping %zu: Primary output already found", __func__, i);
            continue; // Primary output already found
        }

        switch (mixMatch(policyMix.get(), i, attributes, uid)) {
            case MixMatchStatus::INVALID_MIX:
                // The mix has contradictory rules, ignore it
                // TODO: reject invalid mix at registration
                continue;
            case MixMatchStatus::NO_MATCH:
                ALOGV("%s: Mix %zu: does not match", __func__, i);
                continue; // skip the mix
            case MixMatchStatus::MATCH:;
        }

        if (primaryOutputMix) {
            primaryMix = policyMix;
            ALOGV("%s: Mix %zu: set primary desc", __func__, i);
        } else {
            ALOGV("%s: Add a secondary desc %zu", __func__, i);
            if (secondaryMixes != nullptr) {
                secondaryMixes->push_back(policyMix);
            }
        }
    }
    return NO_ERROR;
}

AudioPolicyMixCollection::MixMatchStatus AudioPolicyMixCollection::mixMatch(
        const AudioMix* mix, size_t mixIndex, const audio_attributes_t& attributes, uid_t uid) {

    if (mix->mMixType == MIX_TYPE_PLAYERS) {
        // Loopback render mixes are created from a public API and thus restricted
        // to non sensible audio that have not opted out.
        if (is_mix_loopback_render(mix->mRouteFlags)) {
            auto hasFlag = [](auto flags, auto flag) { return (flags & flag) == flag; };
            if (hasFlag(attributes.flags, AUDIO_FLAG_NO_SYSTEM_CAPTURE)) {
                return MixMatchStatus::NO_MATCH;
            }
            if (!mix->mAllowPrivilegedPlaybackCapture &&
                hasFlag(attributes.flags, AUDIO_FLAG_NO_MEDIA_PROJECTION)) {
                return MixMatchStatus::NO_MATCH;
            }
            if (attributes.usage == AUDIO_USAGE_VOICE_COMMUNICATION &&
                !mix->mVoiceCommunicationCaptureAllowed) {
                return MixMatchStatus::NO_MATCH;
            }
            if (!(attributes.usage == AUDIO_USAGE_UNKNOWN ||
                  attributes.usage == AUDIO_USAGE_MEDIA ||
                  attributes.usage == AUDIO_USAGE_GAME ||
                  attributes.usage == AUDIO_USAGE_VOICE_COMMUNICATION)) {
                return MixMatchStatus::NO_MATCH;
            }
        }

        int userId = (int) multiuser_get_user_id(uid);

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

        bool hasUserIdExcludeRules = false;
        bool userIdExclusionFound = false;
        bool hasUserIdMatchRules = false;
        bool userIdMatchFound = false;


        bool hasAddrMatch = false;

        // iterate over all mix criteria to list what rules this mix contains
        for (size_t j = 0; j < mix->mCriteria.size(); j++) {
            ALOGV(" getOutputForAttr: mix %zu: inspecting mix criteria %zu of %zu",
                    mixIndex, j, mix->mCriteria.size());

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
                ALOGV("\tmix has RULE_MATCH_ATTRIBUTE_USAGE for usage %d",
                                            mix->mCriteria[j].mValue.mUsage);
                hasUsageMatchRules = true;
                if (mix->mCriteria[j].mValue.mUsage == attributes.usage) {
                    // found one match against all allowed usages
                    usageMatchFound = true;
                }
                break;
            case RULE_EXCLUDE_ATTRIBUTE_USAGE:
                ALOGV("\tmix has RULE_EXCLUDE_ATTRIBUTE_USAGE for usage %d",
                        mix->mCriteria[j].mValue.mUsage);
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
            case RULE_MATCH_USERID:
                ALOGV("\tmix has RULE_MATCH_USERID for userId %d",
                    mix->mCriteria[j].mValue.mUserId);
                hasUserIdMatchRules = true;
                if (mix->mCriteria[j].mValue.mUserId == userId) {
                    // found one userId match against all allowed userIds
                    userIdMatchFound = true;
                }
                break;
            case RULE_EXCLUDE_USERID:
                ALOGV("\tmix has RULE_EXCLUDE_USERID for userId %d",
                    mix->mCriteria[j].mValue.mUserId);
                hasUserIdExcludeRules = true;
                if (mix->mCriteria[j].mValue.mUserId == userId) {
                    // found this userId is to be excluded
                    userIdExclusionFound = true;
                }
                break;
            default:
                break;
            }

            // consistency checks: for each "dimension" of rules (usage, uid...), we can
            // only have MATCH rules, or EXCLUDE rules in each dimension, not a combination
            if (hasUsageMatchRules && hasUsageExcludeRules) {
                ALOGE("getOutputForAttr: invalid combination of RULE_MATCH_ATTRIBUTE_USAGE"
                        " and RULE_EXCLUDE_ATTRIBUTE_USAGE in mix %zu", mixIndex);
                return MixMatchStatus::INVALID_MIX;
            }
            if (hasUidMatchRules && hasUidExcludeRules) {
                ALOGE("getOutputForAttr: invalid combination of RULE_MATCH_UID"
                        " and RULE_EXCLUDE_UID in mix %zu", mixIndex);
                return MixMatchStatus::INVALID_MIX;
            }
            if (hasUserIdMatchRules && hasUserIdExcludeRules) {
                ALOGE("getOutputForAttr: invalid combination of RULE_MATCH_USERID"
                        " and RULE_EXCLUDE_USERID in mix %zu", mixIndex);
                    return MixMatchStatus::INVALID_MIX;
            }

            if ((hasUsageExcludeRules && usageExclusionFound)
                    || (hasUidExcludeRules && uidExclusionFound)
                    || (hasUserIdExcludeRules && userIdExclusionFound)) {
                break; // stop iterating on criteria because an exclusion was found (will fail)
            }
        }//iterate on mix criteria

        // determine if exiting on success (or implicit failure as desc is 0)
        if (hasAddrMatch ||
                !((hasUsageExcludeRules && usageExclusionFound) ||
                  (hasUsageMatchRules && !usageMatchFound)  ||
                  (hasUidExcludeRules && uidExclusionFound) ||
                  (hasUidMatchRules && !uidMatchFound) ||
                  (hasUserIdExcludeRules && userIdExclusionFound) ||
                  (hasUserIdMatchRules && !userIdMatchFound))) {
            ALOGV("\tgetOutputForAttr will use mix %zu", mixIndex);
            return MixMatchStatus::MATCH;
        }

    } else if (mix->mMixType == MIX_TYPE_RECORDERS) {
        if (attributes.usage == AUDIO_USAGE_VIRTUAL_SOURCE &&
                strncmp(attributes.tags, "addr=", strlen("addr=")) == 0 &&
                strncmp(attributes.tags + strlen("addr="),
                        mix->mDeviceAddress.string(),
                        AUDIO_ATTRIBUTES_TAGS_MAX_SIZE - strlen("addr=") - 1) == 0) {
            return MixMatchStatus::MATCH;
        }
    }
    return MixMatchStatus::NO_MATCH;
}


















////////////////////////////////////////////////////////////////////////////////////////////////
void AudioPolicyMixCollection::dump(String8 *dst) const
{
    dst->append("\nAudio Policy Mix:\n");
    for (size_t i = 0; i < size(); i++) {
        itemAt(i)->dump(dst, 2, i);
    }
}