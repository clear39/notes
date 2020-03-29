//  @ frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp
AudioPolicyService::AudioPolicyService()
    : BnAudioPolicyService(), mpAudioPolicyDev(NULL), mpAudioPolicy(NULL),
      mAudioPolicyManager(NULL), mAudioPolicyClient(NULL), mPhoneState(AUDIO_MODE_INVALID)
{
    //这里啥事没干
}

void AudioPolicyService::onFirstRef()
{
    {
        Mutex::Autolock _l(mLock);

        /***
         * 启动三个线程
         */ 
        // start tone playback thread
        mTonePlaybackThread = new AudioCommandThread(String8("ApmTone"), this);
        // start audio commands thread
        mAudioCommandThread = new AudioCommandThread(String8("ApmAudio"), this);
        // start output activity command thread
        mOutputCommandThread = new AudioCommandThread(String8("ApmOutput"), this);

        /***
         * 封装 毁掉给 AudioPolicyService 的接口
         * 
         * @    frameworks/av/services/audiopolicy/service/AudioPolicyClientImpl.cpp
         */ 
        mAudioPolicyClient = new AudioPolicyClient(this);
        //  @   /work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/manager/AudioPolicyFactory.cpp
        /***
         * createAudioPolicyManager 只是new AudioPolicyManager 类并将 mAudioPolicyClient 传递给AudioPolicyManager
         * AudioPolicyClient 用于 AudioPolicyManager 回调 AudioPolicyService 的接口封装
         * 
         * extern "C" AudioPolicyInterface* createAudioPolicyManager(AudioPolicyClientInterface *clientInterface)
         * ---> new AudioPolicyManager(clientInterface);
         * 
         */ 
        mAudioPolicyManager = createAudioPolicyManager(mAudioPolicyClient);
    }

    /**
     * 
     */ 
    // load audio processing modules
    sp<AudioPolicyEffects> audioPolicyEffects = new AudioPolicyEffects();
    {
        Mutex::Autolock _l(mLock);
        mAudioPolicyEffects = audioPolicyEffects;
    }

    /**
     * 由于多用户管理，和ActivityManagerService交互
     */ 
    mUidPolicy = new UidPolicy(this);
    mUidPolicy->registerSelf();
}











/**
 * ActivityManagerService
 * --> UidPolicy::onUidActive/onUidGone/onUidIdle
 * ---> AudioPolicyService::UidPolicy::notifyService
 * 
*/
void AudioPolicyService::setRecordSilenced(uid_t uid, bool silenced)
{
    {
        Mutex::Autolock _l(mLock);
        if (mAudioPolicyManager) {
            AutoCallerClear acc;
            mAudioPolicyManager->setRecordSilenced(uid, silenced);
        }
    }
    sp<IAudioFlinger> af = AudioSystem::get_audio_flinger();
    if (af) {
        af->setRecordSilenced(uid, silenced);
    }
}


/**
 * @    frameworks/av/services/audiopolicy/service/AudioPolicyInterfaceImpl.cpp
 *  
 * AudioTrack::createTrack_l()
 * --->AudioTrack::set(...)
 * -------> AudioFlinger::createTrack(...)
*/
status_t AudioPolicyService::getOutputForAttr(const audio_attributes_t *attr,
                                              audio_io_handle_t *output,
                                              audio_session_t session,
                                              audio_stream_type_t *stream,
                                              pid_t pid,
                                              uid_t uid,
                                              const audio_config_t *config,
                                              audio_output_flags_t flags,
                                              audio_port_handle_t *selectedDeviceId,
                                              audio_port_handle_t *portId)
{
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    ALOGV("getOutputForAttr()");
    Mutex::Autolock _l(mLock);

    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!isTrustedCallingUid(callingUid) || uid == (uid_t)-1) {
        ALOGW_IF(uid != (uid_t)-1 && uid != callingUid,"%s uid %d tried to pass itself off as %d", __FUNCTION__, callingUid, uid);
        uid = callingUid;
    }
    audio_output_flags_t originalFlags = flags;
    AutoCallerClear acc;

    /**
     * 
    */
    status_t result = mAudioPolicyManager->getOutputForAttr(attr, output, session, stream, uid,config,&flags, selectedDeviceId, portId);

    // FIXME: Introduce a way to check for the the telephony device before opening the output
    if ((result == NO_ERROR) &&  (flags & AUDIO_OUTPUT_FLAG_INCALL_MUSIC) &&!modifyPhoneStateAllowed(pid, uid)) {
        // If the app tries to play music through the telephony device and doesn't have permission
        // the fallback to the default output device.
        mAudioPolicyManager->releaseOutput(*output, *stream, session);
        flags = originalFlags;
        *selectedDeviceId = AUDIO_PORT_HANDLE_NONE;
        *portId = AUDIO_PORT_HANDLE_NONE;
        result = mAudioPolicyManager->getOutputForAttr(attr, output, session, stream, uid,config,&flags, selectedDeviceId, portId);
    }
    return result;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

status_t AudioPolicyService::listAudioPorts(audio_port_role_t role,audio_port_type_t type,unsigned int *num_ports,struct audio_port *ports,unsigned int *generation)
{
    Mutex::Autolock _l(mLock);
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    AutoCallerClear acc;
    return mAudioPolicyManager->listAudioPorts(role, type, num_ports, ports, generation);
}

status_t AudioPolicyService::listAudioPatches(unsigned int *num_patches,struct audio_patch *patches,unsigned int *generation)
{
    Mutex::Autolock _l(mLock);
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    AutoCallerClear acc;
    return mAudioPolicyManager->listAudioPatches(num_patches, patches, generation);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyService::createAudioPatch(const struct audio_patch *patch,audio_patch_handle_t *handle)
{
    Mutex::Autolock _l(mLock);
    if(!modifyAudioRoutingAllowed()) {
        return PERMISSION_DENIED;
    }
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    AutoCallerClear acc;
    return mAudioPolicyManager->createAudioPatch(patch, handle,IPCThreadState::self()->getCallingUid());
}

status_t AudioPolicyService::releaseAudioPatch(audio_patch_handle_t handle)
{
    Mutex::Autolock _l(mLock);
    if(!modifyAudioRoutingAllowed()) {
        return PERMISSION_DENIED;
    }
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    AutoCallerClear acc;
    return mAudioPolicyManager->releaseAudioPatch(handle,IPCThreadState::self()->getCallingUid());
}




