/***
 * AudioCommandThread 类是 AudioPolicyService内部类，为了方便单独弄出来
 * 实现代码在 @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/service/AudioPolicyService.cpp 
 */ 


// Thread(false) 中 表示线程是c层启动
AudioPolicyService::AudioCommandThread::AudioCommandThread(String8 name,const wp<AudioPolicyService>& service)
    : Thread(false), mName(name), mService(service)
{
    mpToneGenerator = NULL;
}


void AudioPolicyService::AudioCommandThread::onFirstRef()
{
    run(mName.string(), ANDROID_PRIORITY_AUDIO);//启动线程执行
}


bool AudioPolicyService::AudioCommandThread::threadLoop()
{
    nsecs_t waitTime = -1;

    mLock.lock();
    while (!exitPending())
    {
        sp<AudioPolicyService> svc;
        while (!mAudioCommands.isEmpty() && !exitPending()) {
            nsecs_t curTime = systemTime();
            // commands are sorted by increasing time stamp: execute them from index 0 and up
            if (mAudioCommands[0]->mTime <= curTime) {
                sp<AudioCommand> command = mAudioCommands[0];
                mAudioCommands.removeAt(0);
                mLastCommand = command;

                switch (command->mCommand) {
                case START_TONE: {
                    mLock.unlock();
                    ToneData *data = (ToneData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing start tone %d on stream %d",data->mType, data->mStream);
                    delete mpToneGenerator;
                    mpToneGenerator = new ToneGenerator(data->mStream, 1.0);
                    mpToneGenerator->startTone(data->mType);
                    mLock.lock();
                    }break;
                case STOP_TONE: {
                    mLock.unlock();
                    ALOGV("AudioCommandThread() processing stop tone");
                    if (mpToneGenerator != NULL) {
                        mpToneGenerator->stopTone();
                        delete mpToneGenerator;
                        mpToneGenerator = NULL;
                    }
                    mLock.lock();
                    }break;
                case SET_VOLUME: {
                    VolumeData *data = (VolumeData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing set volume stream %d, volume %f, output %d", data->mStream, data->mVolume, data->mIO);
                    command->mStatus = AudioSystem::setStreamVolume(data->mStream,data->mVolume, data->mIO);
                    }break;
                case SET_PARAMETERS: {
                    ParametersData *data = (ParametersData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing set parameters string %s, io %d",data->mKeyValuePairs.string(), data->mIO);
                    command->mStatus = AudioSystem::setParameters(data->mIO, data->mKeyValuePairs);
                    }break;
                case SET_VOICE_VOLUME: {
                    VoiceVolumeData *data = (VoiceVolumeData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing set voice volume volume %f",data->mVolume);
                    command->mStatus = AudioSystem::setVoiceVolume(data->mVolume);
                    }break;
                case STOP_OUTPUT: {
                    StopOutputData *data = (StopOutputData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing stop output %d",data->mIO);
                    svc = mService.promote();
                    if (svc == 0) {
                        break;
                    }
                    mLock.unlock();
                    svc->doStopOutput(data->mIO, data->mStream, data->mSession);
                    mLock.lock();
                    }break;
                case RELEASE_OUTPUT: {
                    ReleaseOutputData *data = (ReleaseOutputData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing release output %d",data->mIO);
                    svc = mService.promote();
                    if (svc == 0) {
                        break;
                    }
                    mLock.unlock();
                    svc->doReleaseOutput(data->mIO, data->mStream, data->mSession);
                    mLock.lock();
                    }break;
                case CREATE_AUDIO_PATCH: {
                    CreateAudioPatchData *data = (CreateAudioPatchData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing create audio patch");
                    sp<IAudioFlinger> af = AudioSystem::get_audio_flinger();
                    if (af == 0) {
                        command->mStatus = PERMISSION_DENIED;
                    } else {
                        command->mStatus = af->createAudioPatch(&data->mPatch, &data->mHandle);
                    }
                    } break;
                case RELEASE_AUDIO_PATCH: {
                    ReleaseAudioPatchData *data = (ReleaseAudioPatchData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing release audio patch");
                    sp<IAudioFlinger> af = AudioSystem::get_audio_flinger();
                    if (af == 0) {
                        command->mStatus = PERMISSION_DENIED;
                    } else {
                        command->mStatus = af->releaseAudioPatch(data->mHandle);
                    }
                    } break;
                case UPDATE_AUDIOPORT_LIST: {
                    ALOGV("AudioCommandThread() processing update audio port list");
                    svc = mService.promote();
                    if (svc == 0) {
                        break;
                    }
                    mLock.unlock();
                    svc->doOnAudioPortListUpdate();
                    mLock.lock();
                    }break;
                case UPDATE_AUDIOPATCH_LIST: {
                    ALOGV("AudioCommandThread() processing update audio patch list");
                    svc = mService.promote();
                    if (svc == 0) {
                        break;
                    }
                    mLock.unlock();
                    svc->doOnAudioPatchListUpdate();
                    mLock.lock();
                    }break;
                case SET_AUDIOPORT_CONFIG: {
                    SetAudioPortConfigData *data = (SetAudioPortConfigData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing set port config");
                    sp<IAudioFlinger> af = AudioSystem::get_audio_flinger();
                    if (af == 0) {
                        command->mStatus = PERMISSION_DENIED;
                    } else {
                        command->mStatus = af->setAudioPortConfig(&data->mConfig);
                    }
                    } break;
                case DYN_POLICY_MIX_STATE_UPDATE: {
                    DynPolicyMixStateUpdateData *data = (DynPolicyMixStateUpdateData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing dyn policy mix state update %s %d", data->mRegId.string(), data->mState);
                    svc = mService.promote();
                    if (svc == 0) {
                        break;
                    }
                    mLock.unlock();
                    svc->doOnDynamicPolicyMixStateUpdate(data->mRegId, data->mState);
                    mLock.lock();
                    } break;
                case RECORDING_CONFIGURATION_UPDATE: {
                    RecordingConfigurationUpdateData *data = (RecordingConfigurationUpdateData *)command->mParam.get();
                    ALOGV("AudioCommandThread() processing recording configuration update");
                    svc = mService.promote();
                    if (svc == 0) {
                        break;
                    }
                    mLock.unlock();
                    svc->doOnRecordingConfigurationUpdate(data->mEvent, &data->mClientInfo, &data->mClientConfig, &data->mDeviceConfig, data->mPatchHandle);
                    mLock.lock();
                    } break;
                default:
                    ALOGW("AudioCommandThread() unknown command %d", command->mCommand);
                }


                {
                    Mutex::Autolock _l(command->mLock);
                    if (command->mWaitStatus) {
                        command->mWaitStatus = false;
                        command->mCond.signal();
                    }
                    
                }
                waitTime = -1;
                // release mLock before releasing strong reference on the service as
                // AudioPolicyService destructor calls AudioCommandThread::exit() which
                // acquires mLock.
                mLock.unlock();
                svc.clear();
                mLock.lock();
            } else {
                waitTime = mAudioCommands[0]->mTime - curTime;
                break;
            }
        }

        // release delayed commands wake lock if the queue is empty
        if (mAudioCommands.isEmpty()) {
            release_wake_lock(mName.string());
        }

        // At this stage we have either an empty command queue or the first command in the queue
        // has a finite delay. So unless we are exiting it is safe to wait.
        if (!exitPending()) {
            ALOGV("AudioCommandThread() going to sleep");
            if (waitTime == -1) {
                mWaitWorkCV.wait(mLock);
            } else {
                mWaitWorkCV.waitRelative(mLock, waitTime);
            }
        }
    }
    // release delayed commands wake lock before quitting
    if (!mAudioCommands.isEmpty()) {
        release_wake_lock(mName.string());
    }
    mLock.unlock();
    return false;
}

