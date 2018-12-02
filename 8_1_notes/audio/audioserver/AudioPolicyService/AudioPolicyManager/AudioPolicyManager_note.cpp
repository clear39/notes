//	@frameworks/av/services/audiopolicy/AudioPolicyInterface.h
class AudioPolicyInterface{}



//	@frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
class AudioPolicyManager : public AudioPolicyInterface, public AudioPolicyManagerObserver

#ifdef AUDIO_POLICY_TEST
    , public Thread
#endif //AUDIO_POLICY_TEST
{}






AudioPolicyManager::AudioPolicyManager(AudioPolicyClientInterface *clientInterface)
    :
#ifdef AUDIO_POLICY_TEST
    Thread(false),
#endif //AUDIO_POLICY_TEST
    mLimitRingtoneVolume(false), mLastVoiceVolume(-1.0f),
    mA2dpSuspended(false),
    mAudioPortGeneration(1),
    mBeaconMuteRefCount(0),
    mBeaconPlayingRefCount(0),
    mBeaconMuted(false),
    mTtsOutputAvailable(false),
    mMasterMono(false),
    mMusicEffectOutput(AUDIO_IO_HANDLE_NONE),
    mHasComputedSoundTriggerSupportsConcurrentCapture(false)
{
    mUidCached = getuid();
    mpClientInterface = clientInterface;

    // TODO: remove when legacy conf file is removed. true on devices that use DRC on the
    // DEVICE_CATEGORY_SPEAKER path to boost soft sounds, used to adjust volume curves accordingly.
    // Note: remove also speaker_drc_enabled from global configuration of XML config file.
    bool speakerDrcEnabled = false;

#ifdef USE_XML_AUDIO_POLICY_CONF			//	device/autolink/imx8/autolink_8qxp.mk:35:USE_XML_AUDIO_POLICY_CONF := 1
    mVolumeCurves = new VolumeCurvesCollection();
    AudioPolicyConfig config(mHwModules, mAvailableOutputDevices, mAvailableInputDevices,mDefaultOutputDevice, speakerDrcEnabled,static_cast<VolumeCurvesCollection *>(mVolumeCurves));
    // frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:28:#define AUDIO_POLICY_XML_CONFIG_FILE_NAME "audio_policy_configuration.xml"
    //	out/target/product/autolink_8qxp/vendor/etc/audio_policy_configuration.xml
    if (deserializeAudioPolicyXmlConfig(config) != NO_ERROR) {
#else
    mVolumeCurves = new StreamDescriptorCollection();
    AudioPolicyConfig config(mHwModules, mAvailableOutputDevices, mAvailableInputDevices,mDefaultOutputDevice, speakerDrcEnabled);
    //	frameworks/av/services/audiopolicy/common/managerdefinitions/include/audio_policy_conf.h:27:#define AUDIO_POLICY_VENDOR_CONFIG_FILE "/vendor/etc/audio_policy.conf"
    //	frameworks/av/services/audiopolicy/common/managerdefinitions/include/audio_policy_conf.h:26:#define AUDIO_POLICY_CONFIG_FILE "/system/etc/audio_policy.conf"
    if ((ConfigParsingUtils::loadConfig(AUDIO_POLICY_VENDOR_CONFIG_FILE, config) != NO_ERROR) &&  (ConfigParsingUtils::loadConfig(AUDIO_POLICY_CONFIG_FILE, config) != NO_ERROR)) {
#endif
        ALOGE("could not load audio policy configuration file, setting defaults");
        config.setDefault();
    }
    // must be done after reading the policy (since conditionned by Speaker Drc Enabling)
    mVolumeCurves->initializeVolumeCurves(speakerDrcEnabled);

    // Once policy config has been parsed, retrieve an instance of the engine and initialize it.
    audio_policy::EngineInstance *engineInstance = audio_policy::EngineInstance::getInstance();
    if (!engineInstance) {
        ALOGE("%s:  Could not get an instance of policy engine", __FUNCTION__);
        return;
    }
    // Retrieve the Policy Manager Interface
    mEngine = engineInstance->queryInterface<AudioPolicyManagerInterface>();
    if (mEngine == NULL) {
        ALOGE("%s: Failed to get Policy Engine Interface", __FUNCTION__);
        return;
    }
    mEngine->setObserver(this);
    status_t status = mEngine->initCheck();
    (void) status;
    ALOG_ASSERT(status == NO_ERROR, "Policy engine not initialized(err=%d)", status);

    // mAvailableOutputDevices and mAvailableInputDevices now contain all attached devices
    // open all output streams needed to access attached devices
    audio_devices_t outputDeviceTypes = mAvailableOutputDevices.types();
    audio_devices_t inputDeviceTypes = mAvailableInputDevices.types() & ~AUDIO_DEVICE_BIT_IN;
    for (size_t i = 0; i < mHwModules.size(); i++) {
        mHwModules[i]->mHandle = mpClientInterface->loadHwModule(mHwModules[i]->getName());
        if (mHwModules[i]->mHandle == 0) {
            ALOGW("could not open HW module %s", mHwModules[i]->getName());
            continue;
        }
        // open all output streams needed to access attached devices
        // except for direct output streams that are only opened when they are actually
        // required by an app.
        // This also validates mAvailableOutputDevices list
        for (size_t j = 0; j < mHwModules[i]->mOutputProfiles.size(); j++)
        {
            const sp<IOProfile> outProfile = mHwModules[i]->mOutputProfiles[j];

            if (!outProfile->hasSupportedDevices()) {
                ALOGW("Output profile contains no device on module %s", mHwModules[i]->getName());
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
            sp<SwAudioOutputDescriptor> outputDesc = new SwAudioOutputDescriptor(outProfile,
                                                                                 mpClientInterface);
            const DeviceVector &supportedDevices = outProfile->getSupportedDevices();
            const DeviceVector &devicesForType = supportedDevices.getDevicesFromType(profileType);
            String8 address = devicesForType.size() > 0 ? devicesForType.itemAt(0)->mAddress
                    : String8("");

            outputDesc->mDevice = profileType;
            audio_config_t config = AUDIO_CONFIG_INITIALIZER;
            config.sample_rate = outputDesc->mSamplingRate;
            config.channel_mask = outputDesc->mChannelMask;
            config.format = outputDesc->mFormat;
            audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
            status_t status = mpClientInterface->openOutput(outProfile->getModuleHandle(),
                                                            &output,
                                                            &config,
                                                            &outputDesc->mDevice,
                                                            address,
                                                            &outputDesc->mLatency,
                                                            outputDesc->mFlags);

            if (status != NO_ERROR) {
                ALOGW("Cannot open output stream for device %08x on hw module %s",
                      outputDesc->mDevice,
                      mHwModules[i]->getName());
            } else {
                outputDesc->mSamplingRate = config.sample_rate;
                outputDesc->mChannelMask = config.channel_mask;
                outputDesc->mFormat = config.format;

                for (size_t k = 0; k  < supportedDevices.size(); k++) {
                    ssize_t index = mAvailableOutputDevices.indexOf(supportedDevices[k]);
                    // give a valid ID to an attached device once confirmed it is reachable
                    if (index >= 0 && !mAvailableOutputDevices[index]->isAttached()) {
                        mAvailableOutputDevices[index]->attach(mHwModules[i]);
                    }
                }
                if (mPrimaryOutput == 0 &&
                        outProfile->getFlags() & AUDIO_OUTPUT_FLAG_PRIMARY) {
                    mPrimaryOutput = outputDesc;
                }
                addOutput(output, outputDesc);
                setOutputDevice(outputDesc,
                                outputDesc->mDevice,
                                true,
                                0,
                                NULL,
                                address.string());
            }
        }
        // open input streams needed to access attached devices to validate
        // mAvailableInputDevices list
        for (size_t j = 0; j < mHwModules[i]->mInputProfiles.size(); j++)
        {
            const sp<IOProfile> inProfile = mHwModules[i]->mInputProfiles[j];

            if (!inProfile->hasSupportedDevices()) {
                ALOGW("Input profile contains no device on module %s", mHwModules[i]->getName());
                continue;
            }
            // chose first device present in profile's SupportedDevices also part of
            // inputDeviceTypes
            audio_devices_t profileType = inProfile->getSupportedDeviceForType(inputDeviceTypes);

            if ((profileType & inputDeviceTypes) == 0) {
                continue;
            }
            sp<AudioInputDescriptor> inputDesc =
                    new AudioInputDescriptor(inProfile);

            inputDesc->mDevice = profileType;

            // find the address
            DeviceVector inputDevices = mAvailableInputDevices.getDevicesFromType(profileType);
            //   the inputs vector must be of size 1, but we don't want to crash here
            String8 address = inputDevices.size() > 0 ? inputDevices.itemAt(0)->mAddress
                    : String8("");
            ALOGV("  for input device 0x%x using address %s", profileType, address.string());
            ALOGE_IF(inputDevices.size() == 0, "Input device list is empty!");

            audio_config_t config = AUDIO_CONFIG_INITIALIZER;
            config.sample_rate = inputDesc->mSamplingRate;
            config.channel_mask = inputDesc->mChannelMask;
            config.format = inputDesc->mFormat;
            audio_io_handle_t input = AUDIO_IO_HANDLE_NONE;
            status_t status = mpClientInterface->openInput(inProfile->getModuleHandle(),
                                                           &input,
                                                           &config,
                                                           &inputDesc->mDevice,
                                                           address,
                                                           AUDIO_SOURCE_MIC,
                                                           AUDIO_INPUT_FLAG_NONE);

            if (status == NO_ERROR) {
                const DeviceVector &supportedDevices = inProfile->getSupportedDevices();
                for (size_t k = 0; k  < supportedDevices.size(); k++) {
                    ssize_t index =  mAvailableInputDevices.indexOf(supportedDevices[k]);
                    // give a valid ID to an attached device once confirmed it is reachable
                    if (index >= 0) {
                        sp<DeviceDescriptor> devDesc = mAvailableInputDevices[index];
                        if (!devDesc->isAttached()) {
                            devDesc->attach(mHwModules[i]);
                            devDesc->importAudioPort(inProfile, true);
                        }
                    }
                }
                mpClientInterface->closeInput(input);
            } else {
                ALOGW("Cannot open input stream for device %08x on hw module %s",
                      inputDesc->mDevice,
                      mHwModules[i]->getName());
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
        mEngine->setDeviceConnectionState(mAvailableOutputDevices[i],
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE);
        i++;
    }
    for (size_t i = 0; i  < mAvailableInputDevices.size();) {
        if (!mAvailableInputDevices[i]->isAttached()) {
            ALOGW("Input device %08x unreachable", mAvailableInputDevices[i]->type());
            mAvailableInputDevices.remove(mAvailableInputDevices[i]);
            continue;
        }
        // The device is now validated and can be appended to the available devices of the engine
        mEngine->setDeviceConnectionState(mAvailableInputDevices[i],
                                          AUDIO_POLICY_DEVICE_STATE_AVAILABLE);
        i++;
    }
    // make sure default device is reachable
    if (mDefaultOutputDevice == 0 || mAvailableOutputDevices.indexOf(mDefaultOutputDevice) < 0) {
        ALOGE("Default device %08x is unreachable", mDefaultOutputDevice->type());
    }

    ALOGE_IF((mPrimaryOutput == 0), "Failed to open primary output");

    updateDevicesAndOutputs();

#ifdef AUDIO_POLICY_TEST
    if (mPrimaryOutput != 0) {
        AudioParameter outputCmd = AudioParameter();
        outputCmd.addInt(String8("set_id"), 0);
        mpClientInterface->setParameters(mPrimaryOutput->mIoHandle, outputCmd.toString());

        mTestDevice = AUDIO_DEVICE_OUT_SPEAKER;
        mTestSamplingRate = 44100;
        mTestFormat = AUDIO_FORMAT_PCM_16_BIT;
        mTestChannels =  AUDIO_CHANNEL_OUT_STEREO;
        mTestLatencyMs = 0;
        mCurOutput = 0;
        mDirectOutput = false;
        for (int i = 0; i < NUM_TEST_OUTPUTS; i++) {
            mTestOutputs[i] = 0;
        }

        const size_t SIZE = 256;
        char buffer[SIZE];
        snprintf(buffer, SIZE, "AudioPolicyManagerTest");
        run(buffer, ANDROID_PRIORITY_AUDIO);
    }
#endif //AUDIO_POLICY_TEST
}





//	这里系统目前只有/vendor/etc/audio_policy_configuration.xml
static status_t deserializeAudioPolicyXmlConfig(AudioPolicyConfig &config) {
    char audioPolicyXmlConfigFile[AUDIO_POLICY_XML_CONFIG_FILE_PATH_MAX_LENGTH];
    status_t ret;

    for (int i = 0; i < kConfigLocationListSize; i++) {
        PolicySerializer serializer;
        snprintf(audioPolicyXmlConfigFile,
                 sizeof(audioPolicyXmlConfigFile),
                 "%s/%s",
                 kConfigLocationList[i],
                 AUDIO_POLICY_XML_CONFIG_FILE_NAME);
        ret = serializer.deserialize(audioPolicyXmlConfigFile, config);
        if (ret == NO_ERROR) {
            break;
        }
    }
    return ret;
}