//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libaudioclient/AudioSystem.cpp
status_t AudioSystem::getInputForAttr(const audio_attributes_t *attr,
                                audio_io_handle_t *input,
                                audio_session_t session,
                                pid_t pid,
                                uid_t uid,
                                const String16& opPackageName,
                                const audio_config_base_t *config,
                                audio_input_flags_t flags,
                                audio_port_handle_t *selectedDeviceId,
                                audio_port_handle_t *portId)
{
    const sp<IAudioPolicyService>& aps = AudioSystem::get_audio_policy_service();
    if (aps == 0) return NO_INIT;
    //  @   frameworks/av/services/audiopolicy/service/AudioPolicyInterfaceImpl.cpp
    return aps->getInputForAttr(attr, input, session, pid, uid, opPackageName,config, flags, selectedDeviceId, portId);
}




status_t AudioSystem::startInput(audio_port_handle_t portId, bool *silenced)
{
    const sp<IAudioPolicyService>& aps = AudioSystem::get_audio_policy_service();
    if (aps == 0) return PERMISSION_DENIED;
    return aps->startInput(portId, silenced);
}