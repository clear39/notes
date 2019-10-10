


//  @   hardware/interfaces/audio/core/all-versions/default/include/core/all-versions/default/PrimaryDevice.h
struct PrimaryDevice : public IPrimaryDevice {}

//  @   hardware/interfaces/audio/core/all-versions/default/include/core/all-versions/default/PrimaryDevice.impl.h
PrimaryDevice::PrimaryDevice(audio_hw_device_t* device) : mDevice(new Device(device)) {

}