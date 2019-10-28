//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
/**
 * IAudioFlinger::createRecord
 * --> AudioFlinger::createRecord
 * ---> AudioSystem::getInputForAttr
*/
status_t AudioPolicyManager::getInputForAttr(const audio_attributes_t *attr,
                                             audio_io_handle_t *input,  // out
                                             audio_session_t session,
                                             uid_t uid,
                                             const audio_config_base_t *config,
                                             audio_input_flags_t flags,
                                             audio_port_handle_t *selectedDeviceId,  // out 
                                             input_type_t *inputType,       // out
                                             audio_port_handle_t *portId       // out
                                             )
{
    ALOGV("getInputForAttr() source %d, sampling rate %d, format %#x, channel mask %#x,"
            "session %d, flags %#x",
          attr->source, config->sample_rate, config->format, config->channel_mask, session, flags);

    status_t status = NO_ERROR;
    // handle legacy remote submix case where the address was not always specified
    String8 address = String8("");
    audio_source_t halInputSource;
    audio_source_t inputSource = attr->source;  // 8
    AudioMix *policyMix = NULL;
    DeviceVector inputDevices;

    /**
     * 这里如果默认为 AUDIO_SOURCE_DEFAULT ，强制设置为 AUDIO_SOURCE_MIC
    */
    if (inputSource == AUDIO_SOURCE_DEFAULT) {
        inputSource = AUDIO_SOURCE_MIC;
    }

    // Explicit routing?
    sp<DeviceDescriptor> deviceDesc;
    /**
     * *selectedDeviceId 等于 AUDIO_PORT_HANDLE_NONE
    */
    if (*selectedDeviceId != AUDIO_PORT_HANDLE_NONE) {
        deviceDesc = mAvailableInputDevices.getDeviceFromId(*selectedDeviceId);
    }
    mInputRoutes.addRoute(session, SessionRoute::STREAM_TYPE_NA, inputSource, deviceDesc, uid);

    // special case for mmap capture: if an input IO handle is specified, we reuse this input if
    // possible
    if ((flags & AUDIO_INPUT_FLAG_MMAP_NOIRQ) == AUDIO_INPUT_FLAG_MMAP_NOIRQ && *input != AUDIO_IO_HANDLE_NONE) {
        。。。。。。
    }

    *input = AUDIO_IO_HANDLE_NONE;
    *inputType = API_INPUT_INVALID;

    halInputSource = inputSource;  //   system/media/audio/include/system/audio-base.h:51:    AUDIO_SOURCE_REMOTE_SUBMIX = 8,

    /**
     * portId 在这里随机生成
     * 
     * **/
    // TODO: check for existing client for this port ID
    if (*portId == AUDIO_PORT_HANDLE_NONE) {
        *portId = AudioPort::getNextUniqueId();
    }

    audio_devices_t device;

    ALOGV("getInputForAttr() tags(%s)",attr->tags); // 空的

    if (inputSource == AUDIO_SOURCE_REMOTE_SUBMIX && strncmp(attr->tags, "addr=", strlen("addr=")) == 0) {
        /**
         * 
        */
        status = mPolicyMixes.getInputMixForAttr(*attr, &policyMix);
        if (status != NO_ERROR) {
            goto error;
        }
        *inputType = API_INPUT_MIX_EXT_POLICY_REROUTE;
        device = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
        address = String8(attr->tags + strlen("addr="));
    } else {
        /**
         * 
         * */
        device = getDeviceAndMixForInputSource(inputSource, &policyMix);

        // 10-10 13:44:10.458  1771  1771 V APM_AudioPolicyManager: getInputForAttr() device -2147483392
        ALOGV("getInputForAttr() device %#x",*device);
        if (device == AUDIO_DEVICE_NONE) {
            ALOGW("getInputForAttr() could not find device for source %d", inputSource);
            status = BAD_VALUE;
            goto error;
        }
        if (policyMix != NULL) {
            address = policyMix->mDeviceAddress;
            if (policyMix->mMixType == MIX_TYPE_RECORDERS) {
                // there is an external policy, but this input is attached to a mix of recorders,
                // meaning it receives audio injected into the framework, so the recorder doesn't
                // know about it and is therefore considered "legacy"
                *inputType = API_INPUT_LEGACY;
            } else {
                // recording a mix of players defined by an external policy, we're rerouting for
                // an external policy
                *inputType = API_INPUT_MIX_EXT_POLICY_REROUTE;
            }
        } else if (audio_is_remote_submix_device(device)) {
            address = String8("0");
            *inputType = API_INPUT_MIX_CAPTURE;
        } else if (device == AUDIO_DEVICE_IN_TELEPHONY_RX) {
            *inputType = API_INPUT_TELEPHONY_RX;
        } else {
            *inputType = API_INPUT_LEGACY;
        }

    }
    /**
     * 
     * 这里 *inputType = API_INPUT_MIX_CAPTURE;
    */
    *input = getInputForDevice(device, address, session, uid, inputSource,config, flags,policyMix);
    if (*input == AUDIO_IO_HANDLE_NONE) {
        status = INVALID_OPERATION;
        goto error;
    }

    inputDevices = mAvailableInputDevices.getDevicesFromType(device);
    *selectedDeviceId = inputDevices.size() > 0 ? inputDevices.itemAt(0)->getId() : AUDIO_PORT_HANDLE_NONE;

    ALOGV("getInputForAttr() returns input %d type %d selectedDeviceId %d",*input, *inputType, *selectedDeviceId);

    return NO_ERROR;

error:
    mInputRoutes.removeRoute(session);
    return status;
}


//  frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
audio_devices_t AudioPolicyManager::getDeviceAndMixForInputSource(audio_source_t inputSource,AudioMix **policyMix)
{
    /**
     * system/media/audio/include/system/audio-base.h:291:    AUDIO_DEVICE_BIT_IN                        = 0x80000000u,
     * 
     * AUDIO_DEVICE_IN_BUILTIN_MIC                = 0x80000004u, // BIT_IN | 0x4
     * 
     * AUDIO_DEVICE_IN_REMOTE_SUBMIX              = 0x80000100u, // BIT_IN | 0x100
     * 
    */
    audio_devices_t availableDeviceTypes = mAvailableInputDevices.types() & ~AUDIO_DEVICE_BIT_IN; // 0x104
    /***
     *   @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyMix.cpp
     * 
     * 
     * */
    audio_devices_t selectedDeviceFromMix = mPolicyMixes.getDeviceAndMixForInputSource(inputSource, availableDeviceTypes, policyMix);

    //  10-22 09:05:43.788  2962 10902 V APM_AudioPolicyManager: getInputForAttr() availableDeviceTypes(0x104) selectedDeviceFromMix(0)
    ALOGV("getInputForAttr() availableDeviceTypes(%#x) selectedDeviceFromMix(%#x)",availableDeviceTypes,selectedDeviceFromMix);

    if (selectedDeviceFromMix != AUDIO_DEVICE_NONE) {
        return selectedDeviceFromMix;
    }
    
    return getDeviceForInputSource(inputSource);
}



audio_devices_t AudioPolicyManager::getDeviceForInputSource(audio_source_t inputSource)
{
    // Routing
    // Scan the whole RouteMap to see if we have an explicit route:
    // if the input source in the RouteMap is the same as the argument above,
    // and activity count is non-zero and the device in the route descriptor is available
    // then select this device.
    for (size_t routeIndex = 0; routeIndex < mInputRoutes.size(); routeIndex++) {
         sp<SessionRoute> route = mInputRoutes.valueAt(routeIndex);
         if ((inputSource == route->mSource) && route->isActiveOrChanged() && (mAvailableInputDevices.indexOf(route->mDeviceDescriptor) >= 0)) {
             return route->mDeviceDescriptor->type();
         }
     }

     return mEngine->getDeviceForInputSource(inputSource);
}















//////////////////////////////////////////////////////////////////////////////////////
/**
 * 
 * IAudioRecord::start 
 * ---> RecordHandle::start 
 * ----> RecordTrack::start 
 * -----> RecordThread::start
 * ------> AudioSystem::startInput
 * 
*/
status_t AudioPolicyManager::startInput(audio_io_handle_t input,
                                        audio_session_t session,
                                        bool silenced,
                                        concurrency_type__mask_t *concurrency)
{

    ALOGV("AudioPolicyManager::startInput(input:%d, session:%d, silenced:%d, concurrency:%d)",
            input, session, silenced, *concurrency);

    *concurrency = API_INPUT_CONCURRENCY_NONE;

    ssize_t index = mInputs.indexOfKey(input);
    if (index < 0) {
        ALOGW("startInput() unknown input %d", input);
        return BAD_VALUE;
    }
    sp<AudioInputDescriptor> inputDesc = mInputs.valueAt(index);

    sp<AudioSession> audioSession = inputDesc->getAudioSession(session);
    if (audioSession == 0) {
        ALOGW("startInput() unknown session %d on input %d", session, input);
        return BAD_VALUE;
    }

// FIXME: disable concurrent capture until UI is ready
#if 0
    ......
#else
    if (!is_virtual_input_device(inputDesc->mDevice)) {
        if (mCallTxPatch != 0 &&
            inputDesc->getModuleHandle() == mCallTxPatch->mPatch.sources[0].ext.device.hw_module) {
            ALOGW("startInput(%d) failed: call in progress", input);
            *concurrency |= API_INPUT_CONCURRENCY_CALL;
            return INVALID_OPERATION;
        }

        Vector<sp<AudioInputDescriptor>> activeInputs = mInputs.getActiveInputs();

        // If a UID is idle and records silence and another not silenced recording starts
        // from another UID (idle or active) we stop the current idle UID recording in
        // favor of the new one - "There can be only one" TM
        if (!silenced) {
            for (const auto& activeDesc : activeInputs) {
                if ((audioSession->flags() & AUDIO_INPUT_FLAG_MMAP_NOIRQ) != 0 &&
                        activeDesc->getId() == inputDesc->getId()) {
                     continue;
                }

                AudioSessionCollection activeSessions = activeDesc->getAudioSessions(true /*activeOnly*/);
                sp<AudioSession> activeSession = activeSessions.valueAt(0);
                if (activeSession->isSilenced()) {
                    audio_io_handle_t activeInput = activeDesc->mIoHandle;
                    audio_session_t activeSessionId = activeSession->session();
                    stopInput(activeInput, activeSessionId);
                    releaseInput(activeInput, activeSessionId);
                    ALOGV("startInput(%d) stopping silenced input %d", input, activeInput);
                    activeInputs = mInputs.getActiveInputs();
                }
            }
        }

        for (const auto& activeDesc : activeInputs) {
            if ((audioSession->flags() & AUDIO_INPUT_FLAG_MMAP_NOIRQ) != 0 && activeDesc->getId() == inputDesc->getId()) {
                continue;
            }

            audio_source_t activeSource = activeDesc->inputSource(true);
            if (audioSession->inputSource() == AUDIO_SOURCE_HOTWORD) {
                if (activeSource == AUDIO_SOURCE_HOTWORD) {
                    if (activeDesc->hasPreemptedSession(session)) {
                        ALOGW("startInput(%d) failed for HOTWORD: " "other input %d already started for HOTWORD", input, activeDesc->mIoHandle);
                        *concurrency |= API_INPUT_CONCURRENCY_HOTWORD;
                        return INVALID_OPERATION;
                    }
                } else {
                    ALOGV("startInput(%d) failed for HOTWORD: other input %d already started", input, activeDesc->mIoHandle);
                    *concurrency |= API_INPUT_CONCURRENCY_CAPTURE;
                    return INVALID_OPERATION;
                }
            } else {
                if (activeSource != AUDIO_SOURCE_HOTWORD) {
                    ALOGW("startInput(%d) failed: other input %d already started", input, activeDesc->mIoHandle);
                    *concurrency |= API_INPUT_CONCURRENCY_CAPTURE;
                    return INVALID_OPERATION;
                }
            }
        }

        // We only need to check if the sound trigger session supports concurrent capture if the
        // input is also a sound trigger input. Otherwise, we should preempt any hotword stream
        // that's running.
        const bool allowConcurrentWithSoundTrigger = inputDesc->isSoundTrigger() ? soundTriggerSupportsConcurrentCapture() : false;

        // if capture is allowed, preempt currently active HOTWORD captures
        for (const auto& activeDesc : activeInputs) {
            if (allowConcurrentWithSoundTrigger && activeDesc->isSoundTrigger()) {
                continue;
            }

            audio_source_t activeSource = activeDesc->inputSource(true);
            if (activeSource == AUDIO_SOURCE_HOTWORD) {
                AudioSessionCollection activeSessions =  activeDesc->getAudioSessions(true /*activeOnly*/);
                audio_session_t activeSession = activeSessions.keyAt(0);
                audio_io_handle_t activeHandle = activeDesc->mIoHandle;
                SortedVector<audio_session_t> sessions = activeDesc->getPreemptedSessions();
                *concurrency |= API_INPUT_CONCURRENCY_PREEMPT;
                sessions.add(activeSession);
                inputDesc->setPreemptedSessions(sessions);
                stopInput(activeHandle, activeSession);
                releaseInput(activeHandle, activeSession);
                ALOGV("startInput(%d) for HOTWORD preempting HOTWORD input %d", input, activeDesc->mIoHandle);
            }
        }
    }
#endif

    // Make sure we start with the correct silence state
    audioSession->setSilenced(silenced);

    // increment activity count before calling getNewInputDevice() below as only active sessions
    // are considered for device selection
    audioSession->changeActiveCount(1);

    // Routing?
    mInputRoutes.incRouteActivity(session);

    if (audioSession->activeCount() == 1 || mInputRoutes.getAndClearRouteChanged(session)) {
        // indicate active capture to sound trigger service if starting capture from a mic on
        // primary HW module
        audio_devices_t device = getNewInputDevice(inputDesc);
        setInputDevice(input, device, true /* force */);

        status_t status = inputDesc->start();
        if (status != NO_ERROR) {
            mInputRoutes.decRouteActivity(session);
            audioSession->changeActiveCount(-1);
            return status;
        }

        if (inputDesc->getAudioSessionCount(true/*activeOnly*/) == 1) {
            // if input maps to a dynamic policy with an activity listener, notify of state change
            if ((inputDesc->mPolicyMix != NULL)
                    && ((inputDesc->mPolicyMix->mCbFlags & AudioMix::kCbFlagNotifyActivity) != 0)) {
                mpClientInterface->onDynamicPolicyMixStateUpdate(inputDesc->mPolicyMix->mDeviceAddress,
                        MIX_STATE_MIXING);
            }

            audio_devices_t primaryInputDevices = availablePrimaryInputDevices();
            if (((device & primaryInputDevices & ~AUDIO_DEVICE_BIT_IN) != 0) && mInputs.activeInputsCountOnDevices(primaryInputDevices) == 1) {
                SoundTrigger::setCaptureState(true);
            }

            // automatically enable the remote submix output when input is started if not
            // used by a policy mix of type MIX_TYPE_RECORDERS
            // For remote submix (a virtual device), we open only one input per capture request.
            if (audio_is_remote_submix_device(inputDesc->mDevice)) {
                String8 address = String8("");
                if (inputDesc->mPolicyMix == NULL) {
                    address = String8("0");
                } else if (inputDesc->mPolicyMix->mMixType == MIX_TYPE_PLAYERS) {
                    address = inputDesc->mPolicyMix->mDeviceAddress;
                }
                if (address != "") {
                    /**
                     * 
                     * 
                    */
                    setDeviceConnectionStateInt(AUDIO_DEVICE_OUT_REMOTE_SUBMIX,AUDIO_POLICY_DEVICE_STATE_AVAILABLE,address, "remote-submix");
                }
            }
        }
    }

    ALOGV("AudioPolicyManager::startInput() input source = %d", audioSession->inputSource());

    return NO_ERROR;
}



status_t AudioPolicyManager::setDeviceConnectionStateInt(audio_devices_t device,
                                                         audio_policy_dev_state_t state,
                                                         const char *device_address,
                                                         const char *device_name)
{
    //  10-11 17:03:20.471  1769  1801 V APM_AudioPolicyManager: setDeviceConnectionStateInt() device: 0x8000, state 1, address 0 name remote-submix
    ALOGV("setDeviceConnectionStateInt() device: 0x%X, state %d, address %s name %s", device, state, device_address, device_name);

    // connect/disconnect only 1 device at a time
    /**
     * @    system/media/audio/include/system/audio.h
     * 
    */
    if (!audio_is_output_device(device) && !audio_is_input_device(device)) return BAD_VALUE;

    
    sp<DeviceDescriptor> devDesc = mHwModules.getDeviceDescriptor(device, device_address, device_name);

    // handle output devices
    /**
        554 static inline bool audio_is_output_device(audio_devices_t device)
        555 {   
        556     if (((device & AUDIO_DEVICE_BIT_IN) == 0) &&   //   system/media/audio/include/system/audio-base.h:291:    AUDIO_DEVICE_BIT_IN                        = 0x80000000u,
        557     ┊   ┊   (popcount(device) == 1) && ((device & ~AUDIO_DEVICE_OUT_ALL) == 0))
        558     ┊   return true;
        559     else
        560     ┊   return false;
        561 }
    */
    if (audio_is_output_device(device)) {
        SortedVector <audio_io_handle_t> outputs;

        ssize_t index = mAvailableOutputDevices.indexOf(devDesc);

        // save a copy of the opened output descriptors before any output is opened or closed
        // by checkOutputsForDevice(). This will be needed by checkOutputForAllStrategies()
        mPreviousOutputs = mOutputs;
        switch (state)
        {
        // handle output device connection
        case AUDIO_POLICY_DEVICE_STATE_AVAILABLE: { // 1
            if (index >= 0) {
                ALOGW("setDeviceConnectionState() device already connected: %x", device);
                return INVALID_OPERATION;
            }
            //  10-11 17:03:20.471  1769  1801 V APM_AudioPolicyManager: setDeviceConnectionState() connecting device 8000
            ALOGV("setDeviceConnectionState() connecting device %x", device);

            // register new device as available
            index = mAvailableOutputDevices.add(devDesc);
            if (index >= 0) {
                sp<HwModule> module = mHwModules.getModuleForDevice(device);
                if (module == 0) {
                    ALOGD("setDeviceConnectionState() could not find HW module for device %08x",device);
                    mAvailableOutputDevices.remove(devDesc);
                    return INVALID_OPERATION;
                }
                mAvailableOutputDevices[index]->attach(module);
            } else {
                return NO_MEMORY;
            }

            // Before checking outputs, broadcast connect event to allow HAL to retrieve dynamic
            // parameters on newly connected devices (instead of opening the outputs...)
            broadcastDeviceConnectionState(device, state, devDesc->mAddress);

            /**
             * 
            */
            if (checkOutputsForDevice(devDesc, state, outputs, devDesc->mAddress) != NO_ERROR) {
                mAvailableOutputDevices.remove(devDesc);

                broadcastDeviceConnectionState(device, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,devDesc->mAddress);
                return INVALID_OPERATION;
            }
            // Propagate device availability to Engine
            mEngine->setDeviceConnectionState(devDesc, state);

            // outputs should never be empty here
            ALOG_ASSERT(outputs.size() != 0, "setDeviceConnectionState():" "checkOutputsForDevice() returned no outputs but status OK");
            
            //  10-11 17:03:20.498  1769  1801 V APM_AudioPolicyManager: setDeviceConnectionState() checkOutputsForDevice() returned 1 outputs
            ALOGV("setDeviceConnectionState() checkOutputsForDevice() returned %zu outputs", outputs.size());

            } break;
        // handle output device disconnection
        case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE: {//0 
            if (index < 0) {
                ALOGW("setDeviceConnectionState() device not connected: %x", device);
                return INVALID_OPERATION;
            }

            ALOGV("setDeviceConnectionState() disconnecting output device %x", device);

            // Send Disconnect to HALs
            broadcastDeviceConnectionState(device, state, devDesc->mAddress);

            // remove device from available output devices
            mAvailableOutputDevices.remove(devDesc);

            checkOutputsForDevice(devDesc, state, outputs, devDesc->mAddress);

            // Propagate device availability to Engine
            mEngine->setDeviceConnectionState(devDesc, state);
            } break;

        default:
            ALOGE("setDeviceConnectionState() invalid state: %x", state);
            return BAD_VALUE;
        }

        // checkA2dpSuspend must run before checkOutputForAllStrategies so that A2DP
        // output is suspended before any tracks are moved to it
        checkA2dpSuspend();
        checkOutputForAllStrategies();
        // outputs must be closed after checkOutputForAllStrategies() is executed
        if (!outputs.isEmpty()) {
            for (audio_io_handle_t output : outputs) {
                sp<SwAudioOutputDescriptor> desc = mOutputs.valueFor(output);
                // close unused outputs after device disconnection or direct outputs that have been
                // opened by checkOutputsForDevice() to query dynamic parameters
                if ((state == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE) ||
                        (((desc->mFlags & AUDIO_OUTPUT_FLAG_DIRECT) != 0) &&
                         (desc->mDirectOpenCount == 0))) {
                    closeOutput(output);
                }
            }
            // check again after closing A2DP output to reset mA2dpSuspended if needed
            checkA2dpSuspend();
        }

        updateDevicesAndOutputs();
        if (mEngine->getPhoneState() == AUDIO_MODE_IN_CALL && hasPrimaryOutput()) {
            audio_devices_t newDevice = getNewOutputDevice(mPrimaryOutput, false /*fromCache*/);
            updateCallRouting(newDevice);
        }
        for (size_t i = 0; i < mOutputs.size(); i++) {
            sp<SwAudioOutputDescriptor> desc = mOutputs.valueAt(i);
            if ((mEngine->getPhoneState() != AUDIO_MODE_IN_CALL) || (desc != mPrimaryOutput)) {
                audio_devices_t newDevice = getNewOutputDevice(desc, true /*fromCache*/);
                // do not force device change on duplicated output because if device is 0, it will
                // also force a device 0 for the two outputs it is duplicated to which may override
                // a valid device selection on those outputs.
                bool force = !desc->isDuplicated()
                        && (!device_distinguishes_on_address(device)
                                // always force when disconnecting (a non-duplicated device)
                                || (state == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE));
                setOutputDevice(desc, newDevice, force, 0);
            }
        }

        if (state == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE) {
            cleanUpForDevice(devDesc);
        }

        mpClientInterface->onAudioPortListUpdate();
        return NO_ERROR;
    }  // end if is output device

    // handle input devices
    if (audio_is_input_device(device)) {
        SortedVector <audio_io_handle_t> inputs;

        ssize_t index = mAvailableInputDevices.indexOf(devDesc);
        switch (state)
        {
        // handle input device connection
        case AUDIO_POLICY_DEVICE_STATE_AVAILABLE: {
            if (index >= 0) {
                ALOGW("setDeviceConnectionState() device already connected: %d", device);
                return INVALID_OPERATION;
            }
            sp<HwModule> module = mHwModules.getModuleForDevice(device);
            if (module == NULL) {
                ALOGW("setDeviceConnectionState(): could not find HW module for device %08x",
                      device);
                return INVALID_OPERATION;
            }

            // Before checking intputs, broadcast connect event to allow HAL to retrieve dynamic
            // parameters on newly connected devices (instead of opening the inputs...)
            broadcastDeviceConnectionState(device, state, devDesc->mAddress);

            if (checkInputsForDevice(devDesc, state, inputs, devDesc->mAddress) != NO_ERROR) {
                broadcastDeviceConnectionState(device, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
                                               devDesc->mAddress);
                return INVALID_OPERATION;
            }

            index = mAvailableInputDevices.add(devDesc);
            if (index >= 0) {
                mAvailableInputDevices[index]->attach(module);
            } else {
                return NO_MEMORY;
            }

            // Propagate device availability to Engine
            mEngine->setDeviceConnectionState(devDesc, state);
        } break;

        // handle input device disconnection
        case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE: {
            if (index < 0) {
                ALOGW("setDeviceConnectionState() device not connected: %d", device);
                return INVALID_OPERATION;
            }

            ALOGV("setDeviceConnectionState() disconnecting input device %x", device);

            // Set Disconnect to HALs
            broadcastDeviceConnectionState(device, state, devDesc->mAddress);

            checkInputsForDevice(devDesc, state, inputs, devDesc->mAddress);
            mAvailableInputDevices.remove(devDesc);

            // Propagate device availability to Engine
            mEngine->setDeviceConnectionState(devDesc, state);
        } break;

        default:
            ALOGE("setDeviceConnectionState() invalid state: %x", state);
            return BAD_VALUE;
        }

        closeAllInputs();
        // As the input device list can impact the output device selection, update
        // getDeviceForStrategy() cache
        updateDevicesAndOutputs();

        if (mEngine->getPhoneState() == AUDIO_MODE_IN_CALL && hasPrimaryOutput()) {
            audio_devices_t newDevice = getNewOutputDevice(mPrimaryOutput, false /*fromCache*/);
            updateCallRouting(newDevice);
        }

        if (state == AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE) {
            cleanUpForDevice(devDesc);
        }

        mpClientInterface->onAudioPortListUpdate();
        return NO_ERROR;
    } // end if is input device

    ALOGW("setDeviceConnectionState() invalid device: %x", device);
    return BAD_VALUE;
}

status_t AudioPolicyManager::checkOutputsForDevice(const sp<DeviceDescriptor>& devDesc,
                                                   audio_policy_dev_state_t state,
                                                   SortedVector<audio_io_handle_t>& outputs,
                                                   const String8& address)
{
    audio_devices_t device = devDesc->type();
    sp<SwAudioOutputDescriptor> desc;

    if (audio_device_is_digital(device)) {
        // erase all current sample rates, formats and channel masks
        devDesc->clearAudioProfiles();
    }

    if (state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
        // first list already open outputs that can be routed to this device
        for (size_t i = 0; i < mOutputs.size(); i++) {
            desc = mOutputs.valueAt(i);
            if (!desc->isDuplicated() && (desc->supportedDevices() & device)) {
                if (!device_distinguishes_on_address(device)) {

                    ALOGV("checkOutputsForDevice(): adding opened output %d", mOutputs.keyAt(i));
                    outputs.add(mOutputs.keyAt(i));
                } else {
                    ALOGV("  checking address match due to device 0x%x", device);
                    findIoHandlesByAddress(desc, device, address, outputs);
                }
            }
        }
        // then look for output profiles that can be routed to this device
        SortedVector< sp<IOProfile> > profiles;
        for (const auto& hwModule : mHwModules) {
            for (size_t j = 0; j < hwModule->getOutputProfiles().size(); j++) {
                sp<IOProfile> profile = hwModule->getOutputProfiles()[j];
                if (profile->supportDevice(device)) {
                    if (!device_distinguishes_on_address(device) ||  profile->supportDeviceAddress(address)) {
                        profiles.add(profile);
                        //  10-11 17:03:20.474  1769  1801 V APM_AudioPolicyManager: checkOutputsForDevice(): adding profile 0 from module r_submix
                        ALOGV("checkOutputsForDevice(): adding profile %zu from module %s",  j, hwModule->getName());
                    }
                }
            }
        }
        
        //  10-11 17:03:20.474  1769  1801 V APM_AudioPolicyManager:   found 1 profiles, 0 outputs
        ALOGV("  found %zu profiles, %zu outputs", profiles.size(), outputs.size());

        if (profiles.isEmpty() && outputs.isEmpty()) {
            ALOGW("checkOutputsForDevice(): No output available for device %04x", device);
            return BAD_VALUE;
        }

        // open outputs for matching profiles if needed. Direct outputs are also opened to
        // query for dynamic parameters and will be closed later by setDeviceConnectionState()
        for (ssize_t profile_index = 0; profile_index < (ssize_t)profiles.size(); profile_index++) {
            sp<IOProfile> profile = profiles[profile_index];

            // nothing to do if one output is already opened for this profile
            size_t j;
            for (j = 0; j < outputs.size(); j++) {
                desc = mOutputs.valueFor(outputs.itemAt(j));
                if (!desc->isDuplicated() && desc->mProfile == profile) {
                    // matching profile: save the sample rates, format and channel masks supported
                    // by the profile in our device descriptor
                    if (audio_device_is_digital(device)) {
                        devDesc->importAudioPort(profile);
                    }
                    break;
                }T
            }
            if (j != outputs.size()) {
                continue;
            }

            if (!profile->canOpenNewIo()) {
                ALOGW("Max Output number %u already opened for this profile %s",  profile->maxOpenCount, profile->getTagName().c_str());
                continue;
            }

           
            //  10-11 17:03:20.474  1769  1801 V APM_AudioPolicyManager: opening output for device 00008000 with params 0 profile 0xee7c3640 name r_submix outputd
            ALOGV("opening output for device %08x with params %s profile %p name %s", device, address.string(), profile.get(), profile->getName().string());
            desc = new SwAudioOutputDescriptor(profile, mpClientInterface);
            audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
             /**
             * 这里 open 函数会触发 AudioFlinger::openOutput() 调用
             * 创建 r_submix 输出设备对应的线程MixerThread
             * 
            */
            status_t status = desc->open(nullptr, device, address,  AUDIO_STREAM_DEFAULT, AUDIO_OUTPUT_FLAG_NONE, &output);

            if (status == NO_ERROR) {
                // Here is where the out_set_parameters() for card & device gets called
                if (!address.isEmpty()) {
                    char *param = audio_device_address_to_parameter(device, address);
                    mpClientInterface->setParameters(output, String8(param));
                    free(param);
                }
                updateAudioProfiles(device, output, profile->getAudioProfiles());
                if (!profile->hasValidAudioProfile()) {
                    ALOGW("checkOutputsForDevice() missing param");
                    desc->close();
                    output = AUDIO_IO_HANDLE_NONE;
                } else if (profile->hasDynamicAudioProfile()) {
                    desc->close();
                    output = AUDIO_IO_HANDLE_NONE;
                    audio_config_t config = AUDIO_CONFIG_INITIALIZER;
                    profile->pickAudioProfile(config.sample_rate, config.channel_mask, config.format);
                    config.offload_info.sample_rate = config.sample_rate;
                    config.offload_info.channel_mask = config.channel_mask;
                    config.offload_info.format = config.format;

                    status_t status = desc->open(&config, device, address, AUDIO_STREAM_DEFAULT,AUDIO_OUTPUT_FLAG_NONE, &output);
                    if (status != NO_ERROR) {
                        output = AUDIO_IO_HANDLE_NONE;
                    }
                }

                if (output != AUDIO_IO_HANDLE_NONE) {
                    addOutput(output, desc);
                    if (device_distinguishes_on_address(device) && address != "0") {
                        sp<AudioPolicyMix> policyMix;
                        if (mPolicyMixes.getAudioPolicyMix(address, policyMix) != NO_ERROR) {
                            ALOGE("checkOutputsForDevice() cannot find policy for address %s",address.string());
                        }
                        policyMix->setOutput(desc);
                        desc->mPolicyMix = policyMix->getMix();

                    } else if (((desc->mFlags & AUDIO_OUTPUT_FLAG_DIRECT) == 0) &&hasPrimaryOutput()) {
                        // no duplicated output for direct outputs and
                        // outputs used by dynamic policy mixes
                        audio_io_handle_t duplicatedOutput = AUDIO_IO_HANDLE_NONE;

                        //TODO: configure audio effect output stage here

                        // open a duplicating output thread for the new output and the primary output
                        sp<SwAudioOutputDescriptor> dupOutputDesc = new SwAudioOutputDescriptor(NULL, mpClientInterface);
                        status_t status = dupOutputDesc->openDuplicating(mPrimaryOutput, desc,&duplicatedOutput);
                        if (status == NO_ERROR) {
                            // add duplicated output descriptor
                            addOutput(duplicatedOutput, dupOutputDesc);
                        } else {
                            ALOGW("checkOutputsForDevice() could not open dup output for %d and %d",mPrimaryOutput->mIoHandle, output);
                            desc->close();
                            removeOutput(output);
                            nextAudioPortGeneration();
                            output = AUDIO_IO_HANDLE_NONE;
                        }
                    }
                }
            } else {
                output = AUDIO_IO_HANDLE_NONE;
            }
            if (output == AUDIO_IO_HANDLE_NONE) {
                ALOGW("checkOutputsForDevice() could not open output for device %x", device);
                profiles.removeAt(profile_index);
                profile_index--;
            } else {
                outputs.add(output);
                // Load digital format info only for digital devices
                if (audio_device_is_digital(device)) {
                    devDesc->importAudioPort(profile);
                }

                if (device_distinguishes_on_address(device)) {
                    ALOGV("checkOutputsForDevice(): setOutputDevice(dev=0x%x, addr=%s)",
                            device, address.string());
                    setOutputDevice(desc, device, true/*force*/, 0/*delay*/,
                            NULL/*patch handle*/, address.string());
                }
                ALOGV("checkOutputsForDevice(): adding output %d", output);
            }
        }

        if (profiles.isEmpty()) {
            ALOGW("checkOutputsForDevice(): No output available for device %04x", device);
            return BAD_VALUE;
        }
    } else { // Disconnect
        // check if one opened output is not needed any more after disconnecting one device
        for (size_t i = 0; i < mOutputs.size(); i++) {
            desc = mOutputs.valueAt(i);
            if (!desc->isDuplicated()) {
                // exact match on device
                if (device_distinguishes_on_address(device) &&
                        (desc->supportedDevices() == device)) {
                    findIoHandlesByAddress(desc, device, address, outputs);
                } else if (!(desc->supportedDevices() & mAvailableOutputDevices.types())) {
                    ALOGV("checkOutputsForDevice(): disconnecting adding output %d",
                            mOutputs.keyAt(i));
                    outputs.add(mOutputs.keyAt(i));
                }
            }
        }
        // Clear any profiles associated with the disconnected device.
        for (const auto& hwModule : mHwModules) {
            for (size_t j = 0; j < hwModule->getOutputProfiles().size(); j++) {
                sp<IOProfile> profile = hwModule->getOutputProfiles()[j];
                if (profile->supportDevice(device)) {
                    ALOGV("checkOutputsForDevice(): "
                            "clearing direct output profile %zu on module %s",
                            j, hwModule->getName());
                    profile->clearAudioProfiles();
                }
            }
        }
    }
    return NO_ERROR;
}











/***
 * 
 * AudioFlinger::createTrack ---> AudioPolicyService::getOutputForAttr
 * 
 * */
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
            ALOGE("getOutputForAttr() invalid attributes: usage=%d content=%d flags=0x%x tags=[%s]",
                  attr->usage, attr->content_type, attr->flags,
                  attr->tags);
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
        *selectedDeviceId = outputDevices.size() > 0 ? outputDevices.itemAt(0)->getId() : AUDIO_PORT_HANDLE_NONE;
        ALOGV("getOutputForAttr() returns output %d", *output);
        return NO_ERROR;
    }

    if (attributes.usage == AUDIO_USAGE_VIRTUAL_SOURCE) {
        ALOGW("getOutputForAttr() no policy mix found for usage AUDIO_USAGE_VIRTUAL_SOURCE");
        return BAD_VALUE;
    }

/***
 *  10-22 09:05:43.991  2962  2999 V APM_AudioPolicyManager: getOutputForAttr() usage=1(AUDIO_USAGE_MEDIA), content=2, 
 *  tag= flags=00000200(AUDIO_FLAG_DEEP_BUFFER) session 25 selectedDeviceId 0(AUDIO_PORT_HANDLE_NONE)
 * */
    ALOGV("getOutputForAttr() usage=%d, content=%d, tag=%s flags=%08x"
            " session %d selectedDeviceId %d",
            attributes.usage, attributes.content_type, attributes.tags, attributes.flags,
            session, *selectedDeviceId);

    /**
     * 
     * @    frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
     * 由于 usage=1(AUDIO_USAGE_MEDIA) flags=00000200(AUDIO_FLAG_DEEP_BUFFER)
     * 则返回  AUDIO_STREAM_MUSIC 
    */
    *stream = streamTypefromAttributesInt(&attributes);

    // Explicit routing?
    sp<DeviceDescriptor> deviceDesc;
    /**
     * selectedDeviceId 0(AUDIO_PORT_HANDLE_NONE)
    */
    if (*selectedDeviceId != AUDIO_PORT_HANDLE_NONE) {
        deviceDesc = mAvailableOutputDevices.getDeviceFromId(*selectedDeviceId);
    }
    /**
     * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:561: 
     *  SessionRouteMap mOutputRoutes = SessionRouteMap(SessionRouteMap::MAPTYPE_OUTPUT);
     * 
    */
    mOutputRoutes.addRoute(session, *stream, SessionRoute::SOURCE_TYPE_NA, deviceDesc, uid);

    /**
     * 由于 usage=1(AUDIO_USAGE_MEDIA) flags=00000200(AUDIO_FLAG_DEEP_BUFFER)
     * 则返回  STRATEGY_MEDIA 
    */
    routing_strategy strategy = (routing_strategy) getStrategyForAttr(&attributes);
    /**
     * 
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
    if (device == AUDIO_DEVICE_OUT_TELEPHONY_TX &&
        *stream == AUDIO_STREAM_MUSIC &&
        audio_is_linear_pcm(config->format) &&
        isInCall()) {
        if (*selectedDeviceId != AUDIO_PORT_HANDLE_NONE) {
            *flags = (audio_output_flags_t)AUDIO_OUTPUT_FLAG_INCALL_MUSIC;
        } else {
            device = mEngine->getDeviceForStrategy(strategy);
        }
    }
    /***
     * 10-22 09:05:43.991  2962  2999 V APM_AudioPolicyManager: getOutputForAttr() device 0x8000, sampling rate 44100, format 0x1, channel mask 0x3, flags 0x8
     * */
    ALOGV("getOutputForAttr() device 0x%x, sampling rate %d, format %#x, channel mask %#x, "
          "flags %#x",
          device, config->sample_rate, config->format, config->channel_mask, *flags);

    *output = getOutputForDevice(device, session, *stream, config, flags);

    if (*output == AUDIO_IO_HANDLE_NONE) {
        mOutputRoutes.removeRoute(session);
        return INVALID_OPERATION;
    }

    DeviceVector outputDevices = mAvailableOutputDevices.getDevicesFromType(device);
    *selectedDeviceId = outputDevices.size() > 0 ? outputDevices.itemAt(0)->getId() : AUDIO_PORT_HANDLE_NONE;

    /**
     * 
     * 10-22 09:05:43.991  2962  2999 V APM_AudioPolicyManager:   getOutputForAttr() returns output 21 selectedDeviceId 13
    */
    ALOGV("  getOutputForAttr() returns output %d selectedDeviceId %d", *output, *selectedDeviceId);

    return NO_ERROR;
}


audio_stream_type_t AudioPolicyManager::streamTypefromAttributesInt(const audio_attributes_t *attr)
{
    /**
     * attr->flags = 00000200(AUDIO_FLAG_DEEP_BUFFER)
     * 
    */
    // flags to stream type mapping
    if ((attr->flags & AUDIO_FLAG_AUDIBILITY_ENFORCED) == AUDIO_FLAG_AUDIBILITY_ENFORCED) {
        return AUDIO_STREAM_ENFORCED_AUDIBLE;
    }
    if ((attr->flags & AUDIO_FLAG_SCO) == AUDIO_FLAG_SCO) {
        return AUDIO_STREAM_BLUETOOTH_SCO;
    }
    if ((attr->flags & AUDIO_FLAG_BEACON) == AUDIO_FLAG_BEACON) {
        return AUDIO_STREAM_TTS;
    }

    // usage to stream type mapping
    switch (attr->usage) {
    case AUDIO_USAGE_MEDIA: //  system/media/audio/include/system/audio-base.h:387:    AUDIO_USAGE_MEDIA = 1,
    case AUDIO_USAGE_GAME:
    case AUDIO_USAGE_ASSISTANT:
    case AUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE:
        return AUDIO_STREAM_MUSIC;
    case AUDIO_USAGE_ASSISTANCE_ACCESSIBILITY:
        return AUDIO_STREAM_ACCESSIBILITY;
    case AUDIO_USAGE_ASSISTANCE_SONIFICATION:
        return AUDIO_STREAM_SYSTEM;
    case AUDIO_USAGE_VOICE_COMMUNICATION:
        return AUDIO_STREAM_VOICE_CALL;

    case AUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING:
        return AUDIO_STREAM_DTMF;

    case AUDIO_USAGE_ALARM:
        return AUDIO_STREAM_ALARM;
    case AUDIO_USAGE_NOTIFICATION_TELEPHONY_RINGTONE:
        return AUDIO_STREAM_RING;

    case AUDIO_USAGE_NOTIFICATION:
    case AUDIO_USAGE_NOTIFICATION_COMMUNICATION_REQUEST:
    case AUDIO_USAGE_NOTIFICATION_COMMUNICATION_INSTANT:
    case AUDIO_USAGE_NOTIFICATION_COMMUNICATION_DELAYED:
    case AUDIO_USAGE_NOTIFICATION_EVENT:
        return AUDIO_STREAM_NOTIFICATION;

    case AUDIO_USAGE_UNKNOWN:
    default:
        return AUDIO_STREAM_MUSIC;
    }
}



uint32_t AudioPolicyManager::getStrategyForAttr(const audio_attributes_t *attr) {
    /**
     * attr->flags = 00000200(AUDIO_FLAG_DEEP_BUFFER)
     * 
    */
    // flags to strategy mapping
    if ((attr->flags & AUDIO_FLAG_BEACON) == AUDIO_FLAG_BEACON) {
        return (uint32_t) STRATEGY_TRANSMITTED_THROUGH_SPEAKER;
    }
    if ((attr->flags & AUDIO_FLAG_AUDIBILITY_ENFORCED) == AUDIO_FLAG_AUDIBILITY_ENFORCED) {
        return (uint32_t) STRATEGY_ENFORCED_AUDIBLE;
    }
    // usage to strategy mapping
    return static_cast<uint32_t>(mEngine->getStrategyForUsage(attr->usage));
}


audio_devices_t AudioPolicyManager::getDeviceForStrategy(routing_strategy strategy,bool fromCache /*= "false"*/)
{
    // Check if an explicit routing request exists for a stream type corresponding to the
    // specified strategy and use it in priority over default routing rules.
    for (int stream = 0; stream < AUDIO_STREAM_FOR_POLICY_CNT; stream++) {
        /**
         * getStrategy 直接调用 Engine::getStrategyForStream
        */
        if (getStrategy((audio_stream_type_t)stream) == strategy) {
            /**
             * 
            */
            audio_devices_t forcedDevice = mOutputRoutes.getActiveDeviceForStream((audio_stream_type_t)stream, mAvailableOutputDevices);
            if (forcedDevice != AUDIO_DEVICE_NONE) {
                return forcedDevice;
            }
        }
    }

    if (fromCache) {
        ALOGVV("getDeviceForStrategy() from cache strategy %d, device %x",strategy, mDeviceForStrategy[strategy]);
        return mDeviceForStrategy[strategy];
    }


    /**
     * 
     * 
    */
    return mEngine->getDeviceForStrategy(strategy);
}



audio_io_handle_t AudioPolicyManager::getOutputForDevice(
        audio_devices_t device,
        audio_session_t session,
        audio_stream_type_t stream,
        const audio_config_t *config,
        audio_output_flags_t *flags)
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
    } else if (/* stream == AUDIO_STREAM_MUSIC && */
            *flags == AUDIO_OUTPUT_FLAG_NONE &&
            property_get_bool("audio.deep_buffer.media", false /* default_value */)) {
        // use DEEP_BUFFER as default output for music stream type
        *flags = (audio_output_flags_t)AUDIO_OUTPUT_FLAG_DEEP_BUFFER;
    }
    if (stream == AUDIO_STREAM_TTS) {
        *flags = AUDIO_OUTPUT_FLAG_TTS;
    } else if (stream == AUDIO_STREAM_VOICE_CALL &&
               audio_is_linear_pcm(config->format)) {
        *flags = (audio_output_flags_t)(AUDIO_OUTPUT_FLAG_VOIP_RX |
                                       AUDIO_OUTPUT_FLAG_DIRECT);
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

    if (((*flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) == 0) ||
            !(mEffects.isNonOffloadableEffectEnabled() || mMasterMono)) {
        profile = getProfileForDirectOutput(device,
                                           config->sample_rate,
                                           config->format,
                                           config->channel_mask,
                                           (audio_output_flags_t)*flags);
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
                    ALOGI("getOutputForDevice() reusing direct output %d for session %d",
                        mOutputs.keyAt(i), session);
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

        /**
         * 会出发  AudioFlinger::openOutput_l
        */
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
            if (audio_is_linear_pcm(config->format) &&
                    config->sample_rate  <= SAMPLE_RATE_HZ_MAX) {
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
    ALOGW_IF((output == 0), "getOutputForDevice() could not find output for stream %d, "
            "sampling rate %d, format %#x, channels %#x, flags %#x",
            stream, config->sample_rate, config->format, config->channel_mask, *flags);

    return output;
}



SortedVector<audio_io_handle_t> AudioPolicyManager::getOutputsForDevice(audio_devices_t device,const SwAudioOutputCollection& openOutputs)
{
    SortedVector<audio_io_handle_t> outputs;

    ALOGVV("getOutputsForDevice() device %04x", device);
    for (size_t i = 0; i < openOutputs.size(); i++) {
        ALOGVV("output %zu isDuplicated=%d device=%04x",i, openOutputs.valueAt(i)->isDuplicated(),openOutputs.valueAt(i)->supportedDevices());
        if ((device & openOutputs.valueAt(i)->supportedDevices()) == device) {
            ALOGVV("getOutputsForDevice() found output %d", openOutputs.keyAt(i));
            outputs.add(openOutputs.keyAt(i));
        }
    }
    return outputs;
}


audio_io_handle_t AudioPolicyManager::selectOutput(const SortedVector<audio_io_handle_t>& outputs,
                                                       audio_output_flags_t flags,
                                                       audio_format_t format)
{
    // select one output among several that provide a path to a particular device or set of
    // devices (the list was previously build by getOutputsForDevice()).
    // The priority is as follows:
    // 1: the output with the highest number of requested policy flags
    // 2: the output with the bit depth the closest to the requested one
    // 3: the primary output
    // 4: the first output in the list

    if (outputs.size() == 0) {
        return AUDIO_IO_HANDLE_NONE;
    }
    if (outputs.size() == 1) {
        return outputs[0];
    }

    int maxCommonFlags = 0;
    audio_io_handle_t outputForFlags = AUDIO_IO_HANDLE_NONE;
    audio_io_handle_t outputForPrimary = AUDIO_IO_HANDLE_NONE;
    audio_io_handle_t outputForFormat = AUDIO_IO_HANDLE_NONE;
    audio_format_t bestFormat = AUDIO_FORMAT_INVALID;
    audio_format_t bestFormatForFlags = AUDIO_FORMAT_INVALID;

    for (audio_io_handle_t output : outputs) {
        sp<SwAudioOutputDescriptor> outputDesc = mOutputs.valueFor(output);
        if (!outputDesc->isDuplicated()) {
            // if a valid format is specified, skip output if not compatible
            if (format != AUDIO_FORMAT_INVALID) {
                if (outputDesc->mFlags & AUDIO_OUTPUT_FLAG_DIRECT) {
                    if (format != outputDesc->mFormat) {
                        continue;
                    }
                } else if (!audio_is_linear_pcm(format)) {
                    continue;
                }
                if (AudioPort::isBetterFormatMatch(outputDesc->mFormat, bestFormat, format)) {
                    outputForFormat = output;
                    bestFormat = outputDesc->mFormat;
                }
            }

            int commonFlags = popcount(outputDesc->mProfile->getFlags() & flags);
            if (commonFlags >= maxCommonFlags) {
                if (commonFlags == maxCommonFlags) {
                    if (format != AUDIO_FORMAT_INVALID
                            && AudioPort::isBetterFormatMatch(
                                    outputDesc->mFormat, bestFormatForFlags, format)) {
                        outputForFlags = output;
                        bestFormatForFlags = outputDesc->mFormat;
                    }
                } else {
                    outputForFlags = output;
                    maxCommonFlags = commonFlags;
                    bestFormatForFlags = outputDesc->mFormat;
                }
                ALOGV("selectOutput() commonFlags for output %d, %04x", output, commonFlags);
            }
            if (outputDesc->mProfile->getFlags() & AUDIO_OUTPUT_FLAG_PRIMARY) {
                outputForPrimary = output;
            }
        }
    }

    if (outputForFlags != AUDIO_IO_HANDLE_NONE) {
        return outputForFlags;
    }
    if (outputForFormat != AUDIO_IO_HANDLE_NONE) {
        return outputForFormat;
    }
    if (outputForPrimary != AUDIO_IO_HANDLE_NONE) {
        return outputForPrimary;
    }

    return outputs[0];
}