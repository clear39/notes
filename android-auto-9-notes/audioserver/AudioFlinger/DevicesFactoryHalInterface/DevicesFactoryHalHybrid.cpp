

//  @   frameworks/av/media/libaudiohal/4.0/DevicesFactoryHalHybrid.cpp

DevicesFactoryHalHybrid::DevicesFactoryHalHybrid()
        : mLocalFactory(new DevicesFactoryHalLocal()),
          mHidlFactory(new DevicesFactoryHalHidl()) {
}

DevicesFactoryHalHybrid::~DevicesFactoryHalHybrid() {
}
/*
#define AUDIO_HARDWARE_MODULE_ID_PRIMARY "primary"
#define AUDIO_HARDWARE_MODULE_ID_A2DP "a2dp"
#define AUDIO_HARDWARE_MODULE_ID_USB "usb"
#define AUDIO_HARDWARE_MODULE_ID_REMOTE_SUBMIX "r_submix"
#define AUDIO_HARDWARE_MODULE_ID_CODEC_OFFLOAD "codec_offload"
#define AUDIO_HARDWARE_MODULE_ID_STUB "stub"
#define AUDIO_HARDWARE_MODULE_ID_HEARING_AID "hearing_aid" 
*/
status_t DevicesFactoryHalHybrid::openDevice(const char *name, sp<DeviceHalInterface> *device) {
    /**
     * @    system/media/audio/include/system/audio.h:1124:#define AUDIO_HARDWARE_MODULE_ID_A2DP "a2dp"
     * @    system/media/audio/include/system/audio.h:1129:#define AUDIO_HARDWARE_MODULE_ID_HEARING_AID "hearing_aid"
     * 
     * 这里只有 a2dp 和 hearing_aid 使用 mLocalFactory（属于本地加载） 加载
     * mHidlFactory 通过hidl远程加载
    */
    if (mHidlFactory != 0 && strcmp(AUDIO_HARDWARE_MODULE_ID_A2DP, name) != 0 &&
        strcmp(AUDIO_HARDWARE_MODULE_ID_HEARING_AID, name) != 0) {
        return mHidlFactory->openDevice(name, device);
    }
    return mLocalFactory->openDevice(name, device);
}
