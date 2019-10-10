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