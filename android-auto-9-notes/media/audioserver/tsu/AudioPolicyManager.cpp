//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
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
        status = mPolicyMixes.getInputMixForAttr(*attr, &policyMix);
        if (status != NO_ERROR) {
            goto error;
        }
        *inputType = API_INPUT_MIX_EXT_POLICY_REROUTE;
        device = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
        address = String8(attr->tags + strlen("addr="));
    } else {
        device = getDeviceAndMixForInputSource(inputSource, &policyMix);

        // 10-10 13:44:10.458  1771  1771 V APM_AudioPolicyManager: getInputForAttr() device -2147483392
        ALOGV("getInputForAttr() device %#x",device);
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

    ALOGV("getInputForAttr() availableDeviceTypes(%#x) selectedDeviceFromMix(%#x)",availableDeviceTypes,selectedDeviceFromMix);

    if (selectedDeviceFromMix != AUDIO_DEVICE_NONE) {
        return selectedDeviceFromMix;
    }
    
    return getDeviceForInputSource(inputSource);
}

//  @   frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPolicyMix.cpp
audio_devices_t AudioPolicyMixCollection::getDeviceAndMixForInputSource(audio_source_t inputSource,audio_devices_t availDevices,AudioMix **policyMix)
{
    for (size_t i = 0; i < size(); i++) {
        AudioMix *mix = valueAt(i)->getMix();

        if (mix->mMixType != MIX_TYPE_RECORDERS) {
            continue;
        }

        for (size_t j = 0; j < mix->mCriteria.size(); j++) {
            if ((RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET == mix->mCriteria[j].mRule && mix->mCriteria[j].mValue.mSource == inputSource) ||
               (RULE_EXCLUDE_ATTRIBUTE_CAPTURE_PRESET == mix->mCriteria[j].mRule && mix->mCriteria[j].mValue.mSource != inputSource)) {
                /**
                 * availDevices = 0x104
                 * 
                 * system/media/audio/include/system/audio-base.h:337:    AUDIO_DEVICE_IN_REMOTE_SUBMIX              = 0x80000100u, // BIT_IN | 0x100
                */
                if (availDevices & AUDIO_DEVICE_IN_REMOTE_SUBMIX) {
                    if (policyMix != NULL) {
                        *policyMix = mix;
                    }
                    ALOGV("getDeviceAndMixForInputSource %p",policyMix);
                    return AUDIO_DEVICE_IN_REMOTE_SUBMIX;
                }
                break;
            }
        }
    }
    return AUDIO_DEVICE_NONE;
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
    ALOGV("setDeviceConnectionStateInt() device: 0x%X, state %d, address %s name %s", device, state, device_address, device_name);

    // connect/disconnect only 1 device at a time
    if (!audio_is_output_device(device) && !audio_is_input_device(device)) return BAD_VALUE;

    sp<DeviceDescriptor> devDesc = mHwModules.getDeviceDescriptor(device, device_address, device_name);

    // handle output devices
    if (audio_is_output_device(device)) {
        SortedVector <audio_io_handle_t> outputs;

        ssize_t index = mAvailableOutputDevices.indexOf(devDesc);

        // save a copy of the opened output descriptors before any output is opened or closed
        // by checkOutputsForDevice(). This will be needed by checkOutputForAllStrategies()
        mPreviousOutputs = mOutputs;
        switch (state)
        {
        // handle output device connection
        case AUDIO_POLICY_DEVICE_STATE_AVAILABLE: {
            if (index >= 0) {
                ALOGW("setDeviceConnectionState() device already connected: %x", device);
                return INVALID_OPERATION;
            }
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

            if (checkOutputsForDevice(devDesc, state, outputs, devDesc->mAddress) != NO_ERROR) {
                mAvailableOutputDevices.remove(devDesc);

                broadcastDeviceConnectionState(device, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,devDesc->mAddress);
                return INVALID_OPERATION;
            }
            // Propagate device availability to Engine
            mEngine->setDeviceConnectionState(devDesc, state);

            // outputs should never be empty here
            ALOG_ASSERT(outputs.size() != 0, "setDeviceConnectionState():" "checkOutputsForDevice() returned no outputs but status OK");
            ALOGV("setDeviceConnectionState() checkOutputsForDevice() returned %zu outputs", outputs.size());

            } break;
        // handle output device disconnection
        case AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE: {
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