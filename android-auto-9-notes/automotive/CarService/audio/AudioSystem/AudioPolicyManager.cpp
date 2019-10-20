

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
            for (const auto& dev : mAvailableOutputDevices) {
                if (dev->type() == AUDIO_DEVICE_OUT_STUB) {
                    continue;
                }
                if (portsWritten < portsMax) {
                    dev->toAudioPort(&ports[portsWritten++]);
                }
                (*num_ports)++;
            }
        }
        if (role == AUDIO_PORT_ROLE_SOURCE || role == AUDIO_PORT_ROLE_NONE) {
            for (const auto& dev : mAvailableInputDevices) {
                if (dev->type() == AUDIO_DEVICE_IN_STUB) {
                    continue;
                }
                if (portsWritten < portsMax) {
                    dev->toAudioPort(&ports[portsWritten++]);
                }
                (*num_ports)++;
            }
        }
    }

    
    if (type == AUDIO_PORT_TYPE_NONE || type == AUDIO_PORT_TYPE_MIX) {
        if (role == AUDIO_PORT_ROLE_SINK || role == AUDIO_PORT_ROLE_NONE) {
            for (size_t i = 0; i < mInputs.size() && portsWritten < portsMax; i++) {
                mInputs[i]->toAudioPort(&ports[portsWritten++]);
            }
            *num_ports += mInputs.size();
        }
        if (role == AUDIO_PORT_ROLE_SOURCE || role == AUDIO_PORT_ROLE_NONE) {
            size_t numOutputs = 0;
            for (size_t i = 0; i < mOutputs.size(); i++) {
                if (!mOutputs[i]->isDuplicated()) {
                    numOutputs++;
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