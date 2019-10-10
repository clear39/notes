/**
 * 
 * 
 * @    system/media/audio/include/system/audio.h:312:typedef struct audio_config_base audio_config_base_t;
 * audio_config_base    @    system/media/audio/include/system/audio.h
 * 
 * 
 * system/media/audio/include/system/audio.h:45:typedef int audio_io_handle_t;
*/
status_t AudioPolicyService::getInputForAttr(const audio_attributes_t *attr,
                                             audio_io_handle_t *input,   // 用于标记 AudioFlinger 创建的RecordThread线程
                                             audio_session_t session,  // 用于标记 客户端
                                             pid_t pid,
                                             uid_t uid,
                                             const String16& opPackageName,
                                             const audio_config_base_t *config,
                                             audio_input_flags_t flags,
                                             audio_port_handle_t *selectedDeviceId,   //
                                             audio_port_handle_t *portId   // 
                                             )
{

    //  10-10 13:44:10.458  1771  1771 V AudioPolicyIntefaceImpl: getInputForAttr (0,0,8,0x0,) 0 41 3505 0 MLS_AUDIO_RECORD (48000,0xC,1) 0x0 0 0
    ALOGV("%s (%d,%d,%d,0x%X,%s) %d %d %d %d %s (%d,0x%X,%d) 0x%X %d %d",__func__,
        attr->content_type,attr->usage,attr->source,attr->flags,attr->tags,
        *input,session,pid,uid,String8(opPackageName).c_str(),
        config->sample_rate,config->channel_mask,config->format,
        flags,*selectedDeviceId,*portId);

    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }

    // already checked by client, but double-check in case the client wrapper is bypassed
    /**
     * system/media/audio/include/system/audio-base.h:52:    AUDIO_SOURCE_UNPROCESSED = 9
     * system/media/audio/include/system/audio-base-utils.h:39:    AUDIO_SOURCE_MAX          = AUDIO_SOURCE_UNPROCESSED,
     * system/media/audio/include/system/audio-base-utils.h:40:    AUDIO_SOURCE_CNT          = AUDIO_SOURCE_MAX + 1,
    */
    if (attr->source < AUDIO_SOURCE_DEFAULT && attr->source >= AUDIO_SOURCE_CNT &&
            attr->source != AUDIO_SOURCE_HOTWORD && attr->source != AUDIO_SOURCE_FM_TUNER) {
        return BAD_VALUE;
    }

    bool updatePid = (pid == -1);
    const uid_t callingUid = IPCThreadState::self()->getCallingUid();
    if (!isTrustedCallingUid(callingUid)) {
        ALOGW_IF(uid != (uid_t)-1 && uid != callingUid,"%s uid %d tried to pass itself off as %d", __FUNCTION__, callingUid, uid);
        uid = callingUid;
        updatePid = true;
    }

    if (updatePid) {
        const pid_t callingPid = IPCThreadState::self()->getCallingPid();
        ALOGW_IF(pid != (pid_t)-1 && pid != callingPid,"%s uid %d pid %d tried to pass itself off as pid %d",__func__, callingUid, callingPid, pid);
        pid = callingPid;
    }

    /**
     * @    frameworks/av/services/audioflinger/ServiceUtilities.cpp
    */
    // check calling permissions
    if (!recordingAllowed(opPackageName, pid, uid)) {
        ALOGE("%s permission denied: recording not allowed for uid %d pid %d",__func__, uid, pid);
        return PERMISSION_DENIED;
    }

    if ((attr->source == AUDIO_SOURCE_VOICE_UPLINK ||
        attr->source == AUDIO_SOURCE_VOICE_DOWNLINK ||
        attr->source == AUDIO_SOURCE_VOICE_CALL) &&
        !captureAudioOutputAllowed(pid, uid)) {
        return PERMISSION_DENIED;
    }

    if ((attr->source == AUDIO_SOURCE_HOTWORD) && !captureHotwordAllowed(pid, uid)) {
        return BAD_VALUE;
    }

    sp<AudioPolicyEffects>audioPolicyEffects;
    {
        status_t status;

        /**
         * 
         * **/
        AudioPolicyInterface::input_type_t inputType;

        Mutex::Autolock _l(mLock);
        {
            AutoCallerClear acc;
            // the audio_in_acoustics_t parameter is ignored by get_input()
            status = mAudioPolicyManager->getInputForAttr(attr, input, 
                                                         session,
                                                         uid,
                                                         config,
                                                         flags, 
                                                         selectedDeviceId,
                                                         &inputType,
                                                         portId
                                                         );
        }

        /**
         * 
         * 
        */
        audioPolicyEffects = mAudioPolicyEffects;

        //  10-10 13:44:10.463  1771  1771 V AudioPolicyIntefaceImpl: getInputForAttr inputType:1 
        ALOGV("%s inputType:%d ",__func__,inputType);

        if (status == NO_ERROR) {
            // enforce permission (if any) required for each type of input
            switch (inputType) {
            case AudioPolicyInterface::API_INPUT_LEGACY:
                break;
            case AudioPolicyInterface::API_INPUT_TELEPHONY_RX:
                // FIXME: use the same permission as for remote submix for now.
            case AudioPolicyInterface::API_INPUT_MIX_CAPTURE:   // API_INPUT_MIX_CAPTURE  = 1,//
                if (!captureAudioOutputAllowed(pid, uid)) {
                    ALOGE("getInputForAttr() permission denied: capture not allowed");
                    status = PERMISSION_DENIED;
                }
                break;
            case AudioPolicyInterface::API_INPUT_MIX_EXT_POLICY_REROUTE:
                if (!modifyAudioRoutingAllowed()) {
                    ALOGE("getInputForAttr() permission denied: modify audio routing not allowed");
                    status = PERMISSION_DENIED;
                }
                break;
            case AudioPolicyInterface::API_INPUT_INVALID:
            default:
                LOG_ALWAYS_FATAL("getInputForAttr() encountered an invalid input type %d",(int)inputType);
            }
        }

        if (status != NO_ERROR) {
            if (status == PERMISSION_DENIED) {
                AutoCallerClear acc;
                mAudioPolicyManager->releaseInput(*input, session);
            }
            return status;
        }

        sp<AudioRecordClient> client = new AudioRecordClient(*attr, *input, uid, pid, opPackageName, session);
        client->active = false;
        client->isConcurrent = false;
        client->isVirtualDevice = false; //TODO : update from APM->getInputForAttr()
        client->deviceId = *selectedDeviceId;
        mAudioRecordClients.add(*portId, client);
    }

    if (audioPolicyEffects != 0) {
        // create audio pre processors according to input source
        status_t status = audioPolicyEffects->addInputEffects(*input, attr->source, session);
        if (status != NO_ERROR && status != ALREADY_EXISTS) {
            ALOGW("Failed to add effects on input %d", *input);
        }
    }
    return NO_ERROR;
}