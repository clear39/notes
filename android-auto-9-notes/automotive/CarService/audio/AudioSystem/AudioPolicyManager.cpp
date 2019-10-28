

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp
status_t AudioPolicyManager::listAudioPorts(audio_port_role_t role,
                                            audio_port_type_t type,
                                            unsigned int *num_ports,
                                            struct audio_port *ports,
                                            unsigned int *generation)
{
    /***
     * 
     */
    if (num_ports == NULL || (*num_ports != 0 && ports == NULL) || generation == NULL) {
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
            /***
             * mAvailableOutputDevices 为  DeviceDescriptor 集合 通过 attachedDevices 标签属性配置
             */
            for (const auto& dev : mAvailableOutputDevices) {
                /**
                 * 
                 * dev->type() 对应 DeviceDescriptor（devicePort） 的 type 属性值
                */
                if (dev->type() == AUDIO_DEVICE_OUT_STUB) {
                    continue;
                }
                /**
                 * 注意这里的技巧 如果传入 *num_ports 为 0，以下代码不执行，只是用来统计个数
                */
                if (portsWritten < portsMax) {
                    /**
                     * frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPort.cpp:58:
                     * void AudioPort::toAudioPort(struct audio_port *port) const
                     * 
                     * 将 AudioPort 属性值 赋值 给  audio_port （将 AudioPort 转换成 audio_port）
                    */
                    dev->toAudioPort(&ports[portsWritten++]);
                }
                (*num_ports)++;
            }
        }
        if (role == AUDIO_PORT_ROLE_SOURCE || role == AUDIO_PORT_ROLE_NONE) {
            /***
             * mAvailableOutputDevices 为  DeviceDescriptor（devicePort） 集合 通过 attachedDevices 标签属性配置
             */
            for (const auto& dev : mAvailableInputDevices) {
                /**
                 * 
                 * dev->type() 对应 DeviceDescriptor（devicePort） 的 type 属性值
                */
                if (dev->type() == AUDIO_DEVICE_IN_STUB) {
                    continue;
                }
                /**
                 * 注意这里的技巧 如果传入 *num_ports 为 0，以下代码不执行，只是用来统计个数
                */
                if (portsWritten < portsMax) {
                    /**
                     * frameworks/av/services/audiopolicy/common/managerdefinitions/src/AudioPort.cpp:58:
                     * void AudioPort::toAudioPort(struct audio_port *port) const
                     * 
                     * 将 AudioPort 属性值 赋值 给  audio_port （将 AudioPort 转换成 audio_port）
                    */
                    dev->toAudioPort(&ports[portsWritten++]);
                }
                (*num_ports)++;
            }
        }
    }

    
    if (type == AUDIO_PORT_TYPE_NONE || type == AUDIO_PORT_TYPE_MIX) {
        if (role == AUDIO_PORT_ROLE_SINK || role == AUDIO_PORT_ROLE_NONE) {
            /**
             * 注意这里的 portsWritten < portsMax 技巧 如果传入 *num_ports 为 0，以下代码不执行，只是用来统计个数
             * 
             * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioInputDescriptor.h
             * class AudioInputCollection :public DefaultKeyedVector< audio_io_handle_t, sp<AudioInputDescriptor> >
             * 注意 audio_io_handle_t 实在AudioFlinger中创建，用于标记对应所创建的线程
             * 
             * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioInputDescriptor.h:34:
             * class AudioInputDescriptor: public AudioPortConfig, public AudioSessionInfoProvider
             * 
             * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPort.h:153:
             * class AudioPortConfig : public virtual RefBase
             * 
             * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioSessionInfoProvider.h:25:
             * class AudioSessionInfoProvider
             * 
             * 
             * 
             * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:4219:    
             * mInputs.add(input, inputDesc);
             * 
             * 
             * 
            */
            for (size_t i = 0; i < mInputs.size() && portsWritten < portsMax; i++) {
                /**
                 * void AudioInputDescriptor::toAudioPort(struct audio_port *port) const
                */
                mInputs[i]->toAudioPort(&ports[portsWritten++]);
            }
            *num_ports += mInputs.size();
        }
        if (role == AUDIO_PORT_ROLE_SOURCE || role == AUDIO_PORT_ROLE_NONE) {
            size_t numOutputs = 0;
            for (size_t i = 0; i < mOutputs.size(); i++) {
                if (!mOutputs[i]->isDuplicated()) {
                    numOutputs++;
                    /**
                     * 注意这里的 portsWritten < portsMax 技巧 如果传入 *num_ports 为 0，以下代码不执行，只是用来统计个数
                     * 
                     * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:552:
                     * SwAudioOutputCollection mOutputs;
                     * 
                     * frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioOutputDescriptor.h
                     * class SwAudioOutputCollection : public DefaultKeyedVector< audio_io_handle_t, sp<SwAudioOutputDescriptor> >
                     * 注意 audio_io_handle_t 实在AudioFlinger中创建，用于标记对应所创建的线程
                     * 
                     * 
                     * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:4203:    
                     * mOutputs.add(output, outputDesc);
                     * 
                     * 
    frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:1025:        addOutput(output, outputDesc);
    frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:4063:                addOutput(output, outputDesc);
    frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:4352:                    addOutput(output, desc);
    frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.cpp:4377:                            addOutput(duplicatedOutput, dupOutputDesc);

                    */
                    if (portsWritten < portsMax) {
                        mOutputs[i]->toAudioPort(&ports[portsWritten++]);
                    }
                }
            }
            *num_ports += numOutputs;
        }
    }

    /***
     * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:645:   
     * uint32_t curAudioPortGeneration() const { return mAudioPortGeneration; }
     * */
    *generation = curAudioPortGeneration();
    ALOGV("listAudioPorts() got %zu ports needed %d", portsWritten, *num_ports);
    return NO_ERROR;
}



status_t AudioPolicyManager::listAudioPatches(unsigned int *num_patches,struct audio_patch *patches,unsigned int *generation)
{
    if (generation == NULL) {
        return BAD_VALUE;
    }
    /***
     * frameworks/av/services/audiopolicy/managerdefault/AudioPolicyManager.h:645:   
     * uint32_t curAudioPortGeneration() const { return mAudioPortGeneration; }
     * */
    *generation = curAudioPortGeneration();
    /**
     * 
     * 
    */
    return mAudioPatches.listAudioPatches(num_patches, patches);
}






////////////////////////////////////////////////////////////////////////////////////////////////////
// Register a list of custom mixes with their attributes and format.
// When a mix is registered, corresponding input and output profiles are
// added to the remote submix hw module. The profile contains only the
// parameters (sampling rate, format...) specified by the mix.
// The corresponding input remote submix device is also connected.
//
// When a remote submix device is connected, the address is checked to select the
// appropriate profile and the corresponding input or output stream is opened.
//
// When capture starts, getInputForAttr() will:
//  - 1 look for a mix matching the address passed in attribtutes tags if any
//  - 2 if none found, getDeviceForInputSource() will:
//     - 2.1 look for a mix matching the attributes source
//     - 2.2 if none found, default to device selection by policy rules
// At this time, the corresponding output remote submix device is also connected
// and active playback use cases can be transferred to this mix if needed when reconnecting
// after AudioTracks are invalidated
//
// When playback starts, getOutputForAttr() will:
//  - 1 look for a mix matching the address passed in attribtutes tags if any
//  - 2 if none found, look for a mix matching the attributes usage
//  - 3 if none found, default to device and output selection by policy rules.

/***
 * 
 * CarAudioService.init()
 * --> AudioManager.registerAudioPolicy()
 * ---> AudioService.registerAudioPolicy new AudioService$AudioPolicyProxy
 * ----> AudioPolicyProxy.connectMixes
 * ----> AudioSystem.registerPolicyMixes
 * -----> AudioPolicyService::registerPolicyMixes
*/



status_t AudioPolicyManager::registerPolicyMixes(const Vector<AudioMix>& mixes)
{
    ALOGV("registerPolicyMixes() %zu mix(es)", mixes.size());
    status_t res = NO_ERROR;

    sp<HwModule> rSubmixModule;
    // examine each mix's route type
    for (size_t i = 0; i < mixes.size(); i++) {
        // we only support MIX_ROUTE_FLAG_LOOP_BACK or MIX_ROUTE_FLAG_RENDER, not the combination
        if ((mixes[i].mRouteFlags & MIX_ROUTE_FLAG_ALL) == MIX_ROUTE_FLAG_ALL) {
            res = INVALID_OPERATION;
            break;
        }
        if ((mixes[i].mRouteFlags & MIX_ROUTE_FLAG_LOOP_BACK) == MIX_ROUTE_FLAG_LOOP_BACK) {
            ALOGV("registerPolicyMixes() mix %zu of %zu is LOOP_BACK", i, mixes.size());
            if (rSubmixModule == 0) {
                rSubmixModule = mHwModules.getModuleFromName(AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX);
                if (rSubmixModule == 0) {
                    ALOGE(" Unable to find audio module for submix, aborting mix %zu registration", i);
                    res = INVALID_OPERATION;
                    break;
                }
            }

            /**
             * address="bus0_media_out"
            */
            String8 address = mixes[i].mDeviceAddress;

            if (mPolicyMixes.registerMix(address, mixes[i], 0 /*output desc*/) != NO_ERROR) {
                ALOGE(" Error registering mix %zu for address %s", i, address.string());
                res = INVALID_OPERATION;
                break;
            }
            audio_config_t outputConfig = mixes[i].mFormat;
            audio_config_t inputConfig = mixes[i].mFormat;
            // NOTE: audio flinger mixer does not support mono output: configure remote submix HAL in
            // stereo and let audio flinger do the channel conversion if needed.
            outputConfig.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            inputConfig.channel_mask = AUDIO_CHANNEL_IN_STEREO;
            rSubmixModule->addOutputProfile(address, &outputConfig,AUDIO_DEVICE_OUT_REMOTE_SUBMIX, address);
            rSubmixModule->addInputProfile(address, &inputConfig,AUDIO_DEVICE_IN_REMOTE_SUBMIX, address);

            if (mixes[i].mMixType == MIX_TYPE_PLAYERS) {
                setDeviceConnectionStateInt(AUDIO_DEVICE_IN_REMOTE_SUBMIX,AUDIO_POLICY_DEVICE_STATE_AVAILABLE,address.string(), "remote-submix");
            } else {
                setDeviceConnectionStateInt(AUDIO_DEVICE_OUT_REMOTE_SUBMIX,AUDIO_POLICY_DEVICE_STATE_AVAILABLE,address.string(), "remote-submix");
            }
        } else if ((mixes[i].mRouteFlags & MIX_ROUTE_FLAG_RENDER) == MIX_ROUTE_FLAG_RENDER) {
            String8 address = mixes[i].mDeviceAddress;
            audio_devices_t device = mixes[i].mDeviceType;
            ALOGV(" registerPolicyMixes() mix %zu of %zu is RENDER, dev=0x%X addr=%s", i, mixes.size(), device, address.string());
            ALOGV(" registerPolicyMixes() mOutputs.size()=%zu",mOutputs.size());
            bool foundOutput = false;
            for (size_t j = 0 ; j < mOutputs.size() ; j++) {
                sp<SwAudioOutputDescriptor> desc = mOutputs.valueAt(j);
                sp<AudioPatch> patch = mAudioPatches.valueFor(desc->getPatchHandle());
                if ((patch != 0) && (patch->mPatch.num_sinks != 0)
                        && (patch->mPatch.sinks[0].type == AUDIO_PORT_TYPE_DEVICE)
                        && (patch->mPatch.sinks[0].ext.device.type == device)
                        && (strncmp(patch->mPatch.sinks[0].ext.device.address, address.string(),AUDIO_DEVICE_MAX_ADDRESS_LEN) == 0)) {
                    if (mPolicyMixes.registerMix(address, mixes[i], desc) != NO_ERROR) {
                        res = INVALID_OPERATION;
                    } else {
                        foundOutput = true;
                    }
                    break;
                }
            }

            if (res != NO_ERROR) {
                ALOGE(" Error registering mix %zu for device 0x%X addr %s",i, device, address.string());
                res = INVALID_OPERATION;
                break;
            } else if (!foundOutput) {
                ALOGE(" Output not found for mix %zu for device 0x%X addr %s",i, device, address.string());
                res = INVALID_OPERATION;
                break;
            }
        }
    }
    if (res != NO_ERROR) {
        unregisterPolicyMixes(mixes);
    }
    return res;
}