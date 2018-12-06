//	@frameworks/av/services/audiopolicy/common/managerdefinitions/include/AudioPolicyConfig.h
class AudioPolicyConfig
{
	public:
    AudioPolicyConfig(HwModuleCollection &hwModules,
                      DeviceVector &availableOutputDevices,
                      DeviceVector &availableInputDevices,
                      sp<DeviceDescriptor> &defaultOutputDevices,
                      bool &isSpeakerDrcEnabled,
                      VolumeCurvesCollection *volumes = nullptr)
        : mHwModules(hwModules),
          mAvailableOutputDevices(availableOutputDevices),
          mAvailableInputDevices(availableInputDevices),
          mDefaultOutputDevices(defaultOutputDevices),
          mVolumeCurves(volumes),
          mIsSpeakerDrcEnabled(isSpeakerDrcEnabled)
    {}

    void setVolumes(const VolumeCurvesCollection &volumes)
    {
        if (mVolumeCurves != nullptr) {
            *mVolumeCurves = volumes;
        }
    }

    void setHwModules(const HwModuleCollection &hwModules)
    {
        mHwModules = hwModules;
    }

    void addAvailableDevice(const sp<DeviceDescriptor> &availableDevice)
    {
        if (audio_is_output_device(availableDevice->type())) {//    system/media/audio/include/system/audio.h:527
            mAvailableOutputDevices.add(availableDevice);
        } else if (audio_is_input_device(availableDevice->type())) {
            mAvailableInputDevices.add(availableDevice);
        }
    }

    void addAvailableInputDevices(const DeviceVector &availableInputDevices)
    {
        mAvailableInputDevices.add(availableInputDevices);
    }

    void addAvailableOutputDevices(const DeviceVector &availableOutputDevices)
    {
        mAvailableOutputDevices.add(availableOutputDevices);
    }

    void setSpeakerDrcEnabled(bool isSpeakerDrcEnabled)
    {
        mIsSpeakerDrcEnabled = isSpeakerDrcEnabled;//true
    }

    const HwModuleCollection getHwModules() const { return mHwModules; }

    const DeviceVector &getAvailableInputDevices() const
    {
        return mAvailableInputDevices;
    }

    const DeviceVector &getAvailableOutputDevices() const
    {
        return mAvailableOutputDevices;
    }

    void setDefaultOutputDevice(const sp<DeviceDescriptor> &defaultDevice)
    {
        mDefaultOutputDevices = defaultDevice;
    }

    const sp<DeviceDescriptor> &getDefaultOutputDevice() const { return mDefaultOutputDevices; }

    void setDefault(void)
    {
        mDefaultOutputDevices = new DeviceDescriptor(AUDIO_DEVICE_OUT_SPEAKER);
        sp<HwModule> module;
        sp<DeviceDescriptor> defaultInputDevice = new DeviceDescriptor(AUDIO_DEVICE_IN_BUILTIN_MIC);
        mAvailableOutputDevices.add(mDefaultOutputDevices);
        mAvailableInputDevices.add(defaultInputDevice);

        module = new HwModule("primary");

        sp<OutputProfile> outProfile;
        outProfile = new OutputProfile(String8("primary"));
        outProfile->attach(module);
        outProfile->addAudioProfile(
                new AudioProfile(AUDIO_FORMAT_PCM_16_BIT, AUDIO_CHANNEL_OUT_STEREO, 44100));
        outProfile->addSupportedDevice(mDefaultOutputDevices);
        outProfile->setFlags(AUDIO_OUTPUT_FLAG_PRIMARY);
        module->mOutputProfiles.add(outProfile);

        sp<InputProfile> inProfile;
        inProfile = new InputProfile(String8("primary"));
        inProfile->attach(module);
        inProfile->addAudioProfile(
                new AudioProfile(AUDIO_FORMAT_PCM_16_BIT, AUDIO_CHANNEL_IN_MONO, 8000));
        inProfile->addSupportedDevice(defaultInputDevice);
        module->mInputProfiles.add(inProfile);

        mHwModules.add(module);
    }

private:
    HwModuleCollection &mHwModules; /**< Collection of Module, with Profiles, i.e. Mix Ports. */
    DeviceVector &mAvailableOutputDevices;
    DeviceVector &mAvailableInputDevices;
    sp<DeviceDescriptor> &mDefaultOutputDevices;
    VolumeCurvesCollection *mVolumeCurves;
    bool &mIsSpeakerDrcEnabled;
}


//  system/media/audio/include/system/audio-base.h
enum {
    AUDIO_DEVICE_NONE = 0u, // 0x0
    AUDIO_DEVICE_BIT_IN = 2147483648u, // 0x80000000
    AUDIO_DEVICE_BIT_DEFAULT = 1073741824u, // 0x40000000
    AUDIO_DEVICE_OUT_EARPIECE = 1u, // 0x1
    AUDIO_DEVICE_OUT_SPEAKER = 2u, // 0x2
    AUDIO_DEVICE_OUT_WIRED_HEADSET = 4u, // 0x4
    AUDIO_DEVICE_OUT_WIRED_HEADPHONE = 8u, // 0x8
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO = 16u, // 0x10
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET = 32u, // 0x20
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT = 64u, // 0x40
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP = 128u, // 0x80
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES = 256u, // 0x100
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER = 512u, // 0x200
    AUDIO_DEVICE_OUT_AUX_DIGITAL = 1024u, // 0x400
    AUDIO_DEVICE_OUT_HDMI = 1024u, // OUT_AUX_DIGITAL
    AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET = 2048u, // 0x800
    AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET = 4096u, // 0x1000
    AUDIO_DEVICE_OUT_USB_ACCESSORY = 8192u, // 0x2000
    AUDIO_DEVICE_OUT_USB_DEVICE = 16384u, // 0x4000
    AUDIO_DEVICE_OUT_REMOTE_SUBMIX = 32768u, // 0x8000
    AUDIO_DEVICE_OUT_TELEPHONY_TX = 65536u, // 0x10000
    AUDIO_DEVICE_OUT_LINE = 131072u, // 0x20000
    AUDIO_DEVICE_OUT_HDMI_ARC = 262144u, // 0x40000
    AUDIO_DEVICE_OUT_SPDIF = 524288u, // 0x80000
    AUDIO_DEVICE_OUT_FM = 1048576u, // 0x100000
    AUDIO_DEVICE_OUT_AUX_LINE = 2097152u, // 0x200000
    AUDIO_DEVICE_OUT_SPEAKER_SAFE = 4194304u, // 0x400000
    AUDIO_DEVICE_OUT_IP = 8388608u, // 0x800000
    AUDIO_DEVICE_OUT_BUS = 16777216u, // 0x1000000
    AUDIO_DEVICE_OUT_PROXY = 33554432u, // 0x2000000
    AUDIO_DEVICE_OUT_USB_HEADSET = 67108864u, // 0x4000000
    AUDIO_DEVICE_OUT_DEFAULT = 1073741824u, // BIT_DEFAULT
    AUDIO_DEVICE_OUT_ALL = 1207959551u, // (((((((((((((((((((((((((((OUT_EARPIECE | OUT_SPEAKER) | OUT_WIRED_HEADSET) | OUT_WIRED_HEADPHONE) | OUT_BLUETOOTH_SCO) | OUT_BLUETOOTH_SCO_HEADSET) | OUT_BLUETOOTH_SCO_CARKIT) | OUT_BLUETOOTH_A2DP) | OUT_BLUETOOTH_A2DP_HEADPHONES) | OUT_BLUETOOTH_A2DP_SPEAKER) | OUT_HDMI) | OUT_ANLG_DOCK_HEADSET) | OUT_DGTL_DOCK_HEADSET) | OUT_USB_ACCESSORY) | OUT_USB_DEVICE) | OUT_REMOTE_SUBMIX) | OUT_TELEPHONY_TX) | OUT_LINE) | OUT_HDMI_ARC) | OUT_SPDIF) | OUT_FM) | OUT_AUX_LINE) | OUT_SPEAKER_SAFE) | OUT_IP) | OUT_BUS) | OUT_PROXY) | OUT_USB_HEADSET) | OUT_DEFAULT)
    AUDIO_DEVICE_OUT_ALL_A2DP = 896u, // ((OUT_BLUETOOTH_A2DP | OUT_BLUETOOTH_A2DP_HEADPHONES) | OUT_BLUETOOTH_A2DP_SPEAKER)
    AUDIO_DEVICE_OUT_ALL_SCO = 112u, // ((OUT_BLUETOOTH_SCO | OUT_BLUETOOTH_SCO_HEADSET) | OUT_BLUETOOTH_SCO_CARKIT)
    AUDIO_DEVICE_OUT_ALL_USB = 67133440u, // ((OUT_USB_ACCESSORY | OUT_USB_DEVICE) | OUT_USB_HEADSET)
    AUDIO_DEVICE_IN_COMMUNICATION = 2147483649u, // (BIT_IN | 0x1)
    AUDIO_DEVICE_IN_AMBIENT = 2147483650u, // (BIT_IN | 0x2)
    AUDIO_DEVICE_IN_BUILTIN_MIC = 2147483652u, // (BIT_IN | 0x4)
    AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET = 2147483656u, // (BIT_IN | 0x8)
    AUDIO_DEVICE_IN_WIRED_HEADSET = 2147483664u, // (BIT_IN | 0x10)
    AUDIO_DEVICE_IN_AUX_DIGITAL = 2147483680u, // (BIT_IN | 0x20)
    AUDIO_DEVICE_IN_HDMI = 2147483680u, // IN_AUX_DIGITAL
    AUDIO_DEVICE_IN_VOICE_CALL = 2147483712u, // (BIT_IN | 0x40)
    AUDIO_DEVICE_IN_TELEPHONY_RX = 2147483712u, // IN_VOICE_CALL
    AUDIO_DEVICE_IN_BACK_MIC = 2147483776u, // (BIT_IN | 0x80)
    AUDIO_DEVICE_IN_REMOTE_SUBMIX = 2147483904u, // (BIT_IN | 0x100)
    AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET = 2147484160u, // (BIT_IN | 0x200)
    AUDIO_DEVICE_IN_DGTL_DOCK_HEADSET = 2147484672u, // (BIT_IN | 0x400)
    AUDIO_DEVICE_IN_USB_ACCESSORY = 2147485696u, // (BIT_IN | 0x800)
    AUDIO_DEVICE_IN_USB_DEVICE = 2147487744u, // (BIT_IN | 0x1000)
    AUDIO_DEVICE_IN_FM_TUNER = 2147491840u, // (BIT_IN | 0x2000)
    AUDIO_DEVICE_IN_TV_TUNER = 2147500032u, // (BIT_IN | 0x4000)
    AUDIO_DEVICE_IN_LINE = 2147516416u, // (BIT_IN | 0x8000)
    AUDIO_DEVICE_IN_SPDIF = 2147549184u, // (BIT_IN | 0x10000)
    AUDIO_DEVICE_IN_BLUETOOTH_A2DP = 2147614720u, // (BIT_IN | 0x20000)
    AUDIO_DEVICE_IN_LOOPBACK = 2147745792u, // (BIT_IN | 0x40000)
    AUDIO_DEVICE_IN_IP = 2148007936u, // (BIT_IN | 0x80000)
    AUDIO_DEVICE_IN_BUS = 2148532224u, // (BIT_IN | 0x100000)
    AUDIO_DEVICE_IN_PROXY = 2164260864u, // (BIT_IN | 0x1000000)
    AUDIO_DEVICE_IN_USB_HEADSET = 2181038080u, // (BIT_IN | 0x2000000)
    AUDIO_DEVICE_IN_DEFAULT = 3221225472u, // (BIT_IN | BIT_DEFAULT)
    AUDIO_DEVICE_IN_ALL = 3273654271u, // (((((((((((((((((((((((IN_COMMUNICATION | IN_AMBIENT) | IN_BUILTIN_MIC) | IN_BLUETOOTH_SCO_HEADSET) | IN_WIRED_HEADSET) | IN_HDMI) | IN_TELEPHONY_RX) | IN_BACK_MIC) | IN_REMOTE_SUBMIX) | IN_ANLG_DOCK_HEADSET) | IN_DGTL_DOCK_HEADSET) | IN_USB_ACCESSORY) | IN_USB_DEVICE) | IN_FM_TUNER) | IN_TV_TUNER) | IN_LINE) | IN_SPDIF) | IN_BLUETOOTH_A2DP) | IN_LOOPBACK) | IN_IP) | IN_BUS) | IN_PROXY) | IN_USB_HEADSET) | IN_DEFAULT)
    AUDIO_DEVICE_IN_ALL_SCO = 2147483656u, // IN_BLUETOOTH_SCO_HEADSET
    AUDIO_DEVICE_IN_ALL_USB = 2181044224u, // ((IN_USB_ACCESSORY | IN_USB_DEVICE) | IN_USB_HEADSET)
};

//  system/media/audio/include/system/audio.h
static inline bool audio_is_output_device(audio_devices_t device)
{
    if (((device & AUDIO_DEVICE_BIT_IN) == 0) &&
            (popcount(device) == 1) && ((device & ~AUDIO_DEVICE_OUT_ALL) == 0))
        return true;
    else
        return false;
}

static inline bool audio_is_input_device(audio_devices_t device)
{
    if ((device & AUDIO_DEVICE_BIT_IN) != 0) {
        device &= ~AUDIO_DEVICE_BIT_IN;
        if ((popcount(device) == 1) && ((device & ~AUDIO_DEVICE_IN_ALL) == 0))
            return true;
    }
    return false;
}


//  system/core/libcutils/include/cutils/bitops.h
static inline int popcount(unsigned int x) {
    return __builtin_popcount(x);// _builtin_popcount()计算二进制中多少个1
}

