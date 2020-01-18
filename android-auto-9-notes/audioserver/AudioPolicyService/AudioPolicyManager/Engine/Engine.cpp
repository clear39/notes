
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/enginedefault/src/Engine.cpp


template <>
AudioPolicyManagerInterface *Engine::queryInterface()
{
    /**
     * mManagerInterface 在 Engine::Engine()赋值
     * */
    return &mManagerInterface; // Engine
}


/**
 * 
 * AudioPolicyManager::AudioPolicyManager 
 * --> AudioPolicyManager::initialize()
 * ---> audio_policy::EngineInstance::getInstance()
 * ----> EngineInstance::getEngine()
 * 
*/
Engine::Engine()
    : mManagerInterface(this),
      mPhoneState(AUDIO_MODE_NORMAL),
      mApmObserver(NULL)
{
    //  @   system/media/audio/include/system/audio_policy.h
    for (int i = 0; i < AUDIO_POLICY_FORCE_USE_CNT; i++) {
        mForceUse[i] = AUDIO_POLICY_FORCE_NONE;
    }
}

template <>
AudioPolicyManagerInterface *Engine::queryInterface()
{
    return &mManagerInterface;
}



void Engine::setObserver(AudioPolicyManagerObserver *observer)
{
    ALOG_ASSERT(observer != NULL, "Invalid Audio Policy Manager observer");
    /**
     * mApmObserver 为 AudioPolicyManager
    */
    mApmObserver = observer;
}


status_t Engine::initCheck()
{
    return (mApmObserver != NULL) ?  NO_ERROR : NO_INIT;
}



audio_devices_t Engine::getDeviceForStrategy(routing_strategy strategy) const
{
    DeviceVector availableOutputDevices = mApmObserver->getAvailableOutputDevices();
    DeviceVector availableInputDevices = mApmObserver->getAvailableInputDevices();

    const SwAudioOutputCollection &outputs = mApmObserver->getOutputs();

    return getDeviceForStrategyInt(strategy, availableOutputDevices, availableInputDevices, outputs, (uint32_t)AUDIO_DEVICE_NONE);
}


audio_devices_t Engine::getDeviceForStrategyInt(routing_strategy strategy,DeviceVector availableOutputDevices,DeviceVector availableInputDevices, const SwAudioOutputCollection &outputs, uint32_t outputDeviceTypesToIgnore) const
{
    uint32_t device = AUDIO_DEVICE_NONE;
    uint32_t availableOutputDevicesType = availableOutputDevices.types() & ~outputDeviceTypesToIgnore;

    switch (strategy) {

    case STRATEGY_TRANSMITTED_THROUGH_SPEAKER:
        device = availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER;
        break;

    case STRATEGY_SONIFICATION_RESPECTFUL:
        if (isInCall() || outputs.isStreamActiveLocally(AUDIO_STREAM_VOICE_CALL)) {
            device = getDeviceForStrategyInt(STRATEGY_SONIFICATION, availableOutputDevices, availableInputDevices, outputs,outputDeviceTypesToIgnore);
        } else {
            bool media_active_locally =
                    outputs.isStreamActiveLocally(AUDIO_STREAM_MUSIC, SONIFICATION_RESPECTFUL_AFTER_MUSIC_DELAY)
                    || outputs.isStreamActiveLocally(AUDIO_STREAM_ACCESSIBILITY, SONIFICATION_RESPECTFUL_AFTER_MUSIC_DELAY);
            // routing is same as media without the "remote" device
            device = getDeviceForStrategyInt(STRATEGY_MEDIA,
                    availableOutputDevices,
                    availableInputDevices, outputs,
                    AUDIO_DEVICE_OUT_REMOTE_SUBMIX | outputDeviceTypesToIgnore);
            // if no media is playing on the device, check for mandatory use of "safe" speaker
            // when media would have played on speaker, and the safe speaker path is available
            if (!media_active_locally
                    && (device & AUDIO_DEVICE_OUT_SPEAKER)
                    && (availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER_SAFE)) {
                device |= AUDIO_DEVICE_OUT_SPEAKER_SAFE;
                device &= ~AUDIO_DEVICE_OUT_SPEAKER;
            }
        }
        break;

    case STRATEGY_DTMF:
        if (!isInCall()) {
            // when off call, DTMF strategy follows the same rules as MEDIA strategy
            device = getDeviceForStrategyInt( STRATEGY_MEDIA, availableOutputDevices, availableInputDevices, outputs,outputDeviceTypesToIgnore);
            break;
        }
        // when in call, DTMF and PHONE strategies follow the same rules
        // FALL THROUGH

    case STRATEGY_PHONE:
        // Force use of only devices on primary output if:
        // - in call AND
        //   - cannot route from voice call RX OR
        //   - audio HAL version is < 3.0 and TX device is on the primary HW module
        if (getPhoneState() == AUDIO_MODE_IN_CALL) {
            audio_devices_t txDevice = getDeviceForInputSource(AUDIO_SOURCE_VOICE_COMMUNICATION);
            sp<AudioOutputDescriptor> primaryOutput = outputs.getPrimaryOutput();
            audio_devices_t availPrimaryInputDevices =
                 availableInputDevices.getDevicesFromHwModule(primaryOutput->getModuleHandle());

            // TODO: getPrimaryOutput return only devices from first module in
            // audio_policy_configuration.xml, hearing aid is not there, but it's
            // a primary device
            // FIXME: this is not the right way of solving this problem
            audio_devices_t availPrimaryOutputDevices =
                (primaryOutput->supportedDevices() | AUDIO_DEVICE_OUT_HEARING_AID) &
                availableOutputDevices.types();

            if (((availableInputDevices.types() &  AUDIO_DEVICE_IN_TELEPHONY_RX & ~AUDIO_DEVICE_BIT_IN) == 0) ||
                    (((txDevice & availPrimaryInputDevices & ~AUDIO_DEVICE_BIT_IN) != 0) &&  (primaryOutput->getAudioPort()->getModuleVersionMajor() < 3))) {
                availableOutputDevicesType = availPrimaryOutputDevices;
            }
        }
        // for phone strategy, we first consider the forced use and then the available devices by
        // order of priority
        switch (mForceUse[AUDIO_POLICY_FORCE_FOR_COMMUNICATION]) {
        case AUDIO_POLICY_FORCE_BT_SCO:
            if (!isInCall() || strategy != STRATEGY_DTMF) {
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT;
                if (device) break;
            }
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
            if (device) break;
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_SCO;
            if (device) break;
            // if SCO device is requested but no SCO device is available, fall back to default case
            // FALL THROUGH

        default:    // FORCE_NONE
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_HEARING_AID;
            if (device) break;
            // when not in a phone call, phone strategy should route STREAM_VOICE_CALL to A2DP
            if (!isInCall() &&
                    (mForceUse[AUDIO_POLICY_FORCE_FOR_MEDIA] != AUDIO_POLICY_FORCE_NO_BT_A2DP) &&
                     outputs.isA2dpSupported()) {
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_A2DP;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES;
                if (device) break;
            }
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
            if (device) break;
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_WIRED_HEADSET;
            if (device) break;
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_LINE;
            if (device) break;
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_HEADSET;
            if (device) break;
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_DEVICE;
            if (device) break;
            if (!isInCall()) {
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_ACCESSORY;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_AUX_DIGITAL;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
                if (device) break;
            }
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_EARPIECE;
            break;

        case AUDIO_POLICY_FORCE_SPEAKER:
            // when not in a phone call, phone strategy should route STREAM_VOICE_CALL to
            // A2DP speaker when forcing to speaker output
            if (!isInCall() &&
                    (mForceUse[AUDIO_POLICY_FORCE_FOR_MEDIA] != AUDIO_POLICY_FORCE_NO_BT_A2DP) &&
                     outputs.isA2dpSupported()) {
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER;
                if (device) break;
            }
            if (!isInCall()) {
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_ACCESSORY;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_DEVICE;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_AUX_DIGITAL;
                if (device) break;
                device = availableOutputDevicesType & AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
                if (device) break;
            }
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER;
            break;
        }
    break;

    case STRATEGY_SONIFICATION:

        // If incall, just select the STRATEGY_PHONE device: The rest of the behavior is handled by
        // handleIncallSonification().
        if (isInCall() || outputs.isStreamActiveLocally(AUDIO_STREAM_VOICE_CALL)) {
            device = getDeviceForStrategyInt(STRATEGY_PHONE, availableOutputDevices, availableInputDevices, outputs,outputDeviceTypesToIgnore);
            break;
        }
        // FALL THROUGH

    case STRATEGY_ENFORCED_AUDIBLE:
        // strategy STRATEGY_ENFORCED_AUDIBLE uses same routing policy as STRATEGY_SONIFICATION
        // except:
        //   - when in call where it doesn't default to STRATEGY_PHONE behavior
        //   - in countries where not enforced in which case it follows STRATEGY_MEDIA

        if ((strategy == STRATEGY_SONIFICATION) ||  (mForceUse[AUDIO_POLICY_FORCE_FOR_SYSTEM] == AUDIO_POLICY_FORCE_SYSTEM_ENFORCED)) {
            device = availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER;
        }

        // if SCO headset is connected and we are told to use it, play ringtone over
        // speaker and BT SCO
        if ((availableOutputDevicesType & AUDIO_DEVICE_OUT_ALL_SCO) != 0) {
            uint32_t device2 = AUDIO_DEVICE_NONE;
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT;
            if (device2 == AUDIO_DEVICE_NONE) {
                device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
            }
            if (device2 == AUDIO_DEVICE_NONE) {
                device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_SCO;
            }
            // Use ONLY Bluetooth SCO output when ringing in vibration mode
            if (!((mForceUse[AUDIO_POLICY_FORCE_FOR_SYSTEM] == AUDIO_POLICY_FORCE_SYSTEM_ENFORCED)  && (strategy == STRATEGY_ENFORCED_AUDIBLE))) {
                if (mForceUse[AUDIO_POLICY_FORCE_FOR_VIBRATE_RINGING]  == AUDIO_POLICY_FORCE_BT_SCO) {
                    if (device2 != AUDIO_DEVICE_NONE) {
                        device = device2;
                        break;
                    }
                }
            }
            // Use both Bluetooth SCO and phone default output when ringing in normal mode
            if (mForceUse[AUDIO_POLICY_FORCE_FOR_COMMUNICATION] == AUDIO_POLICY_FORCE_BT_SCO) {
                if ((strategy == STRATEGY_SONIFICATION) && (device & AUDIO_DEVICE_OUT_SPEAKER) && (availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER_SAFE)) {
                    device |= AUDIO_DEVICE_OUT_SPEAKER_SAFE;
                    device &= ~AUDIO_DEVICE_OUT_SPEAKER;
                }
                if (device2 != AUDIO_DEVICE_NONE) {
                    device |= device2;
                    break;
                }
            }
        }
        // The second device used for sonification is the same as the device used by media strategy
        // FALL THROUGH

    case STRATEGY_ACCESSIBILITY:
        if (strategy == STRATEGY_ACCESSIBILITY) {
            // do not route accessibility prompts to a digital output currently configured with a
            // compressed format as they would likely not be mixed and dropped.
            for (size_t i = 0; i < outputs.size(); i++) {
                sp<AudioOutputDescriptor> desc = outputs.valueAt(i);
                audio_devices_t devices = desc->device() &
                    (AUDIO_DEVICE_OUT_HDMI | AUDIO_DEVICE_OUT_SPDIF | AUDIO_DEVICE_OUT_HDMI_ARC);
                if (desc->isActive() && !audio_is_linear_pcm(desc->mFormat) &&   devices != AUDIO_DEVICE_NONE) {
                    availableOutputDevicesType = availableOutputDevices.types() & ~devices;
                }
            }
            availableOutputDevices = availableOutputDevices.getDevicesFromType(availableOutputDevicesType);
            if (outputs.isStreamActive(AUDIO_STREAM_RING) || outputs.isStreamActive(AUDIO_STREAM_ALARM)) {
                return getDeviceForStrategyInt(STRATEGY_SONIFICATION, availableOutputDevices, availableInputDevices, outputs,outputDeviceTypesToIgnore);
            }
            if (isInCall()) {
                return getDeviceForStrategyInt(
                        STRATEGY_PHONE, availableOutputDevices, availableInputDevices, outputs,
                        outputDeviceTypesToIgnore);
            }
        }
        // For other cases, STRATEGY_ACCESSIBILITY behaves like STRATEGY_MEDIA
        // FALL THROUGH

    // FIXME: STRATEGY_REROUTING follow STRATEGY_MEDIA for now
    case STRATEGY_REROUTING:
    case STRATEGY_MEDIA: {
        uint32_t device2 = AUDIO_DEVICE_NONE;
        if (strategy != STRATEGY_SONIFICATION) {
            // no sonification on remote submix (e.g. WFD)
            if (availableOutputDevices.getDevice(AUDIO_DEVICE_OUT_REMOTE_SUBMIX,String8("0")) != 0) {
                device2 = availableOutputDevices.types() & AUDIO_DEVICE_OUT_REMOTE_SUBMIX;
            }
        }
        if (isInCall() && (strategy == STRATEGY_MEDIA)) {
            device = getDeviceForStrategyInt(STRATEGY_PHONE, availableOutputDevices, availableInputDevices, outputs,outputDeviceTypesToIgnore);
            break;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_HEARING_AID;
        }
        if ((device2 == AUDIO_DEVICE_NONE) && (mForceUse[AUDIO_POLICY_FORCE_FOR_MEDIA] != AUDIO_POLICY_FORCE_NO_BT_A2DP) &&outputs.isA2dpSupported()) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_A2DP;
            if (device2 == AUDIO_DEVICE_NONE) {
                device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES;
            }
            if (device2 == AUDIO_DEVICE_NONE) {
                device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER;
            }
        }
        if ((device2 == AUDIO_DEVICE_NONE) &&  (mForceUse[AUDIO_POLICY_FORCE_FOR_MEDIA] == AUDIO_POLICY_FORCE_SPEAKER)) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_LINE;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_WIRED_HEADSET;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_HEADSET;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_ACCESSORY;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_USB_DEVICE;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET;
        }
        if ((device2 == AUDIO_DEVICE_NONE) && (strategy != STRATEGY_SONIFICATION)) {
            // no sonification on aux digital (e.g. HDMI)
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_AUX_DIGITAL;
        }
        if ((device2 == AUDIO_DEVICE_NONE) && (mForceUse[AUDIO_POLICY_FORCE_FOR_DOCK] == AUDIO_POLICY_FORCE_ANALOG_DOCK)) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET;
        }
        if (device2 == AUDIO_DEVICE_NONE) {
            device2 = availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER;
        }
        int device3 = AUDIO_DEVICE_NONE;
        if (strategy == STRATEGY_MEDIA) {
            // ARC, SPDIF and AUX_LINE can co-exist with others.
            device3 = availableOutputDevicesType & AUDIO_DEVICE_OUT_HDMI_ARC;
            device3 |= (availableOutputDevicesType & AUDIO_DEVICE_OUT_SPDIF);
            device3 |= (availableOutputDevicesType & AUDIO_DEVICE_OUT_AUX_LINE);
        }

        device2 |= device3;
        // device is DEVICE_OUT_SPEAKER if we come from case STRATEGY_SONIFICATION or
        // STRATEGY_ENFORCED_AUDIBLE, AUDIO_DEVICE_NONE otherwise
        device |= device2;

        // If hdmi system audio mode is on, remove speaker out of output list.
        if ((strategy == STRATEGY_MEDIA) && (mForceUse[AUDIO_POLICY_FORCE_FOR_HDMI_SYSTEM_AUDIO] == AUDIO_POLICY_FORCE_HDMI_SYSTEM_AUDIO_ENFORCED)) {
            device &= ~AUDIO_DEVICE_OUT_SPEAKER;
        }

        // for STRATEGY_SONIFICATION:
        // if SPEAKER was selected, and SPEAKER_SAFE is available, use SPEAKER_SAFE instead
        if ((strategy == STRATEGY_SONIFICATION) &&
                (device & AUDIO_DEVICE_OUT_SPEAKER) &&
                (availableOutputDevicesType & AUDIO_DEVICE_OUT_SPEAKER_SAFE)) {
            device |= AUDIO_DEVICE_OUT_SPEAKER_SAFE;
            device &= ~AUDIO_DEVICE_OUT_SPEAKER;
        }
        } break;

    default:
        ALOGW("getDeviceForStrategy() unknown strategy: %d", strategy);
        break;
    }

    if (device == AUDIO_DEVICE_NONE) {
        ALOGV("getDeviceForStrategy() no device found for strategy %d", strategy);
        device = mApmObserver->getDefaultOutputDevice()->type();
        ALOGE_IF(device == AUDIO_DEVICE_NONE, "getDeviceForStrategy() no default device defined");
    }
    ALOGVV("getDeviceForStrategy() strategy %d, device %x", strategy, device);
    return device;
}

