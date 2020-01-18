//  @frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
/***
 * AudioPolicyManager 继承了 AudioPolicyManagerObserver 和 AudioPolicyInterface
 * 
*/

#define USE_XML_AUDIO_POLICY_CONF 1

class AudioPolicyManager : public AudioPolicyInterface, public AudioPolicyManagerObserver
{
    ......
}


AudioPolicyManager::AudioPolicyManager(AudioPolicyClientInterface *clientInterface) : AudioPolicyManager(clientInterface, false /*forTesting*/)
{
    loadConfig();
    initialize();
}

AudioPolicyManager::AudioPolicyManager(AudioPolicyClientInterface *clientInterface,bool /*forTesting*/):
    mUidCached(getuid()),
    mpClientInterface(clientInterface),
    mLimitRingtoneVolume(false), mLastVoiceVolume(-1.0f),
    mA2dpSuspended(false),
#ifdef USE_XML_AUDIO_POLICY_CONF
    //  VolumeCurvesCollection @ /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/include/VolumeCurve.h
    mVolumeCurves(new VolumeCurvesCollection()),
    //注意这里创建的 AudioPolicyConfig
    mConfig(mHwModulesAll, mAvailableOutputDevices, mAvailableInputDevices,mDefaultOutputDevice, static_cast<VolumeCurvesCollection*>(mVolumeCurves.get())),
#else
    mVolumeCurves(new StreamDescriptorCollection()),
    mConfig(mHwModulesAll, mAvailableOutputDevices, mAvailableInputDevices,mDefaultOutputDevice),
#endif
    mAudioPortGeneration(1),
    mBeaconMuteRefCount(0),
    mBeaconPlayingRefCount(0),
    mBeaconMuted(false),
    mTtsOutputAvailable(false),
    mMasterMono(false),
    mMusicEffectOutput(AUDIO_IO_HANDLE_NONE),
    mHasComputedSoundTriggerSupportsConcurrentCapture(false)
{

}

/**
 * AudioPolicyManager::AudioPolicyManager()
 * ---> 
*/
void AudioPolicyManager::loadConfig() {
#ifdef USE_XML_AUDIO_POLICY_CONF
    if (deserializeAudioPolicyXmlConfig(getConfig()) != NO_ERROR) {
#else
    if ((ConfigParsingUtils::loadConfig(AUDIO_POLICY_VENDOR_CONFIG_FILE, getConfig()) != NO_ERROR)
           && (ConfigParsingUtils::loadConfig(AUDIO_POLICY_CONFIG_FILE, getConfig()) != NO_ERROR)) {
#endif
        ALOGE("could not load audio policy configuration file, setting defaults");
        getConfig().setDefault();
    }
}

#ifdef USE_XML_AUDIO_POLICY_CONF
// Treblized audio policy xml config will be located in /odm/etc or /vendor/etc.
static const char *kConfigLocationList[] =  {"/odm/etc", "/vendor/etc", "/system/etc"};
static const int kConfigLocationListSize = (sizeof(kConfigLocationList) / sizeof(kConfigLocationList[0]));

static status_t deserializeAudioPolicyXmlConfig(AudioPolicyConfig &config) {
    char audioPolicyXmlConfigFile[AUDIO_POLICY_XML_CONFIG_FILE_PATH_MAX_LENGTH];
    std::vector<const char*> fileNames;
    status_t ret;
    /**
     * 当前平台由于 ro.bluetooth.a2dp_offload.supported 和 persist.bluetooth.a2dp_offload.disabled 属性均为空
     * 所以 audio_policy_configuration_a2dp_offload_disabled.xml 不会添加到 fileNames 集合中
    */
    if (property_get_bool("ro.bluetooth.a2dp_offload.supported", false) && property_get_bool("persist.bluetooth.a2dp_offload.disabled", false)) {
        // A2DP offload supported but disabled: try to use special XML file
        //  #define AUDIO_POLICY_A2DP_OFFLOAD_DISABLED_XML_CONFIG_FILE_NAME "audio_policy_configuration_a2dp_offload_disabled.xml"
        fileNames.push_back(AUDIO_POLICY_A2DP_OFFLOAD_DISABLED_XML_CONFIG_FILE_NAME);
    }

    //  #define AUDIO_POLICY_XML_CONFIG_FILE_NAME "audio_policy_configuration.xml"
    fileNames.push_back(AUDIO_POLICY_XML_CONFIG_FILE_NAME);

    
    /**
     * 这里 fileNames 集合中只有一个成员 audio_policy_configuration.xml 
     * 当前平台只有 /vendor/etc/audio_policy_configuration.xml 存在
     * 
     */ 
    for (const char* fileName : fileNames) {
        for (int i = 0; i < kConfigLocationListSize; i++) {
            //  @frameworks/av/services/audiopolicy/common/managerdefinitions/src/Serializer.cpp
            /***
             * 通过 PolicySerializer 解析 xml文件，并且把数据存入 AudioPolicyConfig
             * 具体的解析流程详细信息再 Serializer 文件夹下
             * 
             * */
            PolicySerializer serializer;
            snprintf(audioPolicyXmlConfigFile, sizeof(audioPolicyXmlConfigFile),"%s/%s", kConfigLocationList[i], fileName);

            ret = serializer.deserialize(audioPolicyXmlConfigFile, config);
            if (ret == NO_ERROR) {
                return ret;
            }
        }
    }
    return ret;
}
#endif


/**
 * AudioPolicyManager::AudioPolicyManager()
 * ---> 
*/
status_t AudioPolicyManager::initialize() {
    //  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/StreamDescriptor.cpp
    /*
        @   frameworks/av/services/audiopolicy/common/managerdefinitions/include/IVolumeCurvesCollection.h
        initializeVolumeCurves 函数空实现，什么事情也没有做
    */
    mVolumeCurves->initializeVolumeCurves(getConfig().isSpeakerDrcEnabled());

    /**
     * 
     *   @ frameworks/av/services/audiopolicy/engineconfigurable/src/EngineInstance.cpp
     * 以下倆部都是获得 mEngine 为 ManagerInterfaceImpl
     * 
     * */
    // Once policy config has been parsed, retrieve an instance of the engine and initialize it.
    audio_policy::EngineInstance *engineInstance = audio_policy::EngineInstance::getInstance();
    if (!engineInstance) {
        ALOGE("%s:  Could not get an instance of policy engine", __FUNCTION__);
        return NO_INIT;
    }
    // Retrieve the Policy Manager Interface
    mEngine = engineInstance->queryInterface<AudioPolicyManagerInterface>();
    if (mEngine == NULL) {
        ALOGE("%s: Failed to get Policy Engine Interface", __FUNCTION__);
        return NO_INIT;
    }

    //设置观察这模式回调接口
    mEngine->setObserver(this);
    
    status_t status = mEngine->initCheck();
    if (status != NO_ERROR) {
        LOG_FATAL("Policy engine not initialized(err=%d)", status);
        return status;
    }

    /**
     * mAvailableOutputDevices 和 mAvailableInputDevices 是  从 mDeclaredDevices 中查找到的
     * 即可得到 mAvailableOutputDevices 和 mAvailableInputDevices 为 DeviceDescriptor ，
     * outputDeviceTypes 为 AUDIO_DEVICE_OUT_BUS
     * inputDeviceTypes 为 AUDIO_DEVICE_IN_BUILTIN_MIC
     * 
     * 
     * system/media/audio/include/system/audio-base.h:290:    AUDIO_DEVICE_NONE                          = 0x0u
     * 注意这里 mAvailableOutputDevices 和 mAvailableInputDevices 默认 AUDIO_DEVICE_NONE
     * 除了 primary module 和 r_submix module 有 成员
     * 
    */
    // mAvailableOutputDevices and mAvailableInputDevices now contain all attached devices
    // open all output streams needed to access attached devices
    audio_devices_t outputDeviceTypes = mAvailableOutputDevices.types();  //    AUDIO_DEVICE_OUT_BUS
    audio_devices_t inputDeviceTypes = mAvailableInputDevices.types() & ~AUDIO_DEVICE_BIT_IN;  // AUDIO_DEVICE_IN_BUILTIN_MIC

    for (const auto& hwModule : mHwModulesAll) {

        // 返回 audio_module_handle_t 
        //  @ system/media/audio/include/system/audio.h:321:typedef int audio_module_handle_t;
        //  @ system/media/audio/include/system/audio-base.h:15:    AUDIO_MODULE_HANDLE_NONE = 0
        /**
         * audio_module_handle_t AudioFlinger::loadHwModule(const char *name)
         * --> audio_module_handle_t AudioFlinger::loadHwModule_l(const char *name)
        */
        hwModule->setHandle(mpClientInterface->loadHwModule(hwModule->getName()));
        if (hwModule->getHandle() == AUDIO_MODULE_HANDLE_NONE) {
            ALOGW("could not open HW module %s", hwModule->getName());
            continue;
        }
        mHwModules.push_back(hwModule);
        // open all output streams needed to access attached devices
        // except for direct output streams that are only opened when they are actually
        // required by an app.
        // This also validates mAvailableOutputDevices list
        /**
         * 
         * 这里 IOProfile （对应音频策略文件中 mixport 标签）
         * 
        */
        for (const auto& outProfile : hwModule->getOutputProfiles()) {
            /**
             * 这里返回true
            */
            if (!outProfile->canOpenNewIo()) {
                ALOGE("Invalid Output profile max open count %u for profile %s",outProfile->maxOpenCount, outProfile->getTagName().c_str());
                continue;
            }

            /**
             * 这里 HwModule::setRoutes -----> HwModule::refreshSupportedDevices() 进行设置 SupportedDevices
             * 
             * */
            if (!outProfile->hasSupportedDevices()) {
                ALOGW("Output profile contains no device on module %s", hwModule->getName());
                continue;
            }

            if ((outProfile->getFlags() & AUDIO_OUTPUT_FLAG_TTS) != 0) {
                mTtsOutputAvailable = true;
            }

            if ((outProfile->getFlags() & AUDIO_OUTPUT_FLAG_DIRECT) != 0) {
                continue;
            }

            /**
             * HwModule::setRoutes(const AudioRouteVector &routes) 
             * --> HwModule::refreshSupportedDevices() 
             * ---> IOProfile::setSupportedDevices(const DeviceVector &devices)
             * 这里 getSupportedDevicesType 是调用 DeviceDescriptor 的 mDeviceType（匹配配置文件中 type 属性值）
             * 
             * profileType 为 AUDIO_DEVICE_OUT_BUS
             * 
             * 获取 SupportedDevices 所有所支持的  audio_devices_t
            */
            audio_devices_t profileType = outProfile->getSupportedDevicesType();

            /**
             * mDefaultOutputDevice 是 通过 <defaultOutputDevice>bus0_media_out</defaultOutputDevice> 从 mDeclaredDevices 中查找到的
             * 即可得到 mDefaultOutputDevice 为 DeviceDescriptor ，type() 返回值为 mDeviceType 为 AUDIO_DEVICE_OUT_BUS
             * 
            */
            if ((profileType & mDefaultOutputDevice->type()) != AUDIO_DEVICE_NONE) {
                // 执行这里  profileType = AUDIO_DEVICE_OUT_BUS
                profileType = mDefaultOutputDevice->type();
            } else {
                // chose first device present in profile's SupportedDevices also part of
                // outputDeviceTypes
                /**
                 * outputDeviceTypes = AUDIO_DEVICE_OUT_BUS
                 * 查找支持 outputDeviceTypes 属性的 audio_devices_t(有可能支持不仅仅只有outputDeviceTypes) 并且返回
                */
                profileType = outProfile->getSupportedDeviceForType(outputDeviceTypes);
            }

            /**
             * 如果 mAvailableOutputDevices 为空，则  outputDeviceTypes = AUDIO_DEVICE_NONE = 0
             * 这里注意 首先只有添加到 mAvailableOutputDevices 的设备 才能往下走，不然直接continue
            */
            if ((profileType & outputDeviceTypes) == 0) {
                continue;
            }

            sp<SwAudioOutputDescriptor> outputDesc = new SwAudioOutputDescriptor(outProfile,mpClientInterface);
            /**
             * 
            */
            const DeviceVector &supportedDevices = outProfile->getSupportedDevices();
            /**
             * 通过传入参数 profileType 过滤出匹配的 DeviceDescriptor 
            */
            const DeviceVector &devicesForType = supportedDevices.getDevicesFromType(profileType);
            /**
             * <devicePort tagName="bus0_media_out" role="sink" type="AUDIO_DEVICE_OUT_BUS" address="bus0_media_out">
             * 
             * 这里获取匹配设备对应的address
            */
            String8 address = devicesForType.size() > 0 ? devicesForType.itemAt(0)->mAddress : String8("");
           

            /**
             * 之类内部会 调用 AudioFlinger::openOutput 接口
             * output 用于获取创建AudioFlinger::PlaybackThread线程对应的key,同时也是硬件抽象层输出流
             * */
            audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
            status_t status = outputDesc->open(nullptr, profileType, address,AUDIO_STREAM_DEFAULT, AUDIO_OUTPUT_FLAG_NONE, &output);

            if (status != NO_ERROR) {
                ALOGW("Cannot open output stream for device %08x on hw module %s", outputDesc->mDevice, hwModule->getName());
            } else {
                for (const auto& dev : supportedDevices) {
                    ssize_t index = mAvailableOutputDevices.indexOf(dev);
                    // give a valid ID to an attached device once confirmed it is reachable
                    if (index >= 0 && !mAvailableOutputDevices[index]->isAttached()) {
                        mAvailableOutputDevices[index]->attach(hwModule);
                    }
                }

                if (mPrimaryOutput == 0 && outProfile->getFlags() & AUDIO_OUTPUT_FLAG_PRIMARY) {
                    mPrimaryOutput = outputDesc;
                }

                /**
                 * 
                */
                addOutput(output, outputDesc);

                /**
                 * 
                */
                setOutputDevice(outputDesc,profileType,true,0,NULL,address);
            }
        }

        //////////////////////////////////////////////////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////////////////////////////////////

        // open input streams needed to access attached devices to validate
        // mAvailableInputDevices list
        for (const auto& inProfile : hwModule->getInputProfiles()) {
            /*
            */
            if (!inProfile->canOpenNewIo()) {
                ALOGE("Invalid Input profile max open count %u for profile %s", inProfile->maxOpenCount, inProfile->getTagName().c_str());
                continue;
            }
            if (!inProfile->hasSupportedDevices()) {
                ALOGW("Input profile contains no device on module %s", hwModule->getName());
                continue;
            }

             /**
             * HwModule::setRoutes(const AudioRouteVector &routes) --> HwModule::refreshSupportedDevices() --> IOProfile::setSupportedDevices(const DeviceVector &devices)
             * 这里 getSupportedDevicesType 是调用 DeviceDescriptor 的 mDeviceType（匹配配置文件中 type 属性值）
             * 
             * profileType 为 AUDIO_DEVICE_IN_BUILTIN_MIC
            */
            // chose first device present in profile's SupportedDevices also part of
            // inputDeviceTypes
            audio_devices_t profileType = inProfile->getSupportedDeviceForType(inputDeviceTypes);

            if ((profileType & inputDeviceTypes) == 0) {
                continue;
            }
            sp<AudioInputDescriptor> inputDesc =  new AudioInputDescriptor(inProfile, mpClientInterface);

            DeviceVector inputDevices = mAvailableInputDevices.getDevicesFromType(profileType);
            //   the inputs vector must be of size >= 1, but we don't want to crash here
            String8 address = inputDevices.size() > 0 ? inputDevices.itemAt(0)->mAddress : String8("");
            ALOGV("  for input device 0x%x using address %s", profileType, address.string());
            ALOGE_IF(inputDevices.size() == 0, "Input device list is empty!");

            audio_io_handle_t input = AUDIO_IO_HANDLE_NONE;
            status_t status = inputDesc->open(nullptr,profileType,address,AUDIO_SOURCE_MIC,AUDIO_INPUT_FLAG_NONE,&input);

            if (status == NO_ERROR) {
                for (const auto& dev : inProfile->getSupportedDevices()) {
                    ssize_t index = mAvailableInputDevices.indexOf(dev);
                    // give a valid ID to an attached device once confirmed it is reachable
                    if (index >= 0) {
                        sp<DeviceDescriptor> devDesc = mAvailableInputDevices[index];
                        if (!devDesc->isAttached()) {
                            devDesc->attach(hwModule);
                            devDesc->importAudioPort(inProfile, true);
                        }
                    }
                }
                inputDesc->close();
            } else {
                ALOGW("Cannot open input stream for device %08x on hw module %s",profileType,hwModule->getName());
            }
        }
    }

    // make sure all attached devices have been allocated a unique ID
    for (size_t i = 0; i  < mAvailableOutputDevices.size();) {
        if (!mAvailableOutputDevices[i]->isAttached()) {
            ALOGW("Output device %08x unreachable", mAvailableOutputDevices[i]->type());
            mAvailableOutputDevices.remove(mAvailableOutputDevices[i]);
            continue;
        }
        // The device is now validated and can be appended to the available devices of the engine
        /**
         * --> ManagerInterfaceImpl::setDeviceConnectionState //空实现，什么也没有干
        */
        mEngine->setDeviceConnectionState(mAvailableOutputDevices[i],AUDIO_POLICY_DEVICE_STATE_AVAILABLE);
        i++;
    }

    for (size_t i = 0; i  < mAvailableInputDevices.size();) {
        if (!mAvailableInputDevices[i]->isAttached()) {
            ALOGW("Input device %08x unreachable", mAvailableInputDevices[i]->type());
            mAvailableInputDevices.remove(mAvailableInputDevices[i]);
            continue;
        }
        // The device is now validated and can be appended to the available devices of the engine
        /**
         * --> ManagerInterfaceImpl::setDeviceConnectionState //空实现，什么也没有干
        */
        mEngine->setDeviceConnectionState(mAvailableInputDevices[i],AUDIO_POLICY_DEVICE_STATE_AVAILABLE);
        i++;
    }

    // make sure default device is reachable
    if (mDefaultOutputDevice == 0 || mAvailableOutputDevices.indexOf(mDefaultOutputDevice) < 0) {
        ALOGE("Default device %08x is unreachable", mDefaultOutputDevice->type());
        status = NO_INIT;
    }

    // If microphones address is empty, set it according to device type
    for (size_t i = 0; i  < mAvailableInputDevices.size(); i++) {
        if (mAvailableInputDevices[i]->mAddress.isEmpty()) {
            if (mAvailableInputDevices[i]->type() == AUDIO_DEVICE_IN_BUILTIN_MIC) {
                /**
                 * system/media/audio/include/system/audio.h:1089:
                 * #define AUDIO_BOTTOM_MICROPHONE_ADDRESS "bottom"
                */
                mAvailableInputDevices[i]->mAddress = String8(AUDIO_BOTTOM_MICROPHONE_ADDRESS);
            } else if (mAvailableInputDevices[i]->type() == AUDIO_DEVICE_IN_BACK_MIC) {
                /**
                 * system/media/audio/include/system/audio.h:1091:
                 * #define AUDIO_BACK_MICROPHONE_ADDRESS "back"
                */
                mAvailableInputDevices[i]->mAddress = String8(AUDIO_BACK_MICROPHONE_ADDRESS);
            }
        }
    }

    if (mPrimaryOutput == 0) {
        ALOGE("Failed to open primary output");
        status = NO_INIT;
    }

    updateDevicesAndOutputs();
    return status;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * output 由 AudioFlinger 创建的线程  对应的map key
 * 
*/
void AudioPolicyManager::addOutput(audio_io_handle_t output, const sp<SwAudioOutputDescriptor>& outputDesc)
{
    /**
     * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:552:        
     * SwAudioOutputCollection mOutputs;
    */
    mOutputs.add(output, outputDesc);
    applyStreamVolumes(outputDesc, AUDIO_DEVICE_NONE, 0 /* delayMs */, true /* force */);
    updateMono(output); // update mono status when adding to output list
    selectOutputForMusicEffects();
    nextAudioPortGeneration();
}

void AudioPolicyManager::applyStreamVolumes(const sp<AudioOutputDescriptor>& outputDesc, audio_devices_t device /*=AUDIO_DEVICE_NONE*/, int delayMs, bool force)
{
    ALOGVV("applyStreamVolumes() for device %08x", device);
    /***
     * 音频流类型
     * system/media/audio/include/system/audio-base.h:36:    AUDIO_STREAM_REROUTING = 11,
     * 
     * system/media/audio/include/system/audio-base-utils.h:33:    AUDIO_STREAM_FOR_POLICY_CNT= AUDIO_STREAM_REROUTING + 1,
     * 
     * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPolicyConfig.h:143:    VolumeCurvesCollection *mVolumeCurves;
     * */
    for (int stream = 0; stream < AUDIO_STREAM_FOR_POLICY_CNT; stream++) {
        checkAndSetVolume((audio_stream_type_t)stream,
                          mVolumeCurves->getVolumeIndex((audio_stream_type_t)stream, device/*=AUDIO_DEVICE_NONE*/) /*=AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME*/,
                          outputDesc,
                          device,
                          delayMs,
                          force);
    }
}

status_t AudioPolicyManager::checkAndSetVolume(audio_stream_type_t stream,int index,const sp<AudioOutputDescriptor>& outputDesc,audio_devices_t device,int delayMs,bool force)
{   
    /*
    这里默认为 0 ，在构造函数中初始化
    */
    // do not change actual stream volume if the stream is muted
    if (outputDesc->mMuteCount[stream] != 0) {
        ALOGVV("checkAndSetVolume() stream %d muted count %d",stream, outputDesc->mMuteCount[stream]);
        return NO_ERROR;
    }

    /**
     * 
    */
    audio_policy_forced_cfg_t forceUseForComm = mEngine->getForceUse(AUDIO_POLICY_FORCE_FOR_COMMUNICATION);
    // do not change in call volume if bluetooth is connected and vice versa
    if ((stream == AUDIO_STREAM_VOICE_CALL && forceUseForComm == AUDIO_POLICY_FORCE_BT_SCO) ||
        (stream == AUDIO_STREAM_BLUETOOTH_SCO && forceUseForComm != AUDIO_POLICY_FORCE_BT_SCO)) {
        ALOGV("checkAndSetVolume() cannot set stream %d volume with force use = %d for comm", stream, forceUseForComm);
        return INVALID_OPERATION;
    }

    /**
     * 这里执行
     * */
    if (device == AUDIO_DEVICE_NONE) {
        /*
        */
        device = outputDesc->device();
    }

    float volumeDb = computeVolume(stream, index, device);
    if (outputDesc->isFixedVolume(device) ||
            // Force VoIP volume to max for bluetooth SCO
            ((stream == AUDIO_STREAM_VOICE_CALL || stream == AUDIO_STREAM_BLUETOOTH_SCO) &&
             (device & AUDIO_DEVICE_OUT_ALL_SCO) != 0)) {
        volumeDb = 0.0f;
    }

    /**
     * 
    */
    outputDesc->setVolume(volumeDb, stream, device, delayMs, force);

    if (stream == AUDIO_STREAM_VOICE_CALL || stream == AUDIO_STREAM_BLUETOOTH_SCO) {
        float voiceVolume;
        // Force voice volume to max for bluetooth SCO as volume is managed by the headset
        if (stream == AUDIO_STREAM_VOICE_CALL) {
            voiceVolume = (float)index/(float)mVolumeCurves->getVolumeIndexMax(stream);
        } else {
            voiceVolume = 1.0;
        }

        if (voiceVolume != mLastVoiceVolume) {
            mpClientInterface->setVoiceVolume(voiceVolume, delayMs);
            mLastVoiceVolume = voiceVolume;
        }
    }

    return NO_ERROR;
}


void AudioPolicyManager::updateMono(audio_io_handle_t output) {
    AudioParameter param;
    param.addInt(String8(AudioParameter::keyMonoOutput), (int)mMasterMono);
    /**
     * status_t AudioFlinger::setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs)
    */
    mpClientInterface->setParameters(output, param.toString());
}


audio_io_handle_t AudioPolicyManager::selectOutputForMusicEffects()
{
    // select one output among several suitable for global effects.
    // The priority is as follows:
    // 1: An offloaded output. If the effect ends up not being offloadable,
    //    AudioFlinger will invalidate the track and the offloaded output
    //    will be closed causing the effect to be moved to a PCM output.
    // 2: A deep buffer output
    // 3: The primary output
    // 4: the first output in the list

    routing_strategy strategy = getStrategy(AUDIO_STREAM_MUSIC);
    audio_devices_t device = getDeviceForStrategy(strategy, false /*fromCache*/);
    SortedVector<audio_io_handle_t> outputs = getOutputsForDevice(device, mOutputs);

    if (outputs.size() == 0) {
        return AUDIO_IO_HANDLE_NONE;
    }

    audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
    bool activeOnly = true;

    while (output == AUDIO_IO_HANDLE_NONE) {
        audio_io_handle_t outputOffloaded = AUDIO_IO_HANDLE_NONE;
        audio_io_handle_t outputDeepBuffer = AUDIO_IO_HANDLE_NONE;
        audio_io_handle_t outputPrimary = AUDIO_IO_HANDLE_NONE;

        for (audio_io_handle_t output : outputs) {
            sp<SwAudioOutputDescriptor> desc = mOutputs.valueFor(output);
            if (activeOnly && !desc->isStreamActive(AUDIO_STREAM_MUSIC)) {
                continue;
            }
            ALOGV("selectOutputForMusicEffects activeOnly %d output %d flags 0x%08x",activeOnly, output, desc->mFlags);
            if ((desc->mFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) != 0) {
                outputOffloaded = output;
            }
            if ((desc->mFlags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER) != 0) {
                outputDeepBuffer = output;
            }
            if ((desc->mFlags & AUDIO_OUTPUT_FLAG_PRIMARY) != 0) {
                outputPrimary = output;
            }
        }
        if (outputOffloaded != AUDIO_IO_HANDLE_NONE) {
            output = outputOffloaded;
        } else if (outputDeepBuffer != AUDIO_IO_HANDLE_NONE) {
            output = outputDeepBuffer;
        } else if (outputPrimary != AUDIO_IO_HANDLE_NONE) {
            output = outputPrimary;
        } else {
            output = outputs[0];
        }
        activeOnly = false;
    }

    if (output != mMusicEffectOutput) {
        mpClientInterface->moveEffects(AUDIO_SESSION_OUTPUT_MIX, mMusicEffectOutput, output);
        mMusicEffectOutput = output;
    }

    ALOGV("selectOutputForMusicEffects selected output %d", output);
    return output;
}

uint32_t AudioPolicyManager::nextAudioPortGeneration()
{
    return mAudioPortGeneration++;
}


/**
 * 
 * 
 * 
*/
uint32_t AudioPolicyManager::setOutputDevice(const sp<AudioOutputDescriptor>& outputDesc,audio_devices_t device,bool force,int delayMs,audio_patch_handle_t *patchHandle,const char *address,bool requiresMuteCheck)
{
    ALOGV("setOutputDevice() device %04x delayMs %d", device, delayMs);
    AudioParameter param;
    uint32_t muteWaitMs;

    if (outputDesc->isDuplicated()) {
        muteWaitMs = setOutputDevice(outputDesc->subOutput1(), device, force, delayMs, nullptr /* patchHandle */, nullptr /* address */, requiresMuteCheck);
        muteWaitMs += setOutputDevice(outputDesc->subOutput2(), device, force, delayMs, nullptr /* patchHandle */, nullptr /* address */, requiresMuteCheck);
        return muteWaitMs;
    }
    // no need to proceed if new device is not AUDIO_DEVICE_NONE and not supported by current
    // output profile
    if ((device != AUDIO_DEVICE_NONE) && ((device & outputDesc->supportedDevices()) == AUDIO_DEVICE_NONE)) {
        return 0;
    }

    // filter devices according to output selected
    device = (audio_devices_t)(device & outputDesc->supportedDevices());

    audio_devices_t prevDevice = outputDesc->mDevice;

    ALOGV("setOutputDevice() prevDevice 0x%04x", prevDevice);

    if (device != AUDIO_DEVICE_NONE) {
        outputDesc->mDevice = device;
    }

    // if the outputs are not materially active, there is no need to mute.
    if (requiresMuteCheck) {
        muteWaitMs = checkDeviceMuteStrategies(outputDesc, prevDevice, delayMs);
    } else {
        ALOGV("%s: suppressing checkDeviceMuteStrategies", __func__);
        muteWaitMs = 0;
    }

    // Do not change the routing if:
    //      the requested device is AUDIO_DEVICE_NONE
    //      OR the requested device is the same as current device
    //  AND force is not specified
    //  AND the output is connected by a valid audio patch.
    // Doing this check here allows the caller to call setOutputDevice() without conditions
    if ((device == AUDIO_DEVICE_NONE || device == prevDevice) &&　!force &&　outputDesc->getPatchHandle() != 0) {
        ALOGV("setOutputDevice() setting same device 0x%04x or null device", device);
        return muteWaitMs;
    }

    ALOGV("setOutputDevice() changing device");

    // do the routing
    if (device == AUDIO_DEVICE_NONE) {
        resetOutputDevice(outputDesc, delayMs, NULL);
    } else {
        DeviceVector deviceList;
        if ((address == NULL) || (strlen(address) == 0)) {
            deviceList = mAvailableOutputDevices.getDevicesFromType(device);
        } else {
            deviceList = mAvailableOutputDevices.getDevicesFromTypeAddr(device, String8(address));
        }

        if (!deviceList.isEmpty()) {
            struct audio_patch patch;
            outputDesc->toAudioPortConfig(&patch.sources[0]);
            patch.num_sources = 1;
            patch.num_sinks = 0;
            for (size_t i = 0; i < deviceList.size() && i < AUDIO_PATCH_PORTS_MAX; i++) {
                deviceList.itemAt(i)->toAudioPortConfig(&patch.sinks[i]);
                patch.num_sinks++;
            }
            ssize_t index;
            if (patchHandle && *patchHandle != AUDIO_PATCH_HANDLE_NONE) {
                index = mAudioPatches.indexOfKey(*patchHandle);
            } else {
                index = mAudioPatches.indexOfKey(outputDesc->getPatchHandle());
            }
            sp< AudioPatch> patchDesc;
            audio_patch_handle_t afPatchHandle = AUDIO_PATCH_HANDLE_NONE;
            if (index >= 0) {
                patchDesc = mAudioPatches.valueAt(index);
                afPatchHandle = patchDesc->mAfPatchHandle;
            }

            status_t status = mpClientInterface->createAudioPatch(&patch,&afPatchHandle,delayMs);
            ALOGV("setOutputDevice() createAudioPatch returned %d patchHandle %d""num_sources %d num_sinks %d",status, afPatchHandle, patch.num_sources, patch.num_sinks);
            if (status == NO_ERROR) {
                if (index < 0) {
                    patchDesc = new AudioPatch(&patch, mUidCached);
                    addAudioPatch(patchDesc->mHandle, patchDesc);
                } else {
                    patchDesc->mPatch = patch;
                }
                patchDesc->mAfPatchHandle = afPatchHandle;
                if (patchHandle) {
                    *patchHandle = patchDesc->mHandle;
                }
                outputDesc->setPatchHandle(patchDesc->mHandle);
                nextAudioPortGeneration();
                mpClientInterface->onAudioPatchListUpdate();
            }
        }

        // inform all input as well
        for (size_t i = 0; i < mInputs.size(); i++) {
            const sp<AudioInputDescriptor>  inputDescriptor = mInputs.valueAt(i);
            if (!is_virtual_input_device(inputDescriptor->mDevice)) {
                AudioParameter inputCmd = AudioParameter();
                ALOGV("%s: inform inut %d of device:%d", __func__, inputDescriptor->mIoHandle, device);
                inputCmd.addInt(Strinpg8(AudioParameter::keyRouting),device);
                mpClientInterface->setParameters(inputDescriptor->mIoHandle,inputCmd.toString(),delayMs);
            }
        }
    }

    // update stream volumes according to new device
    applyStreamVolumes(outputDesc, device, delayMs);

    return muteWaitMs;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * AudioPolicyManager::AudioPolicyManager()
 * --> AudioPolicyManager::initialize()
 * ---> AudioPolicyManager::updateDevicesAndOutputs()
 * 
*/
void AudioPolicyManager::updateDevicesAndOutputs()
{
    //  frameworks/av/services/audiopolicy/common/include/RoutingStrategy.h:35:    NUM_STRATEGIES
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        mDeviceForStrategy[i] = getDeviceForStrategy((routing_strategy)i, false /*fromCache*/);
    }
    mPreviousOutputs = mOutputs;
}


/**
 * AudioPolicyManager::AudioPolicyManager()
 * --> AudioPolicyManager::initialize()
 * ---> AudioPolicyManager::updateDevicesAndOutputs()
 * ----> AudioPolicyManager::getDeviceForStrategy()
*/
audio_devices_t AudioPolicyManager::getDeviceForStrategy(routing_strategy strategy,bool fromCache)
{
    // Check if an explicit routing request exists for a stream type corresponding to the
    // specified strategy and use it in priority over default routing rules.
    for (int stream = 0; stream < AUDIO_STREAM_FOR_POLICY_CNT; stream++) {
        if (getStrategy((audio_stream_type_t)stream) == strategy) {
            /**
             * @ frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:561:        
             * SessionRouteMap mOutputRoutes = SessionRouteMap(SessionRouteMap::MAPTYPE_OUTPUT);
            */
            audio_devices_t forcedDevice = mOutputRoutes.getActiveDeviceForStream((audio_stream_type_t)stream, mAvailableOutputDevices);
            if (forcedDevice != AUDIO_DEVICE_NONE) {
                return forcedDevice;
            }
        }
    }

    if (fromCache) {
        ALOGVV("getDeviceForStrategy() from cache strategy %d, device %x", strategy, mDeviceForStrategy[strategy]);
        return mDeviceForStrategy[strategy];
    }
    return mEngine->getDeviceForStrategy(strategy);
}











///////////////////////////////////////////////////////////////////////////////////
/**
 * ActivityManagerService
 * --> UidPolicy::onUidActive/onUidGone/onUidIdle
 * ---> AudioPolicyService::UidPolicy::notifyService
 * ----> AudioPolicyService::setRecordSilenced
*/
void AudioPolicyManager::setRecordSilenced(uid_t uid, bool silenced)
{
    ALOGV("AudioPolicyManager:setRecordSilenced(uid:%d, silenced:%d)", uid, silenced);

    Vector<sp<AudioInputDescriptor> > activeInputs = mInputs.getActiveInputs();
    for (size_t i = 0; i < activeInputs.size(); i++) {
        sp<AudioInputDescriptor> activeDesc = activeInputs[i];
        AudioSessionCollection activeSessions = activeDesc->getAudioSessions(true);
        for (size_t j = 0; j < activeSessions.size(); j++) {
            sp<AudioSession> activeSession = activeSessions.valueAt(j);
            if (activeSession->uid() == uid) {
                activeSession->setSilenced(silenced);
            }
        }
    }
}
















////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
*/
status_t AudioPolicyManager::getOutputForAttr(const audio_attributes_t *attr,
                                              audio_io_handle_t *output,
                                              audio_session_t session,
                                              audio_stream_type_t *stream,
                                              uid_t uid,
                                              const audio_config_t *config,
                                              audio_output_flags_t *flags,
                                              audio_port_handle_t *selectedDeviceId,
                                              audio_port_handle_t *portId)
{
    audio_attributes_t attributes;
    if (attr != NULL) {
        if (!isValidAttributes(attr)) {
            ALOGE("getOutputForAttr() invalid attributes: usage=%d content=%d flags=0x%x tags=[%s]", attr->usage, attr->content_type, attr->flags,attr->tags);
            return BAD_VALUE;
        }
        attributes = *attr;
    } else {
        if (*stream < AUDIO_STREAM_MIN || *stream >= AUDIO_STREAM_PUBLIC_CNT) {
            ALOGE("getOutputForAttr():  invalid stream type");
            return BAD_VALUE;
        }
        stream_type_to_audio_attributes(*stream, &attributes);
    }

    // TODO: check for existing client for this port ID
    if (*portId == AUDIO_PORT_HANDLE_NONE) {
        *portId = AudioPort::getNextUniqueId();
    }

   /**
    * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:603:        AudioPolicyMixCollection mPolicyMixes; // list of registered mixes
    * 
   */
    sp<SwAudioOutputDescriptor> desc;
    if (mPolicyMixes.getOutputForAttr(attributes, uid, desc) == NO_ERROR) {
        ALOG_ASSERT(desc != 0, "Invalid desc returned by getOutputForAttr");
        if (!audio_has_proportional_frames(config->format)) {
            return BAD_VALUE;
        }
        *stream = streamTypefromAttributesInt(&attributes);
        *output = desc->mIoHandle;

        routing_strategy strategy = (routing_strategy) getStrategyForAttr(&attributes);
        audio_devices_t device = getDeviceForStrategy(strategy, false /*fromCache*/);

        DeviceVector outputDevices = mAvailableOutputDevices.getDevicesFromType(device);
        *selectedDeviceId = outputDevices.size() > 0 ? outputDevices.itemAt(0)->getId()  : AUDIO_PORT_HANDLE_NONE;
        ALOGV("getOutputForAttr() returns output %d", *output);
        return NO_ERROR;
    }

    if (attributes.usage == AUDIO_USAGE_VIRTUAL_SOURCE) {
        ALOGW("getOutputForAttr() no policy mix found for usage AUDIO_USAGE_VIRTUAL_SOURCE");
        return BAD_VALUE;
    }

    ALOGV("getOutputForAttr() usage=%d, content=%d, tag=%s flags=%08x"  " session %d selectedDeviceId %d",attributes.usage, attributes.content_type, attributes.tags, attributes.flags, session, *selectedDeviceId);

   /**
    * streamTypefromAttributesInt @ frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
    * 通过 attributes->usage 或者 attributes->flags 转化成对应的 流类型 audio_stream_type_t（类似 AUDIO_STREAM_MUSIC）
   */
    *stream = streamTypefromAttributesInt(&attributes);

    // Explicit routing?
    sp<DeviceDescriptor> deviceDesc;
    if (*selectedDeviceId != AUDIO_PORT_HANDLE_NONE) {
        deviceDesc = mAvailableOutputDevices.getDeviceFromId(*selectedDeviceId);
    }
    mOutputRoutes.addRoute(session, *stream, SessionRoute::SOURCE_TYPE_NA, deviceDesc, uid);
   
   /**
    * getStrategyForAttr        @       frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
    * 通过 attributes->usage 或者 attributes->flags 转化成对应的 routing_strategy 类型
   */
    routing_strategy strategy = (routing_strategy) getStrategyForAttr(&attributes);
    /**
     * 
    */
    audio_devices_t device = getDeviceForStrategy(strategy, false /*fromCache*/);

    if ((attributes.flags & AUDIO_FLAG_HW_AV_SYNC) != 0) {
        *flags = (audio_output_flags_t)(*flags | AUDIO_OUTPUT_FLAG_HW_AV_SYNC);
    }

    // Set incall music only if device was explicitly set, and fallback to the device which is
    // chosen by the engine if not.
    // FIXME: provide a more generic approach which is not device specific and move this back
    // to getOutputForDevice.
    if (device == AUDIO_DEVICE_OUT_TELEPHONY_TX && *stream == AUDIO_STREAM_MUSIC && audio_is_linear_pcm(config->format) && isInCall()) {
        if (*selectedDeviceId != AUDIO_PORT_HANDLE_NONE) {
            *flags = (audio_output_flags_t)AUDIO_OUTPUT_FLAG_INCALL_MUSIC;
        } else {
            device = mEngine->getDeviceForStrategy(strategy);
        }
    }

    ALOGV("getOutputForAttr() device 0x%x, sampling rate %d, format %#x, channel mask %#x, " "flags %#x", device, config->sample_rate, config->format, config->channel_mask, *flags);
   
   /**
    * *output 对应输出线程id 类似 MixerThread
   */
    *output = getOutputForDevice(device, session, *stream, config, flags);
    if (*output == AUDIO_IO_HANDLE_NONE) {
        mOutputRoutes.removeRoute(session);
        return INVALID_OPERATION;
    }

    DeviceVector outputDevices = mAvailableOutputDevices.getDevicesFromType(device);
    *selectedDeviceId = outputDevices.size() > 0 ? outputDevices.itemAt(0)->getId() : AUDIO_PORT_HANDLE_NONE;

    ALOGV("  getOutputForAttr() returns output %d selectedDeviceId %d", *output, *selectedDeviceId);

    return NO_ERROR;
}


audio_io_handle_t AudioPolicyManager::getOutputForDevice(audio_devices_t device,audio_session_t session,audio_stream_type_t stream,const audio_config_t *config,audio_output_flags_t *flags)
{
    audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
    status_t status;

    // open a direct output if required by specified parameters
    //force direct flag if offload flag is set: offloading implies a direct output stream
    // and all common behaviors are driven by checking only the direct flag
    // this should normally be set appropriately in the policy configuration file
    if ((*flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) != 0) {
        *flags = (audio_output_flags_t)(*flags | AUDIO_OUTPUT_FLAG_DIRECT);
    }
    if ((*flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) != 0) {
        *flags = (audio_output_flags_t)(*flags | AUDIO_OUTPUT_FLAG_DIRECT);
    }
    // only allow deep buffering for music stream type
    if (stream != AUDIO_STREAM_MUSIC) {
        *flags = (audio_output_flags_t)(*flags &~AUDIO_OUTPUT_FLAG_DEEP_BUFFER);
    } else if (/* stream == AUDIO_STREAM_MUSIC && */  *flags == AUDIO_OUTPUT_FLAG_NONE &&  property_get_bool("audio.deep_buffer.media", false /* default_value */)) {
        // use DEEP_BUFFER as default output for music stream type
        *flags = (audio_output_flags_t)AUDIO_OUTPUT_FLAG_DEEP_BUFFER;
    }
    if (stream == AUDIO_STREAM_TTS) {
        *flags = AUDIO_OUTPUT_FLAG_TTS;
    } else if (stream == AUDIO_STREAM_VOICE_CALL && audio_is_linear_pcm(config->format)) {
        *flags = (audio_output_flags_t)(AUDIO_OUTPUT_FLAG_VOIP_RX | AUDIO_OUTPUT_FLAG_DIRECT);
        ALOGV("Set VoIP and Direct output flags for PCM format");
    }


    sp<IOProfile> profile;

    // skip direct output selection if the request can obviously be attached to a mixed output
    // and not explicitly requested
    if (((*flags & AUDIO_OUTPUT_FLAG_DIRECT) == 0) &&
            audio_is_linear_pcm(config->format) && config->sample_rate <= SAMPLE_RATE_HZ_MAX &&
            audio_channel_count_from_out_mask(config->channel_mask) <= 2) {
        goto non_direct_output;
    }

    // Do not allow offloading if one non offloadable effect is enabled or MasterMono is enabled.
    // This prevents creating an offloaded track and tearing it down immediately after start
    // when audioflinger detects there is an active non offloadable effect.
    // FIXME: We should check the audio session here but we do not have it in this context.
    // This may prevent offloading in rare situations where effects are left active by apps
    // in the background.

    if (((*flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) == 0) || !(mEffects.isNonOffloadableEffectEnabled() || mMasterMono)) {
        profile = getProfileForDirectOutput(device,config->sample_rate, config->format,config->channel_mask,  (audio_output_flags_t)*flags);
    }

    if (profile != 0) {
        // exclusive outputs for MMAP and Offload are enforced by different session ids.
        for (size_t i = 0; i < mOutputs.size(); i++) {
            sp<SwAudioOutputDescriptor> desc = mOutputs.valueAt(i);
            if (!desc->isDuplicated() && (profile == desc->mProfile)) {
                // reuse direct output if currently open by the same client
                // and configured with same parameters
                if ((config->sample_rate == desc->mSamplingRate) &&
                    (config->format == desc->mFormat) &&
                    (config->channel_mask == desc->mChannelMask) &&
                    (session == desc->mDirectClientSession)) {
                    desc->mDirectOpenCount++;
                    ALOGI("getOutputForDevice() reusing direct output %d for session %d",mOutputs.keyAt(i), session);
                    return mOutputs.keyAt(i);
                }
            }
        }

        if (!profile->canOpenNewIo()) {
            goto non_direct_output;
        }

        sp<SwAudioOutputDescriptor> outputDesc = new SwAudioOutputDescriptor(profile, mpClientInterface);

        DeviceVector outputDevices = mAvailableOutputDevices.getDevicesFromType(device);
        String8 address = outputDevices.size() > 0 ? outputDevices.itemAt(0)->mAddress : String8("");

        status = outputDesc->open(config, device, address, stream, *flags, &output);

        // only accept an output with the requested parameters
        if (status != NO_ERROR ||
            (config->sample_rate != 0 && config->sample_rate != outputDesc->mSamplingRate) ||
            (config->format != AUDIO_FORMAT_DEFAULT && config->format != outputDesc->mFormat) ||
            (config->channel_mask != 0 && config->channel_mask != outputDesc->mChannelMask)) {
            ALOGV("getOutputForDevice() failed opening direct output: output %d sample rate %d %d,"
                    "format %d %d, channel mask %04x %04x", output, config->sample_rate,
                    outputDesc->mSamplingRate, config->format, outputDesc->mFormat,
                    config->channel_mask, outputDesc->mChannelMask);
            if (output != AUDIO_IO_HANDLE_NONE) {
                outputDesc->close();
            }
            // fall back to mixer output if possible when the direct output could not be open
            if (audio_is_linear_pcm(config->format) &&   config->sample_rate  <= SAMPLE_RATE_HZ_MAX) {
                goto non_direct_output;
            }
            return AUDIO_IO_HANDLE_NONE;
        }
        outputDesc->mRefCount[stream] = 0;
        outputDesc->mStopTime[stream] = 0;
        outputDesc->mDirectOpenCount = 1;
        outputDesc->mDirectClientSession = session;

        addOutput(output, outputDesc);
        mPreviousOutputs = mOutputs;
        ALOGV("getOutputForDevice() returns new direct output %d", output);
        mpClientInterface->onAudioPortListUpdate();
        return output;
    }

non_direct_output:

    // A request for HW A/V sync cannot fallback to a mixed output because time
    // stamps are embedded in audio data
    if ((*flags & (AUDIO_OUTPUT_FLAG_HW_AV_SYNC | AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) != 0) {
        return AUDIO_IO_HANDLE_NONE;
    }

    // ignoring channel mask due to downmix capability in mixer

    // open a non direct output

    // for non direct outputs, only PCM is supported
    if (audio_is_linear_pcm(config->format)) {
        // get which output is suitable for the specified stream. The actual
        // routing change will happen when startOutput() will be called
        SortedVector<audio_io_handle_t> outputs = getOutputsForDevice(device, mOutputs);

        // at this stage we should ignore the DIRECT flag as no direct output could be found earlier
        *flags = (audio_output_flags_t)(*flags & ~AUDIO_OUTPUT_FLAG_DIRECT);
        output = selectOutput(outputs, *flags, config->format);
    }
    ALOGW_IF((output == 0), "getOutputForDevice() could not find output for stream %d, "  "sampling rate %d, format %#x, channels %#x, flags %#x",stream, config->sample_rate, config->format, config->channel_mask, *flags);

    return output;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 第一次调用　status = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE,AUDIO_PORT_TYPE_NONE,&numPorts,NULL,&generation1);
 * 
 * 第二次调用    status = AudioSystem::listAudioPorts(AUDIO_PORT_ROLE_NONE,AUDIO_PORT_TYPE_NONE,&numPorts,nPorts, &generation);
*/
status_t AudioPolicyManager::listAudioPorts(audio_port_role_t role,audio_port_type_t type, unsigned int *num_ports, struct audio_port *ports,unsigned int *generation)
{
    if (num_ports == NULL || (*num_ports != 0 && ports == NULL) ||
            generation == NULL) {
        return BAD_VALUE;
    }
    ALOGV("listAudioPorts() role %d type %d num_ports %d ports %p", role, type, *num_ports, ports);
    if (ports == NULL) {
        *num_ports = 0;
    }

    size_t portsWritten = 0;
    size_t portsMax = *num_ports;
    *num_ports = 0;
    if (type == AUDIO_PORT_TYPE_NONE || type == AUDIO_PORT_TYPE_DEVICE) {
        // do not report devices with type AUDIO_DEVICE_IN_STUB or AUDIO_DEVICE_OUT_STUB
        // as they are used by stub HALs by convention
        if (role == AUDIO_PORT_ROLE_SINK || role == AUDIO_PORT_ROLE_NONE) {
           /**
            * 有效输出设备
            * 在　 <attachedDevices>　标签中包含的　devicePort(只可能是devicePort)
            * 这里只有　Built-In Mic 和　 Remote Submix In
            * 
            * class DeviceVector : public SortedVector<sp<DeviceDescriptor> >
           */
            for (const auto& dev : mAvailableOutputDevices) {
                if (dev->type() == AUDIO_DEVICE_OUT_STUB) {
                    continue;
                }
                if (portsWritten < portsMax) {
                    /**
                     * 由于 DeviceDescriptor 继承 AudioPortConfig
                    */
                    dev->toAudioPort(&ports[portsWritten++]);
                }
                (*num_ports)++;
            }
        }

        if (role == AUDIO_PORT_ROLE_SOURCE || role == AUDIO_PORT_ROLE_NONE) {
        　/**
            *   在　 <attachedDevices>　标签中包含的　devicePort(只可能是devicePort)
            * 这里只有　bus0_media_out          bus1_system_sound_out
           */
            for (const auto& dev : mAvailableInputDevices) {
                if (dev->type() == AUDIO_DEVICE_IN_STUB) {
                    continue;
                }
                if (portsWritten < portsMax) {
                    /**
                     * 由于 DeviceDescriptor 继承 AudioPortConfig
                    */
                    dev->toAudioPort(&ports[portsWritten++]);
                }
                (*num_ports)++;
            }
        }
    }


    if (type == AUDIO_PORT_TYPE_NONE || type == AUDIO_PORT_TYPE_MIX) {
        /**
         * 
        */
        if (role == AUDIO_PORT_ROLE_SINK || role == AUDIO_PORT_ROLE_NONE) {
            for (size_t i = 0; i < mInputs.size() && portsWritten < portsMax; i++) {
                 /**
                     * 由于 AudioInputDescriptor 继承 AudioPortConfig
                     * 
                     * mInputs 添加 成员 只有一处调用  void AudioPolicyManager::addInput(audio_io_handle_t input,const sp<AudioInputDescriptor>& inputDesc)
                    */
                mInputs[i]->toAudioPort(&ports[portsWritten++]);
            }
            *num_ports += mInputs.size();

        }


        if (role == AUDIO_PORT_ROLE_SOURCE || role == AUDIO_PORT_ROLE_NONE) {
        　/**
                 * @    frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:552:        SwAudioOutputCollection mOutputs;
                */
            size_t numOutputs = 0;
            for (size_t i = 0; i < mOutputs.size(); i++) {
                if (!mOutputs[i]->isDuplicated()) {
                    numOutputs++;
                    if (portsWritten < portsMax) {
                   /**
                     * 由于 SwAudioOutputDescriptor  继承 AudioPortConfig
                     * mOutputs 添加 成员 只有一处调用  void AudioPolicyManager::addOutput(audio_io_handle_t output,const sp<SwAudioOutputDescriptor>& outputDesc)
                    */
                        mOutputs[i]->toAudioPort(&ports[portsWritten++]);
                    }
                }
            }
            *num_ports += numOutputs;
        }

    }

    /**
     * 返回　return mAudioPortGeneration;
    */
    *generation = curAudioPortGeneration();
    ALOGV("listAudioPorts() got %zu ports needed %d", portsWritten, *num_ports);
    return NO_ERROR;
}


/**
 * audio_io_handle_t 对应的 AudioFlinger 中创建的 RecordThread 
*/
void AudioPolicyManager::addInput(audio_io_handle_t input,const sp<AudioInputDescriptor>& inputDesc)
{
    mInputs.add(input, inputDesc);
    nextAudioPortGeneration();
}

/**
 * audio_io_handle_t 对应的 AudioFlinger 中创建的 PlaybackThread 
*/
void AudioPolicyManager::addOutput(audio_io_handle_t output, const sp<SwAudioOutputDescriptor>& outputDesc)
{
    mOutputs.add(output, outputDesc);
    applyStreamVolumes(outputDesc, AUDIO_DEVICE_NONE, 0 /* delayMs */, true /* force */);
    updateMono(output); // update mono status when adding to output list
    selectOutputForMusicEffects();
    nextAudioPortGeneration();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
status_t AudioPolicyManager::createAudioPatch(const struct audio_patch *patch,audio_patch_handle_t *handle,uid_t uid)
{
    ALOGV("createAudioPatch()");

    if (handle == NULL || patch == NULL) {
        return BAD_VALUE;
    }
    ALOGV("createAudioPatch() num sources %d num sinks %d", patch->num_sources, patch->num_sinks);

    if (patch->num_sources == 0 || patch->num_sources > AUDIO_PATCH_PORTS_MAX || patch->num_sinks == 0 || patch->num_sinks > AUDIO_PATCH_PORTS_MAX) {
        return BAD_VALUE;
    }
    // only one source per audio patch supported for now
    if (patch->num_sources > 1) {
        return INVALID_OPERATION;
    }

    if (patch->sources[0].role != AUDIO_PORT_ROLE_SOURCE) {
        return INVALID_OPERATION;
    }
    for (size_t i = 0; i < patch->num_sinks; i++) {
        if (patch->sinks[i].role != AUDIO_PORT_ROLE_SINK) {
            return INVALID_OPERATION;
        }
    }

    sp<AudioPatch> patchDesc;
    ssize_t index = mAudioPatches.indexOfKey(*handle);

    ALOGV("createAudioPatch source id %d role %d type %d", patch->sources[0].id,patch->sources[0].role,patch->sources[0].type);
#if LOG_NDEBUG == 0
    for (size_t i = 0; i < patch->num_sinks; i++) {
        ALOGV("createAudioPatch sink %zu: id %d role %d type %d", i, patch->sinks[i].id,patch->sinks[i].role,patch->sinks[i].type);
    }
#endif

    if (index >= 0) {
        patchDesc = mAudioPatches.valueAt(index);
        ALOGV("createAudioPatch() mUidCached %d patchDesc->mUid %d uid %d", mUidCached, patchDesc->mUid, uid);
        if (patchDesc->mUid != mUidCached && uid != patchDesc->mUid) {
            return INVALID_OPERATION;
        }
    } else {
        *handle = AUDIO_PATCH_HANDLE_NONE;
    }

    if (patch->sources[0].type == AUDIO_PORT_TYPE_MIX) {
        sp<SwAudioOutputDescriptor> outputDesc = mOutputs.getOutputFromId(patch->sources[0].id);
        if (outputDesc == NULL) {
            ALOGV("createAudioPatch() output not found for id %d", patch->sources[0].id);
            return BAD_VALUE;
        }
        ALOG_ASSERT(!outputDesc->isDuplicated(),"duplicated output %d in source in ports",outputDesc->mIoHandle);
        if (patchDesc != 0) {
            if (patchDesc->mPatch.sources[0].id != patch->sources[0].id) {
                ALOGV("createAudioPatch() source id differs for patch current id %d new id %d",patchDesc->mPatch.sources[0].id, patch->sources[0].id);
                return BAD_VALUE;
            }
        }
        DeviceVector devices;
        for (size_t i = 0; i < patch->num_sinks; i++) {
            // Only support mix to devices connection
            // TODO add support for mix to mix connection
            if (patch->sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
                ALOGV("createAudioPatch() source mix but sink is not a device");
                return INVALID_OPERATION;
            }
            sp<DeviceDescriptor> devDesc = mAvailableOutputDevices.getDeviceFromId(patch->sinks[i].id);
            if (devDesc == 0) {
                ALOGV("createAudioPatch() out device not found for id %d", patch->sinks[i].id);
                return BAD_VALUE;
            }

            if (!outputDesc->mProfile->isCompatibleProfile(devDesc->type(),
                                                           devDesc->mAddress,
                                                           patch->sources[0].sample_rate,
                                                           NULL,  // updatedSamplingRate
                                                           patch->sources[0].format,
                                                           NULL,  // updatedFormat
                                                           patch->sources[0].channel_mask,
                                                           NULL,  // updatedChannelMask
                                                           AUDIO_OUTPUT_FLAG_NONE /*FIXME*/)) {
                ALOGV("createAudioPatch() profile not supported for device %08x",devDesc->type());
                return INVALID_OPERATION;
            }
            devices.add(devDesc);
        }
        if (devices.size() == 0) {
            return INVALID_OPERATION;
        }

        // TODO: reconfigure output format and channels here
        ALOGV("createAudioPatch() setting device %08x on output %d", devices.types(), outputDesc->mIoHandle);
        
        setOutputDevice(outputDesc, devices.types(), true, 0, handle);
        index = mAudioPatches.indexOfKey(*handle);
        if (index >= 0) {
            if (patchDesc != 0 && patchDesc != mAudioPatches.valueAt(index)) {
                ALOGW("createAudioPatch() setOutputDevice() did not reuse the patch provided");
            }
            patchDesc = mAudioPatches.valueAt(index);
            patchDesc->mUid = uid;
            ALOGV("createAudioPatch() success");
        } else {
            ALOGW("createAudioPatch() setOutputDevice() failed to create a patch");
            return INVALID_OPERATION;
        }
    } else if (patch->sources[0].type == AUDIO_PORT_TYPE_DEVICE) {
        if (patch->sinks[0].type == AUDIO_PORT_TYPE_MIX) {
            // input device to input mix connection
            // only one sink supported when connecting an input device to a mix
            if (patch->num_sinks > 1) {
                return INVALID_OPERATION;
            }
            sp<AudioInputDescriptor> inputDesc = mInputs.getInputFromId(patch->sinks[0].id);
            if (inputDesc == NULL) {
                return BAD_VALUE;
            }
            if (patchDesc != 0) {
                if (patchDesc->mPatch.sinks[0].id != patch->sinks[0].id) {
                    return BAD_VALUE;
                }
            }
            sp<DeviceDescriptor> devDesc =　mAvailableInputDevices.getDeviceFromId(patch->sources[0].id);
            if (devDesc == 0) {
                return BAD_VALUE;
            }

            if (!inputDesc->mProfile->isCompatibleProfile(devDesc->type(),
                                                          devDesc->mAddress,
                                                          patch->sinks[0].sample_rate,
                                                          NULL, /*updatedSampleRate*/
                                                          patch->sinks[0].format,
                                                          NULL, /*updatedFormat*/
                                                          patch->sinks[0].channel_mask,
                                                          NULL, /*updatedChannelMask*/
                                                          // FIXME for the parameter type,
                                                          // and the NONE
                                                          (audio_output_flags_t)
                                                            AUDIO_INPUT_FLAG_NONE)) {
                return INVALID_OPERATION;
            }
            // TODO: reconfigure output format and channels here
            ALOGV("createAudioPatch() setting device %08x on output %d",devDesc->type(), inputDesc->mIoHandle);
            setInputDevice(inputDesc->mIoHandle, devDesc->type(), true, handle);
            index = mAudioPatches.indexOfKey(*handle);
            if (index >= 0) {
                if (patchDesc != 0 && patchDesc != mAudioPatches.valueAt(index)) {
                    ALOGW("createAudioPatch() setInputDevice() did not reuse the patch provided");
                }
                patchDesc = mAudioPatches.valueAt(index);
                patchDesc->mUid = uid;
                ALOGV("createAudioPatch() success");
            } else {
                ALOGW("createAudioPatch() setInputDevice() failed to create a patch");
                return INVALID_OPERATION;
            }
        } else if (patch->sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
            // device to device connection
            if (patchDesc != 0) {
                if (patchDesc->mPatch.sources[0].id != patch->sources[0].id) {
                    return BAD_VALUE;
                }
            }
            sp<DeviceDescriptor> srcDeviceDesc = mAvailableInputDevices.getDeviceFromId(patch->sources[0].id);
            if (srcDeviceDesc == 0) {
                return BAD_VALUE;
            }

            //update source and sink with our own data as the data passed in the patch may
            // be incomplete.
            struct audio_patch newPatch = *patch;
            srcDeviceDesc->toAudioPortConfig(&newPatch.sources[0], &patch->sources[0]);

            for (size_t i = 0; i < patch->num_sinks; i++) {
                if (patch->sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
                    ALOGV("createAudioPatch() source device but one sink is not a device");
                    return INVALID_OPERATION;
                }

                sp<DeviceDescriptor> sinkDeviceDesc = mAvailableOutputDevices.getDeviceFromId(patch->sinks[i].id);
                if (sinkDeviceDesc == 0) {
                    return BAD_VALUE;
                }
                sinkDeviceDesc->toAudioPortConfig(&newPatch.sinks[i], &patch->sinks[i]);

                // create a software bridge in PatchPanel if:
                // - source and sink devices are on different HW modules OR
                // - audio HAL version is < 3.0
                if (!srcDeviceDesc->hasSameHwModuleAs(sinkDeviceDesc) ||
                        (srcDeviceDesc->mModule->getHalVersionMajor() < 3)) {
                    // support only one sink device for now to simplify output selection logic
                    if (patch->num_sinks > 1) {
                        return INVALID_OPERATION;
                    }
                    SortedVector<audio_io_handle_t> outputs =　getOutputsForDevice(sinkDeviceDesc->type(), mOutputs);
                    // if the sink device is reachable via an opened output stream, request to go via
                    // this output stream by adding a second source to the patch description
                    audio_io_handle_t output = selectOutput(outputs,AUDIO_OUTPUT_FLAG_NONE,AUDIO_FORMAT_INVALID);
                    if (output != AUDIO_IO_HANDLE_NONE) {
                        sp<AudioOutputDescriptor> outputDesc = mOutputs.valueFor(output);
                        if (outputDesc->isDuplicated()) {
                            return INVALID_OPERATION;
                        }
                        outputDesc->toAudioPortConfig(&newPatch.sources[1], &patch->sources[0]);
                        newPatch.sources[1].ext.mix.usecase.stream = AUDIO_STREAM_PATCH;
                        newPatch.num_sources = 2;
                    }
                }
            }
            // TODO: check from routing capabilities in config file and other conflicting patches

            audio_patch_handle_t afPatchHandle = AUDIO_PATCH_HANDLE_NONE;
            if (index >= 0) {
                afPatchHandle = patchDesc->mAfPatchHandle;
            }
            
            /**
             * 这里触发AudioFlinger的createAudioPatch
            */
            status_t status = mpClientInterface->createAudioPatch(&newPatch,&afPatchHandle,0);
            ALOGV("createAudioPatch() patch panel returned %d patchHandle %d",status, afPatchHandle);
            if (status == NO_ERROR) {
                if (index < 0) {
                    patchDesc = new AudioPatch(&newPatch, uid);
                    addAudioPatch(patchDesc->mHandle, patchDesc);
                } else {
                    patchDesc->mPatch = newPatch;
                }
                patchDesc->mAfPatchHandle = afPatchHandle;
                *handle = patchDesc->mHandle;
                nextAudioPortGeneration();
                mpClientInterface->onAudioPatchListUpdate();
            } else {
                ALOGW("createAudioPatch() patch panel could not connect device patch, error %d",
                status);
                return INVALID_OPERATION;
            }
        } else {
            return BAD_VALUE;
        }
    } else {
        return BAD_VALUE;
    }
    return NO_ERROR;
}




status_t AudioPolicyManager::releaseAudioPatch(audio_patch_handle_t handle,uid_t uid)
{
    ALOGV("releaseAudioPatch() patch %d", handle);

    ssize_t index = mAudioPatches.indexOfKey(handle);

    if (index < 0) {
        return BAD_VALUE;
    }
    sp<AudioPatch> patchDesc = mAudioPatches.valueAt(index);
    ALOGV("releaseAudioPatch() mUidCached %d patchDesc->mUid %d uid %d",
          mUidCached, patchDesc->mUid, uid);
    if (patchDesc->mUid != mUidCached && uid != patchDesc->mUid) {
        return INVALID_OPERATION;
    }

    struct audio_patch *patch = &patchDesc->mPatch;
    patchDesc->mUid = mUidCached;
    if (patch->sources[0].type == AUDIO_PORT_TYPE_MIX) {
        sp<SwAudioOutputDescriptor> outputDesc = mOutputs.getOutputFromId(patch->sources[0].id);
        if (outputDesc == NULL) {
            ALOGV("releaseAudioPatch() output not found for id %d", patch->sources[0].id);
            return BAD_VALUE;
        }

        setOutputDevice(outputDesc,
                        getNewOutputDevice(outputDesc, true /*fromCache*/),
                       true,
                       0,
                       NULL);
    } else if (patch->sources[0].type == AUDIO_PORT_TYPE_DEVICE) {
        if (patch->sinks[0].type == AUDIO_PORT_TYPE_MIX) {
            sp<AudioInputDescriptor> inputDesc = mInputs.getInputFromId(patch->sinks[0].id);
            if (inputDesc == NULL) {
                ALOGV("releaseAudioPatch() input not found for id %d", patch->sinks[0].id);
                return BAD_VALUE;
            }
            setInputDevice(inputDesc->mIoHandle,
                           getNewInputDevice(inputDesc),
                           true,
                           NULL);
        } else if (patch->sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
            status_t status = mpClientInterface->releaseAudioPatch(patchDesc->mAfPatchHandle, 0);
            ALOGV("releaseAudioPatch() patch panel returned %d patchHandle %d",
                                                              status, patchDesc->mAfPatchHandle);
            removeAudioPatch(patchDesc->mHandle);
            nextAudioPortGeneration();
            mpClientInterface->onAudioPatchListUpdate();
        } else {
            return BAD_VALUE;
        }
    } else {
        return BAD_VALUE;
    }
    return NO_ERROR;
}



