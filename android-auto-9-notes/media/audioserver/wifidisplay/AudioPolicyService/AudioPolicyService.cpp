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




















/////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyService::startInput(audio_port_handle_t portId, bool *silenced)
{
    if (mAudioPolicyManager == NULL) {
        return NO_INIT;
    }
    sp<AudioRecordClient> client;
    {
        Mutex::Autolock _l(mLock);

        ssize_t index = mAudioRecordClients.indexOfKey(portId);
        if (index < 0) {
            return INVALID_OPERATION;
        }
        client = mAudioRecordClients.valueAt(index);
    }

    /**
     * 
     * 
    */
    // check calling permissions
    if (!startRecording(client->opPackageName, client->pid, client->uid)) {
        ALOGE("%s permission denied: recording not allowed for uid %d pid %d",__func__, client->uid, client->pid);
        return PERMISSION_DENIED;
    }

    // If UID inactive it records silence until becoming active
    *silenced = !mUidPolicy->isUidActive(client->uid) && !client->isVirtualDevice;

    Mutex::Autolock _l(mLock);
    AudioPolicyInterface::concurrency_type__mask_t concurrency = AudioPolicyInterface::API_INPUT_CONCURRENCY_NONE;

    status_t status;
    {
        AutoCallerClear acc;
        /**
         * 
         * 
        */
        status = mAudioPolicyManager->startInput(client->input, client->session, *silenced, &concurrency);

    }

    // including successes gets very verbose
    if (status != NO_ERROR) {

        static constexpr char kAudioPolicy[] = "audiopolicy";

        static constexpr char kAudioPolicyReason[] = "android.media.audiopolicy.reason";
        static constexpr char kAudioPolicyStatus[] = "android.media.audiopolicy.status";
        static constexpr char kAudioPolicyRqstSrc[] = "android.media.audiopolicy.rqst.src";
        static constexpr char kAudioPolicyRqstPkg[] = "android.media.audiopolicy.rqst.pkg";
        static constexpr char kAudioPolicyRqstSession[] = "android.media.audiopolicy.rqst.session";
        static constexpr char kAudioPolicyRqstDevice[] = "android.media.audiopolicy.rqst.device";
        static constexpr char kAudioPolicyActiveSrc[] = "android.media.audiopolicy.active.src";
        static constexpr char kAudioPolicyActivePkg[] = "android.media.audiopolicy.active.pkg";
        static constexpr char kAudioPolicyActiveSession[] = "android.media.audiopolicy.active.session";
        static constexpr char kAudioPolicyActiveDevice[] = "android.media.audiopolicy.active.device";

        MediaAnalyticsItem *item = new MediaAnalyticsItem(kAudioPolicy);
        if (item != NULL) {

            item->setCString(kAudioPolicyReason, audioConcurrencyString(concurrency).c_str());
            item->setInt32(kAudioPolicyStatus, status);

            item->setCString(kAudioPolicyRqstSrc,
                             audioSourceString(client->attributes.source).c_str());
            item->setCString(kAudioPolicyRqstPkg,
                             std::string(String8(client->opPackageName).string()).c_str());
            item->setInt32(kAudioPolicyRqstSession, client->session);

            item->setCString(
                    kAudioPolicyRqstDevice, getDeviceTypeStrForPortId(client->deviceId).c_str());

            // figure out who is active
            // NB: might the other party have given up the microphone since then? how sure.
            // perhaps could have given up on it.
            // we hold mLock, so perhaps we're safe for this looping
            if (concurrency != AudioPolicyInterface::API_INPUT_CONCURRENCY_NONE) {
                int count = mAudioRecordClients.size();
                for (int i = 0; i<count ; i++) {
                    if (portId == mAudioRecordClients.keyAt(i)) {
                        continue;
                    }
                    sp<AudioRecordClient> other = mAudioRecordClients.valueAt(i);
                    if (other->active) {
                        // keeps the last of the clients marked active
                        item->setCString(kAudioPolicyActiveSrc,
                                         audioSourceString(other->attributes.source).c_str());
                        item->setCString(kAudioPolicyActivePkg,
                                     std::string(String8(other->opPackageName).string()).c_str());
                        item->setInt32(kAudioPolicyActiveSession, other->session);
                        item->setCString(kAudioPolicyActiveDevice,
                                         getDeviceTypeStrForPortId(other->deviceId).c_str());
                    }
                }
            }
            item->selfrecord();
            delete item;
            item = NULL;
        }
    }

    if (status == NO_ERROR) {
        LOG_ALWAYS_FATAL_IF(concurrency & ~AudioPolicyInterface::API_INPUT_CONCURRENCY_ALL,"startInput(): invalid concurrency type %d", (int)concurrency);

        // enforce permission (if any) required for each type of concurrency
        if (concurrency & AudioPolicyInterface::API_INPUT_CONCURRENCY_CALL) {
            //TODO: check incall capture permission
        }
        if (concurrency & AudioPolicyInterface::API_INPUT_CONCURRENCY_CAPTURE) {
            //TODO: check concurrent capture permission
        }

        client->active = true;
    } else {
        finishRecording(client->opPackageName, client->uid);
    }

    return status;
}
















/***
 * 
 * AudioFlinger::createTrack ---> 
 * 
 * */
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
    status_t result = mAudioPolicyManager->getOutputForAttr(attr, output, session, stream, uid,
                                                 config,
                                                 &flags, selectedDeviceId, portId);

    // FIXME: Introduce a way to check for the the telephony device before opening the output
    if ((result == NO_ERROR) &&
        (flags & AUDIO_OUTPUT_FLAG_INCALL_MUSIC) &&
        !modifyPhoneStateAllowed(pid, uid)) {
        // If the app tries to play music through the telephony device and doesn't have permission
        // the fallback to the default output device.
        mAudioPolicyManager->releaseOutput(*output, *stream, session);
        flags = originalFlags;
        *selectedDeviceId = AUDIO_PORT_HANDLE_NONE;
        *portId = AUDIO_PORT_HANDLE_NONE;
        result = mAudioPolicyManager->getOutputForAttr(attr, output, session, stream, uid,
                                                 config,
                                                 &flags, selectedDeviceId, portId);
    }
    return result;
}