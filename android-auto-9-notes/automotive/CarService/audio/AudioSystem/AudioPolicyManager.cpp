

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
    *generation = curAudioPortGeneration();
    /**
     * 
     * 
    */
    return mAudioPatches.listAudioPatches(num_patches, patches);
}