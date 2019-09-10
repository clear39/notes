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

status_t AudioPolicyManager::initialize() {
    //  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/src/StreamDescriptor.cpp
    /*
    
    */
    mVolumeCurves->initializeVolumeCurves(getConfig().isSpeakerDrcEnabled());

    //  @ frameworks/av/services/audiopolicy/engineconfigurable/src/EngineInstance.cpp
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

    // mAvailableOutputDevices and mAvailableInputDevices now contain all attached devices
    // open all output streams needed to access attached devices
    audio_devices_t outputDeviceTypes = mAvailableOutputDevices.types();
    audio_devices_t inputDeviceTypes = mAvailableInputDevices.types() & ~AUDIO_DEVICE_BIT_IN;

    for (const auto& hwModule : mHwModulesAll) {

        // 返回 audio_module_handle_t 
        //  @ system/media/audio/include/system/audio.h:321:typedef int audio_module_handle_t;
        //  @ system/media/audio/include/system/audio-base.h:15:    AUDIO_MODULE_HANDLE_NONE = 0
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
        for (const auto& outProfile : hwModule->getOutputProfiles()) {
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

            audio_devices_t profileType = outProfile->getSupportedDevicesType();
            if ((profileType & mDefaultOutputDevice->type()) != AUDIO_DEVICE_NONE) {
                profileType = mDefaultOutputDevice->type();
            } else {
                // chose first device present in profile's SupportedDevices also part of
                // outputDeviceTypes
                profileType = outProfile->getSupportedDeviceForType(outputDeviceTypes);
            }
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
                addOutput(output, outputDesc);
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
                mAvailableInputDevices[i]->mAddress = String8(AUDIO_BOTTOM_MICROPHONE_ADDRESS);
            } else if (mAvailableInputDevices[i]->type() == AUDIO_DEVICE_IN_BACK_MIC) {
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


void AudioPolicyManager::updateDevicesAndOutputs()
{
    //  frameworks/av/services/audiopolicy/common/include/RoutingStrategy.h:35:    NUM_STRATEGIES
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        mDeviceForStrategy[i] = getDeviceForStrategy((routing_strategy)i, false /*fromCache*/);
    }
    mPreviousOutputs = mOutputs;
}

audio_devices_t AudioPolicyManager::getDeviceForStrategy(routing_strategy strategy,bool fromCache)
{
    // Check if an explicit routing request exists for a stream type corresponding to the
    // specified strategy and use it in priority over default routing rules.
    for (int stream = 0; stream < AUDIO_STREAM_FOR_POLICY_CNT; stream++) {
        if (getStrategy((audio_stream_type_t)stream) == strategy) {
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
