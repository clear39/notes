//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPolicyConfig.h
AudioPolicyConfig::AudioPolicyConfig(HwModuleCollection &hwModules,
                    DeviceVector &availableOutputDevices,
                    DeviceVector &availableInputDevices,
                    sp<DeviceDescriptor> &defaultOutputDevices,
                    VolumeCurvesCollection *volumes = nullptr)
    : mHwModules(hwModules),
        mAvailableOutputDevices(availableOutputDevices),
        mAvailableInputDevices(availableInputDevices),
        mDefaultOutputDevices(defaultOutputDevices),
        mVolumeCurves(volumes),
        mIsSpeakerDrcEnabled(false)
{}

/*
    <attachedDevices>
        <item>bus0_media_out</item>
        <item>bus1_system_sound_out</item>
        <item>Built-In Mic</item>
    </attachedDevices>
*/
void addAvailableDevice(const sp<DeviceDescriptor> &availableDevice)
{
    if (audio_is_output_device(availableDevice->type())) {
        mAvailableOutputDevices.add(availableDevice);
    } else if (audio_is_input_device(availableDevice->type())) {
        mAvailableInputDevices.add(availableDevice);
    }
}

/*
   <defaultOutputDevice>bus0_media_out</defaultOutputDevice>
*/
void setDefaultOutputDevice(const sp<DeviceDescriptor> &defaultDevice)
{
    mDefaultOutputDevices = defaultDevice;
}

/*
 <globalConfiguration speaker_drc_enabled="true"/>
*/
void setSpeakerDrcEnabled(bool isSpeakerDrcEnabled)
{
    mIsSpeakerDrcEnabled = isSpeakerDrcEnabled;
}

void setHwModules(const HwModuleCollection &hwModules)
{
    mHwModules = hwModules;
}


void setVolumes(const VolumeCurvesCollection &volumes)
{
    if (mVolumeCurves != nullptr) {
        *mVolumeCurves = volumes;
    }
}