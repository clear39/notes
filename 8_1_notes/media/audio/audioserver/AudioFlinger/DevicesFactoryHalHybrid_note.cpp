class DevicesFactoryHalHybrid : public DevicesFactoryHalInterface
{}

//  @frameworks/av/media/libaudiohal/DevicesFactoryHalHybrid.cpp

// static
sp<DevicesFactoryHalInterface> DevicesFactoryHalInterface::create() {
    return new DevicesFactoryHalHybrid();
}





DevicesFactoryHalHybrid::DevicesFactoryHalHybrid()
        : mLocalFactory(new DevicesFactoryHalLocal()),
          mHidlFactory(
#ifdef USE_LEGACY_LOCAL_AUDIO_HAL		//这个宏没有定义
                  nullptr
#else
                  new DevicesFactoryHalHidl()
#endif
                       ) {
}




status_t DevicesFactoryHalHybrid::openDevice(const char *name, sp<DeviceHalInterface> *device) {

  /**
  本平台 只有a2dp是本地加载，其他的通过hal service 和 hw通讯
  */
	//	@system/media/audio/include/system/audio.h:980:#define AUDIO_HARDWARE_MODULE_ID_A2DP "a2dp"
    if (mHidlFactory != 0 && strcmp(AUDIO_HARDWARE_MODULE_ID_A2DP, name) != 0) {
        return mHidlFactory->openDevice(name, device);
    }
    return mLocalFactory->openDevice(name, device);//	AUDIO_HARDWARE_MODULE_ID_A2DP
}
