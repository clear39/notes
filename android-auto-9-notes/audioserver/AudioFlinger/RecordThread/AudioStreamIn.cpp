

//  @   frameworks/av/services/audioflinger/AudioFlinger.h


struct AudioStreamIn {
    AudioHwDevice* const audioHwDev;
    sp<StreamInHalInterface> stream;
    audio_input_flags_t flags;

    sp<DeviceHalInterface> hwDev() const { return audioHwDev->hwDevice(); }

    AudioStreamIn(AudioHwDevice *dev, sp<StreamInHalInterface> in, audio_input_flags_t flags) :
        audioHwDev(dev), stream(in), flags(flags) {}
};