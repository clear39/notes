//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
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
            const DeviceVector &supportedDevices = outProfile->getSupportedDevices();
            const DeviceVector &devicesForType = supportedDevices.getDevicesFromType(profileType);
            String8 address = devicesForType.size() > 0 ? devicesForType.itemAt(0)->mAddress : String8("");
            audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;

            /**
             * 
             * 之类内部会 调用 AudioFlinger::openOutput 接口
             * */
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

void AudioPolicyManager::applyStreamVolumes(const sp<AudioOutputDescriptor>& outputDesc, audio_devices_t device, int delayMs, bool force)
{
    ALOGVV("applyStreamVolumes() for device %08x", device);
    /***
     * 音频流类型
     * system/media/audio/include/system/audio-base.h:36:    AUDIO_STREAM_REROUTING = 11,
     * 
     * system/media/audio/include/system/audio-base-utils.h:33:    AUDIO_STREAM_FOR_POLICY_CNT= AUDIO_STREAM_REROUTING + 1,
     * */
    for (int stream = 0; stream < AUDIO_STREAM_FOR_POLICY_CNT; stream++) {
        checkAndSetVolume((audio_stream_type_t)stream,
                          mVolumeCurves->getVolumeIndex((audio_stream_type_t)stream, device) /*=AUDIO_DEVICE_OUT_DEFAULT_FOR_VOLUME*/,
                          outputDesc,
                          device,
                          delayMs,
                          force);
    }
}

status_t AudioPolicyManager::checkAndSetVolume(audio_stream_type_t stream,int index,
                                                   const sp<AudioOutputDescriptor>& outputDesc,
                                                   audio_devices_t device,
                                                   int delayMs,bool force)
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
uint32_t AudioPolicyManager::setOutputDevice(const sp<AudioOutputDescriptor>& outputDesc,
                                             audio_devices_t device,
                                             bool force,
                                             int delayMs,
                                             audio_patch_handle_t *patchHandle,
                                             const char *address,
                                             bool requiresMuteCheck)
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
    if ((device == AUDIO_DEVICE_NONE || device == prevDevice) &&
        !force &&
        outputDesc->getPatchHandle() != 0) {
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
