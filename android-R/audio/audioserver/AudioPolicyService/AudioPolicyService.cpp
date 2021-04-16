// @ frameworks/av/services/audiopolicy/service/AudioPolicyInterfaceImpl.cpp

status_t AudioPolicyService::setForceUse(audio_policy_force_use_t usage,
                                         audio_policy_forced_cfg_t config)
{
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }

    if (!modifyAudioRoutingAllowed()) {
        return PERMISSION_DENIED;
    }

    if (usage < 0 || usage >= AUDIO_POLICY_FORCE_USE_CNT) {
        return BAD_VALUE;
    }
    if (config < 0 || config >= AUDIO_POLICY_FORCE_CFG_CNT) {
        return BAD_VALUE;
    }
    ALOGV("setForceUse()");
    Mutex::Autolock _l(mLock);
    AutoCallerClear acc;
    mAudioPolicyManager->setForceUse(usage, config);
    return NO_ERROR;
}

audio_policy_forced_cfg_t AudioPolicyService::getForceUse(audio_policy_force_use_t usage)
{
    if (mAudioPolicyManager == NULL) {
        return AUDIO_POLICY_FORCE_NONE;
    }
    if (usage < 0 || usage >= AUDIO_POLICY_FORCE_USE_CNT) {
        return AUDIO_POLICY_FORCE_NONE;
    }
    AutoCallerClear acc;
    return mAudioPolicyManager->getForceUse(usage);
}


//////////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyService::getOutputForAttr(audio_attributes_t *attr,
                                              audio_io_handle_t *output,
                                              audio_session_t session,
                                              audio_stream_type_t *stream,
                                              pid_t pid,
                                              uid_t uid,
                                              const audio_config_t *config,
                                              audio_output_flags_t flags,
                                              audio_port_handle_t *selectedDeviceId,
                                              audio_port_handle_t *portId,
                                              std::vector<audio_io_handle_t> *secondaryOutputs)
{
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }

    status_t result = validateUsage(attr->usage, pid, uid);
    if (result != NO_ERROR) {
        return result;
    }

    ALOGV("%s()", __func__);
    Mutex::Autolock _l(mLock);

    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!isAudioServerOrMediaServerUid(callingUid) || uid == (uid_t)-1) {
        ALOGW_IF(uid != (uid_t)-1 && uid != callingUid,
                "%s uid %d tried to pass itself off as %d", __func__, callingUid, uid);
        uid = callingUid;
    }
    if (!mPackageManager.allowPlaybackCapture(uid)) {
        attr->flags |= AUDIO_FLAG_NO_MEDIA_PROJECTION;
    }
    if (((attr->flags & (AUDIO_FLAG_BYPASS_INTERRUPTION_POLICY|AUDIO_FLAG_BYPASS_MUTE)) != 0)
            && !bypassInterruptionPolicyAllowed(pid, uid)) {
        attr->flags &= ~(AUDIO_FLAG_BYPASS_INTERRUPTION_POLICY|AUDIO_FLAG_BYPASS_MUTE);
    }
    AutoCallerClear acc;
    AudioPolicyInterface::output_type_t outputType;
    result = mAudioPolicyManager->getOutputForAttr(attr, output, session, stream, uid,
                                                 config,
                                                 &flags, selectedDeviceId, portId,
                                                 secondaryOutputs,
                                                 &outputType);

    // FIXME: Introduce a way to check for the the telephony device before opening the output
    if (result == NO_ERROR) {
        // enforce permission (if any) required for each type of input
        switch (outputType) {
        case AudioPolicyInterface::API_OUTPUT_LEGACY:
            break;
        case AudioPolicyInterface::API_OUTPUT_TELEPHONY_TX:
            if (!modifyPhoneStateAllowed(pid, uid)) {
                ALOGE("%s() permission denied: modify phone state not allowed for uid %d",
                    __func__, uid);
                result = PERMISSION_DENIED;
            }
            break;
        case AudioPolicyInterface::API_OUT_MIX_PLAYBACK:
            if (!modifyAudioRoutingAllowed(pid, uid)) {
                ALOGE("%s() permission denied: modify audio routing not allowed for uid %d",
                    __func__, uid);
                result = PERMISSION_DENIED;
            }
            break;
        case AudioPolicyInterface::API_OUTPUT_INVALID:
        default:
            LOG_ALWAYS_FATAL("%s() encountered an invalid output type %d",
                __func__, (int)outputType);
        }
    }

    if (result == NO_ERROR) {
        sp <AudioPlaybackClient> client =
            new AudioPlaybackClient(*attr, *output, uid, pid, session, *portId, *selectedDeviceId, *stream);
        mAudioPlaybackClients.add(*portId, client);
    }
    return result;
}




//动态策略注册
//////////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyService::registerPolicyMixes(const Vector<AudioMix>& mixes, bool registration)
{
    Mutex::Autolock _l(mLock);

    // loopback|render only need a MediaProjection (checked in caller AudioService.java)
    bool needModifyAudioRouting = std::any_of(mixes.begin(), mixes.end(), [](auto& mix) {
            return !is_mix_loopback_render(mix.mRouteFlags); });
    if (needModifyAudioRouting && !modifyAudioRoutingAllowed()) {
        return PERMISSION_DENIED;
    }

    // If one of the mixes has needCaptureVoiceCommunicationOutput set to true, then we
    // need to verify that the caller still has CAPTURE_VOICE_COMMUNICATION_OUTPUT
    bool needCaptureVoiceCommunicationOutput =
        std::any_of(mixes.begin(), mixes.end(), [](auto& mix) {
            return mix.mVoiceCommunicationCaptureAllowed; });

    bool needCaptureMediaOutput = std::any_of(mixes.begin(), mixes.end(), [](auto& mix) {
            return mix.mAllowPrivilegedPlaybackCapture; });

    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    const pid_t callingPid = IPCThreadState::self()->getCallingPid();

    if (needCaptureMediaOutput && !captureMediaOutputAllowed(callingPid, callingUid)) {
        return PERMISSION_DENIED;
    }

    if (needCaptureVoiceCommunicationOutput &&
        !captureVoiceCommunicationOutputAllowed(callingPid, callingUid)) {
        return PERMISSION_DENIED;
    }

    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    AutoCallerClear acc;
    if (registration) {
        return mAudioPolicyManager->registerPolicyMixes(mixes);
    } else {
        return mAudioPolicyManager->unregisterPolicyMixes(mixes);
    }
}
