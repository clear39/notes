//	@	frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp


audio_policy_forced_cfg_t AudioPolicyManager::getForceUse(audio_policy_force_use_t usage)
{
    return mEngine->getForceUse(usage);
}

void AudioPolicyManager::setForceUse(audio_policy_force_use_t usage,
                                     audio_policy_forced_cfg_t config)
{
    ALOGV("setForceUse() usage %d, config %d, mPhoneState %d", usage, config, mEngine->getPhoneState());
    if (config == mEngine->getForceUse(usage)) {
        return;
    }

    if (mEngine->setForceUse(usage, config) != NO_ERROR) {
        ALOGW("setForceUse() could not set force cfg %d for usage %d", config, usage);
        return;
    }
    bool forceVolumeReeval = (usage == AUDIO_POLICY_FORCE_FOR_COMMUNICATION) ||
            (usage == AUDIO_POLICY_FORCE_FOR_DOCK) ||
            (usage == AUDIO_POLICY_FORCE_FOR_SYSTEM);

    // check for device and output changes triggered by new force usage
    checkForDeviceAndOutputChanges();

    // force client reconnection to reevaluate flag AUDIO_FLAG_AUDIBILITY_ENFORCED
    if (usage == AUDIO_POLICY_FORCE_FOR_SYSTEM) {
        mpClientInterface->invalidateStream(AUDIO_STREAM_SYSTEM);
        mpClientInterface->invalidateStream(AUDIO_STREAM_ENFORCED_AUDIBLE);
    }

    //FIXME: workaround for truncated touch sounds
    // to be removed when the problem is handled by system UI
    uint32_t delayMs = 0;
    if (usage == AUDIO_POLICY_FORCE_FOR_COMMUNICATION) {
        delayMs = TOUCH_SOUND_FIXED_DELAY_MS;
    }

    updateCallAndOutputRouting(forceVolumeReeval, delayMs);

    for (const auto& activeDesc : mInputs.getActiveInputs()) {
        auto newDevice = getNewInputDevice(activeDesc);
        // Force new input selection if the new device can not be reached via current input
        if (activeDesc->mProfile->getSupportedDevices().contains(newDevice)) {
            setInputDevice(activeDesc->mIoHandle, newDevice);
        } else {
            closeInput(activeDesc->mIoHandle);
        }
    }
}

void AudioPolicyManager::checkForDeviceAndOutputChanges(std::function<bool()> onOutputsChecked)
{
    // checkA2dpSuspend must run before checkOutputForAllStrategies so that A2DP
    // output is suspended before any tracks are moved to it
    checkA2dpSuspend();
    checkOutputForAllStrategies();
    checkSecondaryOutputs();
    if (onOutputsChecked != nullptr && onOutputsChecked()) checkA2dpSuspend();
    updateDevicesAndOutputs();
    if (mHwModules.getModuleFromName(AUDIO_HARDWARE_MODULE_ID_MSD) != 0) {
        setMsdPatch();
    }
}





////////////////////////////////////////////////////////////////

status_t AudioPolicyManager::getOutputForAttr(const audio_attributes_t *attr,
                                              audio_io_handle_t *output,
                                              audio_session_t session,
                                              audio_stream_type_t *stream,
                                              uid_t uid,
                                              const audio_config_t *config,
                                              audio_output_flags_t *flags,
                                              audio_port_handle_t *selectedDeviceId,
                                              audio_port_handle_t *portId,
                                              std::vector<audio_io_handle_t> *secondaryOutputs,
                                              output_type_t *outputType)
{
    // The supplied portId must be AUDIO_PORT_HANDLE_NONE
    if (*portId != AUDIO_PORT_HANDLE_NONE) {
        return INVALID_OPERATION;
    }
    const audio_port_handle_t requestedPortId = *selectedDeviceId;
    audio_attributes_t resultAttr;
    bool isRequestedDeviceForExclusiveUse = false;
    std::vector<sp<AudioPolicyMix>> secondaryMixes;
    const sp<DeviceDescriptor> requestedDevice =
      mAvailableOutputDevices.getDeviceFromId(requestedPortId);

    // Prevent from storing invalid requested device id in clients
    const audio_port_handle_t sanitizedRequestedPortId =
      requestedDevice != nullptr ? requestedPortId : AUDIO_PORT_HANDLE_NONE;
    *selectedDeviceId = sanitizedRequestedPortId;

    status_t status = getOutputForAttrInt(&resultAttr, output, session, attr, stream, uid,
            config, flags, selectedDeviceId, &isRequestedDeviceForExclusiveUse,
            secondaryOutputs != nullptr ? &secondaryMixes : nullptr, outputType);
    if (status != NO_ERROR) {
        return status;
    }
    std::vector<wp<SwAudioOutputDescriptor>> weakSecondaryOutputDescs;
    if (secondaryOutputs != nullptr) {
        for (auto &secondaryMix : secondaryMixes) {
            sp<SwAudioOutputDescriptor> outputDesc = secondaryMix->getOutput();
            if (outputDesc != nullptr &&
                outputDesc->mIoHandle != AUDIO_IO_HANDLE_NONE) {
                secondaryOutputs->push_back(outputDesc->mIoHandle);
                weakSecondaryOutputDescs.push_back(outputDesc);
            }
        }
    }

    audio_config_base_t clientConfig = {.sample_rate = config->sample_rate,
        .channel_mask = config->channel_mask,
        .format = config->format,
    };
    *portId = PolicyAudioPort::getNextUniqueId();

    sp<SwAudioOutputDescriptor> outputDesc = mOutputs.valueFor(*output);
    sp<TrackClientDescriptor> clientDesc =
        new TrackClientDescriptor(*portId, uid, session, resultAttr, clientConfig,
                                  sanitizedRequestedPortId, *stream,
                                  mEngine->getProductStrategyForAttributes(resultAttr),
                                  toVolumeSource(resultAttr),
                                  *flags, isRequestedDeviceForExclusiveUse,
                                  std::move(weakSecondaryOutputDescs),
                                  outputDesc->mPolicyMix);
    outputDesc->addClient(clientDesc);

    ALOGV("%s() returns output %d requestedPortId %d selectedDeviceId %d for port ID %d", __func__,
          *output, requestedPortId, *selectedDeviceId, *portId);

    return NO_ERROR;
}




status_t AudioPolicyManager::getOutputForAttrInt(
        audio_attributes_t *resultAttr,
        audio_io_handle_t *output,
        audio_session_t session,
        const audio_attributes_t *attr,
        audio_stream_type_t *stream,
        uid_t uid,
        const audio_config_t *config,
        audio_output_flags_t *flags,
        audio_port_handle_t *selectedDeviceId,
        bool *isRequestedDeviceForExclusiveUse,
        std::vector<sp<AudioPolicyMix>> *secondaryMixes,
        output_type_t *outputType)
{
    DeviceVector outputDevices;
    const audio_port_handle_t requestedPortId = *selectedDeviceId;
    DeviceVector msdDevices = getMsdAudioOutDevices();
    const sp<DeviceDescriptor> requestedDevice =
        mAvailableOutputDevices.getDeviceFromId(requestedPortId);

    *outputType = API_OUTPUT_INVALID;
    status_t status = getAudioAttributes(resultAttr, attr, *stream);
    if (status != NO_ERROR) {
        return status;
    }
    if (auto it = mAllowedCapturePolicies.find(uid); it != end(mAllowedCapturePolicies)) {
        resultAttr->flags |= it->second;
    }
    *stream = mEngine->getStreamTypeForAttributes(*resultAttr);

    ALOGV("%s() attributes=%s stream=%s session %d selectedDeviceId %d", __func__,
          toString(*resultAttr).c_str(), toString(*stream).c_str(), session, requestedPortId);

    // The primary output is the explicit routing (eg. setPreferredDevice) if specified,
    //       otherwise, fallback to the dynamic policies, if none match, query the engine.
    // Secondary outputs are always found by dynamic policies as the engine do not support them
    sp<AudioPolicyMix> primaryMix;
    status = mPolicyMixes.getOutputForAttr(*resultAttr, uid, *flags, primaryMix, secondaryMixes);
    if (status != OK) {
        return status;
    }

    // Explicit routing is higher priority then any dynamic policy primary output
    bool usePrimaryOutputFromPolicyMixes = requestedDevice == nullptr && primaryMix != nullptr;

    // FIXME: in case of RENDER policy, the output capabilities should be checked
    if ((usePrimaryOutputFromPolicyMixes
            || (secondaryMixes != nullptr && !secondaryMixes->empty()))
        && !audio_is_linear_pcm(config->format)) {
        ALOGD("%s: rejecting request as dynamic audio policy only support pcm", __func__);
        return BAD_VALUE;
    }
    if (usePrimaryOutputFromPolicyMixes) {
        sp<DeviceDescriptor> deviceDesc =
                mAvailableOutputDevices.getDevice(primaryMix->mDeviceType,
                                                  primaryMix->mDeviceAddress,
                                                  AUDIO_FORMAT_DEFAULT);
        sp<SwAudioOutputDescriptor> policyDesc = primaryMix->getOutput();
        if (deviceDesc != nullptr
                && (policyDesc == nullptr || (policyDesc->mFlags & AUDIO_OUTPUT_FLAG_DIRECT))) {
            audio_io_handle_t newOutput;
            status = openDirectOutput(
                    *stream, session, config,
                    (audio_output_flags_t)(*flags | AUDIO_OUTPUT_FLAG_DIRECT),
                    DeviceVector(deviceDesc), &newOutput);
            if (status != NO_ERROR) {
                policyDesc = nullptr;
            } else {
                policyDesc = mOutputs.valueFor(newOutput);
                primaryMix->setOutput(policyDesc);
            }
        }
        if (policyDesc != nullptr) {
            policyDesc->mPolicyMix = primaryMix;
            *output = policyDesc->mIoHandle;
            *selectedDeviceId = deviceDesc != 0 ? deviceDesc->getId() : AUDIO_PORT_HANDLE_NONE;

            ALOGV("getOutputForAttr() returns output %d", *output);
            if (resultAttr->usage == AUDIO_USAGE_VIRTUAL_SOURCE) {
                *outputType = API_OUT_MIX_PLAYBACK;
            } else {
                *outputType = API_OUTPUT_LEGACY;
            }
            return NO_ERROR;
        }
    }
    // Virtual sources must always be dynamicaly or explicitly routed
    if (resultAttr->usage == AUDIO_USAGE_VIRTUAL_SOURCE) {
        ALOGW("getOutputForAttr() no policy mix found for usage AUDIO_USAGE_VIRTUAL_SOURCE");
        return BAD_VALUE;
    }
    // explicit routing managed by getDeviceForStrategy in APM is now handled by engine
    // in order to let the choice of the order to future vendor engine
    outputDevices = mEngine->getOutputDevicesForAttributes(*resultAttr, requestedDevice, false);

    if ((resultAttr->flags & AUDIO_FLAG_HW_AV_SYNC) != 0) {
        *flags = (audio_output_flags_t)(*flags | AUDIO_OUTPUT_FLAG_HW_AV_SYNC);
    }

    // Set incall music only if device was explicitly set, and fallback to the device which is
    // chosen by the engine if not.
    // FIXME: provide a more generic approach which is not device specific and move this back
    // to getOutputForDevice.
    // TODO: Remove check of AUDIO_STREAM_MUSIC once migration is completed on the app side.
    if (outputDevices.onlyContainsDevicesWithType(AUDIO_DEVICE_OUT_TELEPHONY_TX) &&
        (*stream == AUDIO_STREAM_MUSIC  || resultAttr->usage == AUDIO_USAGE_VOICE_COMMUNICATION) &&
        audio_is_linear_pcm(config->format) &&
        isCallAudioAccessible()) {
        if (requestedPortId != AUDIO_PORT_HANDLE_NONE) {
            *flags = (audio_output_flags_t)AUDIO_OUTPUT_FLAG_INCALL_MUSIC;
            *isRequestedDeviceForExclusiveUse = true;
        }
    }

    ALOGV("%s() device %s, sampling rate %d, format %#x, channel mask %#x, flags %#x stream %s",
          __func__, outputDevices.toString().c_str(), config->sample_rate, config->format,
          config->channel_mask, *flags, toString(*stream).c_str());

    *output = AUDIO_IO_HANDLE_NONE;
    if (!msdDevices.isEmpty()) {
        *output = getOutputForDevices(msdDevices, session, *stream, config, flags);
        sp<DeviceDescriptor> device = outputDevices.isEmpty() ? nullptr : outputDevices.itemAt(0);
        if (*output != AUDIO_IO_HANDLE_NONE && setMsdPatch(device) == NO_ERROR) {
            ALOGV("%s() Using MSD devices %s instead of devices %s",
                  __func__, msdDevices.toString().c_str(), outputDevices.toString().c_str());
            outputDevices = msdDevices;
        } else {
            *output = AUDIO_IO_HANDLE_NONE;
        }
    }
    if (*output == AUDIO_IO_HANDLE_NONE) {
        *output = getOutputForDevices(outputDevices, session, *stream, config,
                flags, resultAttr->flags & AUDIO_FLAG_MUTE_HAPTIC);
    }
    if (*output == AUDIO_IO_HANDLE_NONE) {
        return INVALID_OPERATION;
    }

    *selectedDeviceId = getFirstDeviceId(outputDevices);

    if (outputDevices.onlyContainsDevicesWithType(AUDIO_DEVICE_OUT_TELEPHONY_TX)) {
        *outputType = API_OUTPUT_TELEPHONY_TX;
    } else {
        *outputType = API_OUTPUT_LEGACY;
    }

    ALOGV("%s returns output %d selectedDeviceId %d", __func__, *output, *selectedDeviceId);

    return NO_ERROR;
}

sp<DeviceDescriptor> AudioPolicyManager::getMsdAudioInDevice() const {
    auto msdInDevices = mHwModules.getAvailableDevicesFromModuleName(AUDIO_HARDWARE_MODULE_ID_MSD,
                                                                     mAvailableInputDevices);
    return msdInDevices.isEmpty()? nullptr : msdInDevices.itemAt(0);
}

audio_io_handle_t AudioPolicyManager::getOutputForDevices(
        const DeviceVector &devices,
        audio_session_t session,
        audio_stream_type_t stream,
        const audio_config_t *config,
        audio_output_flags_t *flags,
        bool forceMutingHaptic)
{
    audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;

    // Discard haptic channel mask when forcing muting haptic channels.
    audio_channel_mask_t channelMask = forceMutingHaptic
            ? (config->channel_mask & ~AUDIO_CHANNEL_HAPTIC_ALL) : config->channel_mask;

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
               audio_is_linear_pcm(config->format) &&
               (*flags & AUDIO_OUTPUT_FLAG_INCALL_MUSIC) == 0) {
        *flags = (audio_output_flags_t)(AUDIO_OUTPUT_FLAG_VOIP_RX |
                                       AUDIO_OUTPUT_FLAG_DIRECT);
        ALOGV("Set VoIP and Direct output flags for PCM format");
    }

    audio_config_t directConfig = *config;
    directConfig.channel_mask = channelMask;
    //
    status_t status = openDirectOutput(stream, session, &directConfig, *flags, devices, &output);
    if (status != NAME_NOT_FOUND) {
        return output;
    }

    // A request for HW A/V sync cannot fallback to a mixed output because time
    // stamps are embedded in audio data
    if ((*flags & (AUDIO_OUTPUT_FLAG_HW_AV_SYNC | AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) != 0) {
        return AUDIO_IO_HANDLE_NONE;
    }

    // ignoring channel mask due to downmix capability in mixer

    // open a non direct output

    // for non direct outputs, only PCM is supported

    // audio_is_linear_pcm 用于判断是不是pcm格式
    if (audio_is_linear_pcm(config->format)) {
        // get which output is suitable for the specified stream. The actual
        // routing change will happen when startOutput() will be called
        SortedVector<audio_io_handle_t> outputs = getOutputsForDevices(devices, mOutputs);

        // at this stage we should ignore the DIRECT flag as no direct output could be found earlier
        *flags = (audio_output_flags_t)(*flags & ~AUDIO_OUTPUT_FLAG_DIRECT);
        output = selectOutput(outputs, *flags, config->format, channelMask, config->sample_rate);
    }
    ALOGW_IF((output == 0), "getOutputForDevices() could not find output for stream %d, "
            "sampling rate %d, format %#x, channels %#x, flags %#x",
            stream, config->sample_rate, config->format, channelMask, *flags);

    return output;
}



status_t AudioPolicyManager::openDirectOutput(audio_stream_type_t stream,
                                              audio_session_t session,
                                              const audio_config_t *config,
                                              audio_output_flags_t flags,
                                              const DeviceVector &devices,
                                              audio_io_handle_t *output) {

    *output = AUDIO_IO_HANDLE_NONE;

    // skip direct output selection if the request can obviously be attached to a mixed output
    // and not explicitly requested
    // const static int32_t SAMPLE_RATE_HZ_MAX = 192000;
    if (((flags & AUDIO_OUTPUT_FLAG_DIRECT) == 0) &&
            audio_is_linear_pcm(config->format) && config->sample_rate <= SAMPLE_RATE_HZ_MAX &&
            audio_channel_count_from_out_mask(config->channel_mask) <= 2) {
        return NAME_NOT_FOUND;
    }

    // Do not allow offloading if one non offloadable effect is enabled or MasterMono is enabled.
    // This prevents creating an offloaded track and tearing it down immediately after start
    // when audioflinger detects there is an active non offloadable effect.
    // FIXME: We should check the audio session here but we do not have it in this context.
    // This may prevent offloading in rare situations where effects are left active by apps
    // in the background.
    sp<IOProfile> profile;
    if (((flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) == 0) ||
            !(mEffects.isNonOffloadableEffectEnabled() || mMasterMono)) {

        profile = getProfileForOutput(
                devices, config->sample_rate, config->format, config->channel_mask,
                flags, true /* directOnly */);
    }

    if (profile == nullptr) {
        return NAME_NOT_FOUND;
    }

    // exclusive outputs for MMAP and Offload are enforced by different session ids.
    for (size_t i = 0; i < mOutputs.size(); i++) {
        sp<SwAudioOutputDescriptor> desc = mOutputs.valueAt(i);
        if (!desc->isDuplicated() && (profile == desc->mProfile)) {
            // reuse direct output if currently open by the same client
            // and configured with same parameters
            if ((config->sample_rate == desc->getSamplingRate()) &&
                (config->format == desc->getFormat()) &&
                (config->channel_mask == desc->getChannelMask()) &&
                (session == desc->mDirectClientSession)) {
                desc->mDirectOpenCount++;
                ALOGI("%s reusing direct output %d for session %d", __func__,
                    mOutputs.keyAt(i), session);
                *output = mOutputs.keyAt(i);
                return NO_ERROR;
            }
        }
    }

    if (!profile->canOpenNewIo()) {
        return NAME_NOT_FOUND;
    }

    sp<SwAudioOutputDescriptor> outputDesc =
            new SwAudioOutputDescriptor(profile, mpClientInterface);

    String8 address = getFirstDeviceAddress(devices);

    // MSD patch may be using the only output stream that can service this request. Release
    // MSD patch to prioritize this request over any active output on MSD.
    AudioPatchCollection msdPatches = getMsdPatches();
    for (size_t i = 0; i < msdPatches.size(); i++) {
        const auto& patch = msdPatches[i];
        for (size_t j = 0; j < patch->mPatch.num_sinks; ++j) {
            const struct audio_port_config *sink = &patch->mPatch.sinks[j];
            if (sink->type == AUDIO_PORT_TYPE_DEVICE &&
                    devices.containsDeviceWithType(sink->ext.device.type) &&
                    (address.isEmpty() || strncmp(sink->ext.device.address, address.string(),
                            AUDIO_DEVICE_MAX_ADDRESS_LEN) == 0)) {
                releaseAudioPatch(patch->getHandle(), mUidCached);
                break;
            }
        }
    }

    status_t status = outputDesc->open(config, devices, stream, flags, output);

    // only accept an output with the requested parameters
    if (status != NO_ERROR ||
        (config->sample_rate != 0 && config->sample_rate != outputDesc->getSamplingRate()) ||
        (config->format != AUDIO_FORMAT_DEFAULT && config->format != outputDesc->getFormat()) ||
        (config->channel_mask != 0 && config->channel_mask != outputDesc->getChannelMask())) {
        ALOGV("%s failed opening direct output: output %d sample rate %d %d,"
                "format %d %d, channel mask %04x %04x", __func__, *output, config->sample_rate,
                outputDesc->getSamplingRate(), config->format, outputDesc->getFormat(),
                config->channel_mask, outputDesc->getChannelMask());
        if (*output != AUDIO_IO_HANDLE_NONE) {
            outputDesc->close();
        }
        // fall back to mixer output if possible when the direct output could not be open
        if (audio_is_linear_pcm(config->format) &&
                config->sample_rate  <= SAMPLE_RATE_HZ_MAX) {
            return NAME_NOT_FOUND;
        }
        *output = AUDIO_IO_HANDLE_NONE;
        return BAD_VALUE;
    }
    outputDesc->mDirectOpenCount = 1;
    outputDesc->mDirectClientSession = session;

    addOutput(*output, outputDesc);
    mPreviousOutputs = mOutputs;
    ALOGV("%s returns new direct output %d", __func__, *output);
    mpClientInterface->onAudioPortListUpdate();
    return NO_ERROR;
}

SortedVector<audio_io_handle_t> AudioPolicyManager::getOutputsForDevices(
            const DeviceVector &devices,
            const SwAudioOutputCollection& openOutputs)
{
    SortedVector<audio_io_handle_t> outputs;

    ALOGVV("%s() devices %s", __func__, devices.toString().c_str());
    for (size_t i = 0; i < openOutputs.size(); i++) {
        ALOGVV("output %zu isDuplicated=%d device=%s",
                i, openOutputs.valueAt(i)->isDuplicated(),
                openOutputs.valueAt(i)->supportedDevices().toString().c_str());
        if (openOutputs.valueAt(i)->supportsAllDevices(devices)
                && openOutputs.valueAt(i)->devicesSupportEncodedFormats(devices.types())) {
            ALOGVV("%s() found output %d", __func__, openOutputs.keyAt(i));
            outputs.add(openOutputs.keyAt(i));
        }
    }
    return outputs;
}

audio_io_handle_t AudioPolicyManager::selectOutput(const SortedVector<audio_io_handle_t>& outputs,
                                                       audio_output_flags_t flags,
                                                       audio_format_t format,
                                                       audio_channel_mask_t channelMask,
                                                       uint32_t samplingRate)
{
    LOG_ALWAYS_FATAL_IF(!(format == AUDIO_FORMAT_INVALID || audio_is_linear_pcm(format)),
        "%s called with format %#x", __func__, format);

    // Flags disqualifying an output: the match must happen before calling selectOutput()
    static const audio_output_flags_t kExcludedFlags = (audio_output_flags_t)
        (AUDIO_OUTPUT_FLAG_HW_AV_SYNC | AUDIO_OUTPUT_FLAG_MMAP_NOIRQ | AUDIO_OUTPUT_FLAG_DIRECT);

    // Flags expressing a functional request: must be honored in priority over
    // other criteria
    static const audio_output_flags_t kFunctionalFlags = (audio_output_flags_t)
        (AUDIO_OUTPUT_FLAG_VOIP_RX | AUDIO_OUTPUT_FLAG_INCALL_MUSIC |
            AUDIO_OUTPUT_FLAG_TTS | AUDIO_OUTPUT_FLAG_DIRECT_PCM);
    // Flags expressing a performance request: have lower priority than serving
    // requested sampling rate or channel mask
    static const audio_output_flags_t kPerformanceFlags = (audio_output_flags_t)
        (AUDIO_OUTPUT_FLAG_FAST | AUDIO_OUTPUT_FLAG_DEEP_BUFFER |
            AUDIO_OUTPUT_FLAG_RAW | AUDIO_OUTPUT_FLAG_SYNC);

    const audio_output_flags_t functionalFlags =
        (audio_output_flags_t)(flags & kFunctionalFlags);
    const audio_output_flags_t performanceFlags =
        (audio_output_flags_t)(flags & kPerformanceFlags);

    audio_io_handle_t bestOutput = (outputs.size() == 0) ? AUDIO_IO_HANDLE_NONE : outputs[0];

    // select one output among several that provide a path to a particular device or set of
    // devices (the list was previously build by getOutputsForDevices()).
    // The priority is as follows:
    // 1: the output supporting haptic playback when requesting haptic playback
    // 2: the output with the highest number of requested functional flags
    // 3: the output supporting the exact channel mask
    // 4: the output with a higher channel count than requested
    // 5: the output with a higher sampling rate than requested
    // 6: the output with the highest number of requested performance flags
    // 7: the output with the bit depth the closest to the requested one
    // 8: the primary output
    // 9: the first output in the list

    // matching criteria values in priority order for best matching output so far
    std::vector<uint32_t> bestMatchCriteria(8, 0);

    const uint32_t channelCount = audio_channel_count_from_out_mask(channelMask);
    const uint32_t hapticChannelCount = audio_channel_count_from_out_mask(
        channelMask & AUDIO_CHANNEL_HAPTIC_ALL);

    for (audio_io_handle_t output : outputs) {
        sp<SwAudioOutputDescriptor> outputDesc = mOutputs.valueFor(output);
        // matching criteria values in priority order for current output
        std::vector<uint32_t> currentMatchCriteria(8, 0);

        //如果为复制SwAudioOutputDescriptor直接continue
        if (outputDesc->isDuplicated()) {
            continue;
        }
        //
        if ((kExcludedFlags & outputDesc->mFlags) != 0) {
            continue;
        }

        // If haptic channel is specified, use the haptic output if present.
        // When using haptic output, same audio format and sample rate are required.
        const uint32_t outputHapticChannelCount = audio_channel_count_from_out_mask(
            outputDesc->getChannelMask() & AUDIO_CHANNEL_HAPTIC_ALL);
        if ((hapticChannelCount == 0) != (outputHapticChannelCount == 0)) {
            continue;
        }
        if (outputHapticChannelCount >= hapticChannelCount
            && format == outputDesc->getFormat()
            && samplingRate == outputDesc->getSamplingRate()) {
                currentMatchCriteria[0] = outputHapticChannelCount;
        }

        // functional flags match
        currentMatchCriteria[1] = popcount(outputDesc->mFlags & functionalFlags);

        // channel mask and channel count match
        uint32_t outputChannelCount = audio_channel_count_from_out_mask(
                outputDesc->getChannelMask());
        if (channelMask != AUDIO_CHANNEL_NONE && channelCount > 2 &&
            channelCount <= outputChannelCount) {
            if ((audio_channel_mask_get_representation(channelMask) ==
                    audio_channel_mask_get_representation(outputDesc->getChannelMask())) &&
                    ((channelMask & outputDesc->getChannelMask()) == channelMask)) {
                currentMatchCriteria[2] = outputChannelCount;
            }
            currentMatchCriteria[3] = outputChannelCount;
        }

        // sampling rate match
        if (samplingRate > SAMPLE_RATE_HZ_DEFAULT &&
                samplingRate <= outputDesc->getSamplingRate()) {
            currentMatchCriteria[4] = outputDesc->getSamplingRate();
        }

        // performance flags match
        currentMatchCriteria[5] = popcount(outputDesc->mFlags & performanceFlags);

        // format match
        if (format != AUDIO_FORMAT_INVALID) {
            currentMatchCriteria[6] =
                PolicyAudioPort::kFormatDistanceMax -
                PolicyAudioPort::formatDistance(format, outputDesc->getFormat());
        }

        // primary output match
        currentMatchCriteria[7] = outputDesc->mFlags & AUDIO_OUTPUT_FLAG_PRIMARY;

        // compare match criteria by priority then value
        if (std::lexicographical_compare(bestMatchCriteria.begin(), bestMatchCriteria.end(),
                currentMatchCriteria.begin(), currentMatchCriteria.end())) {
            bestMatchCriteria = currentMatchCriteria;
            bestOutput = output;

            std::stringstream result;
            std::copy(bestMatchCriteria.begin(), bestMatchCriteria.end(),
                std::ostream_iterator<int>(result, " "));
            ALOGV("%s new bestOutput %d criteria %s",
                __func__, bestOutput, result.str().c_str());
        }
    }

    return bestOutput;
}



////////////////////////////////////////////////////////////////////////
//动态注册接口定义
////////////////////////////////////////////////////////////////
status_t AudioPolicyManager::registerPolicyMixes(const Vector<AudioMix>& mixes)
{
    ALOGV("registerPolicyMixes() %zu mix(es)", mixes.size());
    status_t res = NO_ERROR;
    bool checkOutputs = false;
    sp<HwModule> rSubmixModule;
    // examine each mix's route type
    for (size_t i = 0; i < mixes.size(); i++) {
        AudioMix mix = mixes[i];
        // Only capture of playback is allowed in LOOP_BACK & RENDER mode
        if (is_mix_loopback_render(mix.mRouteFlags) && mix.mMixType != MIX_TYPE_PLAYERS) {
            ALOGE("Unsupported Policy Mix %zu of %zu: "
                  "Only capture of playback is allowed in LOOP_BACK & RENDER mode",
                   i, mixes.size());
            res = INVALID_OPERATION;
            break;
        }
        // LOOP_BACK and LOOP_BACK | RENDER have the same remote submix backend and are handled
        // in the same way.
        if ((mix.mRouteFlags & MIX_ROUTE_FLAG_LOOP_BACK) == MIX_ROUTE_FLAG_LOOP_BACK) {
            ALOGV("registerPolicyMixes() mix %zu of %zu is LOOP_BACK %d", i, mixes.size(),
                  mix.mRouteFlags);
            if (rSubmixModule == 0) {
                rSubmixModule = mHwModules.getModuleFromName(
                        AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX);
                if (rSubmixModule == 0) {
                    ALOGE("Unable to find audio module for submix, aborting mix %zu registration",
                            i);
                    res = INVALID_OPERATION;
                    break;
                }
            }

            String8 address = mix.mDeviceAddress;
            audio_devices_t deviceTypeToMakeAvailable;
            if (mix.mMixType == MIX_TYPE_PLAYERS) {
                mix.mDeviceType = AUDIO_DEVICE_OUT_REMOTE_SUBMIX;
                deviceTypeToMakeAvailable = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
            } else {
                mix.mDeviceType = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
                deviceTypeToMakeAvailable = AUDIO_DEVICE_OUT_REMOTE_SUBMIX;
            }

            if (mPolicyMixes.registerMix(mix, 0 /*output desc*/) != NO_ERROR) {
                ALOGE("Error registering mix %zu for address %s", i, address.string());
                res = INVALID_OPERATION;
                break;
            }
            audio_config_t outputConfig = mix.mFormat;
            audio_config_t inputConfig = mix.mFormat;
            // NOTE: audio flinger mixer does not support mono output: configure remote submix HAL
            // in stereo and let audio flinger do the channel conversion if needed.
            outputConfig.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            inputConfig.channel_mask = AUDIO_CHANNEL_IN_STEREO;
            rSubmixModule->addOutputProfile(address.c_str(), &outputConfig,
                    AUDIO_DEVICE_OUT_REMOTE_SUBMIX, address);
            rSubmixModule->addInputProfile(address.c_str(), &inputConfig,
                    AUDIO_DEVICE_IN_REMOTE_SUBMIX, address);

            if ((res = setDeviceConnectionStateInt(deviceTypeToMakeAvailable,
                    AUDIO_POLICY_DEVICE_STATE_AVAILABLE,
                    address.string(), "remote-submix", AUDIO_FORMAT_DEFAULT)) != NO_ERROR) {
                ALOGE("Failed to set remote submix device available, type %u, address %s",
                        mix.mDeviceType, address.string());
                break;
            }
        } else if ((mix.mRouteFlags & MIX_ROUTE_FLAG_RENDER) == MIX_ROUTE_FLAG_RENDER) {
            String8 address = mix.mDeviceAddress;
            audio_devices_t type = mix.mDeviceType;
            ALOGV(" registerPolicyMixes() mix %zu of %zu is RENDER, dev=0x%X addr=%s",
                    i, mixes.size(), type, address.string());

            sp<DeviceDescriptor> device = mHwModules.getDeviceDescriptor(
                    mix.mDeviceType, mix.mDeviceAddress,
                    String8(), AUDIO_FORMAT_DEFAULT);
            if (device == nullptr) {
                res = INVALID_OPERATION;
                break;
            }

            bool foundOutput = false;
            // First try to find an already opened output supporting the device
            for (size_t j = 0 ; j < mOutputs.size() && !foundOutput && res == NO_ERROR; j++) {
                sp<SwAudioOutputDescriptor> desc = mOutputs.valueAt(j);
                if (!desc->isDuplicated() && desc->supportedDevices().contains(device)) {
                    //  AudioPolicyMixCollection mPolicyMixes; // list of registered mixes
                    if (mPolicyMixes.registerMix(mix, desc) != NO_ERROR) {
                        ALOGE("Could not register mix RENDER,  dev=0x%X addr=%s", type,
                              address.string());
                        res = INVALID_OPERATION;
                    } else {
                        foundOutput = true;
                    }
                }
            }
            // If no output found, try to find a direct output profile supporting the device
            for (size_t i = 0; i < mHwModules.size() && !foundOutput && res == NO_ERROR; i++) {
                sp<HwModule> module = mHwModules[i];
                for (size_t j = 0;
                        j < module->getOutputProfiles().size() && !foundOutput && res == NO_ERROR;
                        j++) {
                    sp<IOProfile> profile = module->getOutputProfiles()[j];
                    if (profile->isDirectOutput() && profile->supportsDevice(device)) {
                        //  AudioPolicyMixCollection mPolicyMixes; // list of registered mixes
                        if (mPolicyMixes.registerMix(mix, nullptr) != NO_ERROR) {
                            ALOGE("Could not register mix RENDER,  dev=0x%X addr=%s", type,
                                  address.string());
                            res = INVALID_OPERATION;
                        } else {
                            foundOutput = true;
                        }
                    }
                }
            }
            if (res != NO_ERROR) {
                ALOGE(" Error registering mix %zu for device 0x%X addr %s",
                        i, type, address.string());
                res = INVALID_OPERATION;
                break;
            } else if (!foundOutput) {
                ALOGE(" Output not found for mix %zu for device 0x%X addr %s",
                        i, type, address.string());
                res = INVALID_OPERATION;
                break;
            } else {
                checkOutputs = true;
            }
        }
    }
    if (res != NO_ERROR) {
        unregisterPolicyMixes(mixes);
    } else if (checkOutputs) {
        checkForDeviceAndOutputChanges();
        updateCallAndOutputRouting();
    }
    return res;
}

status_t AudioPolicyManager::unregisterPolicyMixes(Vector<AudioMix> mixes)
{
    ALOGV("unregisterPolicyMixes() num mixes %zu", mixes.size());
    status_t res = NO_ERROR;
    bool checkOutputs = false;
    sp<HwModule> rSubmixModule;
    // examine each mix's route type
    for (const auto& mix : mixes) {
        if ((mix.mRouteFlags & MIX_ROUTE_FLAG_LOOP_BACK) == MIX_ROUTE_FLAG_LOOP_BACK) {

            if (rSubmixModule == 0) {
                rSubmixModule = mHwModules.getModuleFromName(
                        AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX);
                if (rSubmixModule == 0) {
                    res = INVALID_OPERATION;
                    continue;
                }
            }

            String8 address = mix.mDeviceAddress;

            if (mPolicyMixes.unregisterMix(mix) != NO_ERROR) {
                res = INVALID_OPERATION;
                continue;
            }

            for (auto device : {AUDIO_DEVICE_IN_REMOTE_SUBMIX, AUDIO_DEVICE_OUT_REMOTE_SUBMIX}) {
                if (getDeviceConnectionState(device, address.string()) ==
                        AUDIO_POLICY_DEVICE_STATE_AVAILABLE)  {
                    res = setDeviceConnectionStateInt(device, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
                                                      address.string(), "remote-submix",
                                                      AUDIO_FORMAT_DEFAULT);
                    if (res != OK) {
                        ALOGE("Error making RemoteSubmix device unavailable for mix "
                              "with type %d, address %s", device, address.string());
                    }
                }
            }
            rSubmixModule->removeOutputProfile(address.c_str());
            rSubmixModule->removeInputProfile(address.c_str());

        } else if ((mix.mRouteFlags & MIX_ROUTE_FLAG_RENDER) == MIX_ROUTE_FLAG_RENDER) {
            if (mPolicyMixes.unregisterMix(mix) != NO_ERROR) {
                res = INVALID_OPERATION;
                continue;
            } else {
                checkOutputs = true;
            }
        }
    }
    if (res == NO_ERROR && checkOutputs) {
        checkForDeviceAndOutputChanges();
        updateCallAndOutputRouting();
    }
    return res;
}







//////////////////////////////////////////////////////////////
status_t AudioPolicyManager::setPreferredDeviceForStrategy(product_strategy_t strategy,
                                                   const AudioDeviceTypeAddr &device) {
    ALOGV("%s() strategy=%d device=%08x addr=%s", __FUNCTION__,
            strategy, device.mType, device.mAddress.c_str());

    Vector<AudioDeviceTypeAddr> devices;
    devices.add(device);
    if (!areAllDevicesSupported(devices, audio_is_output_device, __func__)) {
        return BAD_VALUE;
    }
    status_t status = mEngine->setPreferredDeviceForStrategy(strategy, device);
    if (status != NO_ERROR) {
        ALOGW("Engine could not set preferred device %08x %s for strategy %d",
                device.mType, device.mAddress.c_str(), strategy);
        return status;
    }

    checkForDeviceAndOutputChanges();
    updateCallAndOutputRouting();

    return NO_ERROR;
}


void AudioPolicyManager::checkForDeviceAndOutputChanges(std::function<bool()> onOutputsChecked)
{
    ALOGV("%s start",__func__);
    // checkA2dpSuspend must run before checkOutputForAllStrategies so that A2DP
    // output is suspended before any tracks are moved to it
    checkA2dpSuspend();
    checkOutputForAllStrategies();
    checkSecondaryOutputs();
    if (onOutputsChecked != nullptr && onOutputsChecked()) checkA2dpSuspend();
    updateDevicesAndOutputs();
    if (mHwModules.getModuleFromName(AUDIO_HARDWARE_MODULE_ID_MSD) != 0) {
        setMsdPatch();
    }

    ALOGV("%s end",__func__);
}



status_t AudioPolicyManager::removePreferredDeviceForStrategy(product_strategy_t strategy)
{
    ALOGI("%s() strategy=%d", __FUNCTION__, strategy);

    status_t status = mEngine->removePreferredDeviceForStrategy(strategy);
    if (status != NO_ERROR) {
        ALOGW("Engine could not remove preferred device for strategy %d", strategy);
        return status;
    }

    checkForDeviceAndOutputChanges();
    updateCallAndOutputRouting();

    return NO_ERROR;
}


//////////////////////////////////////////////////////////////