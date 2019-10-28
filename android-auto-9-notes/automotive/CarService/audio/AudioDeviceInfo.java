

//  @   frameworks/base/media/java/android/media/AudioDeviceInfo.java


public final class AudioDeviceInfo {

    AudioDeviceInfo(AudioDevicePort port) {
        mPort = port;
     }
    
    /****
     * mPort.type() 对应这  devicePort 的 type 属性值 
     * system/media/audio/include/system/audio-base.h:319:    AUDIO_DEVICE_OUT_BUS                       = 0x1000000u,
     */
    public int getType() {
        return INT_TO_EXT_DEVICE_MAPPING.get(mPort.type(), TYPE_UNKNOWN);
    }
}

