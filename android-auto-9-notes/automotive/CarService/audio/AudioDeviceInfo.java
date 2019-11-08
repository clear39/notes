

//  @   frameworks/base/media/java/android/media/AudioDeviceInfo.java


public final class AudioDeviceInfo {


    static {
        INT_TO_EXT_DEVICE_MAPPING = new SparseIntArray();
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_EARPIECE, TYPE_BUILTIN_EARPIECE);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_SPEAKER, TYPE_BUILTIN_SPEAKER);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_WIRED_HEADSET, TYPE_WIRED_HEADSET);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_WIRED_HEADPHONE, TYPE_WIRED_HEADPHONES);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BLUETOOTH_SCO, TYPE_BLUETOOTH_SCO);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BLUETOOTH_SCO_HEADSET, TYPE_BLUETOOTH_SCO);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BLUETOOTH_SCO_CARKIT, TYPE_BLUETOOTH_SCO);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BLUETOOTH_A2DP, TYPE_BLUETOOTH_A2DP);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES, TYPE_BLUETOOTH_A2DP);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER, TYPE_BLUETOOTH_A2DP);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_HDMI, TYPE_HDMI);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_ANLG_DOCK_HEADSET, TYPE_DOCK);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_DGTL_DOCK_HEADSET, TYPE_DOCK);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_USB_ACCESSORY, TYPE_USB_ACCESSORY);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_USB_DEVICE, TYPE_USB_DEVICE);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_USB_HEADSET, TYPE_USB_HEADSET);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_TELEPHONY_TX, TYPE_TELEPHONY);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_LINE, TYPE_LINE_ANALOG);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_HDMI_ARC, TYPE_HDMI_ARC);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_SPDIF, TYPE_LINE_DIGITAL);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_FM, TYPE_FM);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_AUX_LINE, TYPE_AUX_LINE);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_IP, TYPE_IP);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_BUS, TYPE_BUS);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_OUT_HEARING_AID, TYPE_HEARING_AID);

        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_BUILTIN_MIC, TYPE_BUILTIN_MIC);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_BLUETOOTH_SCO_HEADSET, TYPE_BLUETOOTH_SCO);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_WIRED_HEADSET, TYPE_WIRED_HEADSET);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_HDMI, TYPE_HDMI);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_TELEPHONY_RX, TYPE_TELEPHONY);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_BACK_MIC, TYPE_BUILTIN_MIC);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_ANLG_DOCK_HEADSET, TYPE_DOCK);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_DGTL_DOCK_HEADSET, TYPE_DOCK);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_USB_ACCESSORY, TYPE_USB_ACCESSORY);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_USB_DEVICE, TYPE_USB_DEVICE);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_USB_HEADSET, TYPE_USB_HEADSET);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_FM_TUNER, TYPE_FM_TUNER);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_TV_TUNER, TYPE_TV_TUNER);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_LINE, TYPE_LINE_ANALOG);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_SPDIF, TYPE_LINE_DIGITAL);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_BLUETOOTH_A2DP, TYPE_BLUETOOTH_A2DP);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_IP, TYPE_IP);
        INT_TO_EXT_DEVICE_MAPPING.put(AudioSystem.DEVICE_IN_BUS, TYPE_BUS);

        // not covered here, legacy
        //AudioSystem.DEVICE_OUT_REMOTE_SUBMIX
        //AudioSystem.DEVICE_IN_REMOTE_SUBMIX

        // privileges mapping to output device
        EXT_TO_INT_DEVICE_MAPPING = new SparseIntArray();
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_BUILTIN_EARPIECE, AudioSystem.DEVICE_OUT_EARPIECE);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_BUILTIN_SPEAKER, AudioSystem.DEVICE_OUT_SPEAKER);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_WIRED_HEADSET, AudioSystem.DEVICE_OUT_WIRED_HEADSET);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_WIRED_HEADPHONES, AudioSystem.DEVICE_OUT_WIRED_HEADPHONE);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_LINE_ANALOG, AudioSystem.DEVICE_OUT_LINE);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_LINE_DIGITAL, AudioSystem.DEVICE_OUT_SPDIF);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_BLUETOOTH_SCO, AudioSystem.DEVICE_OUT_BLUETOOTH_SCO);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_BLUETOOTH_A2DP, AudioSystem.DEVICE_OUT_BLUETOOTH_A2DP);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_HDMI, AudioSystem.DEVICE_OUT_HDMI);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_HDMI_ARC, AudioSystem.DEVICE_OUT_HDMI_ARC);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_USB_DEVICE, AudioSystem.DEVICE_OUT_USB_DEVICE);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_USB_HEADSET, AudioSystem.DEVICE_OUT_USB_HEADSET);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_USB_ACCESSORY, AudioSystem.DEVICE_OUT_USB_ACCESSORY);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_DOCK, AudioSystem.DEVICE_OUT_ANLG_DOCK_HEADSET);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_FM, AudioSystem.DEVICE_OUT_FM);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_BUILTIN_MIC, AudioSystem.DEVICE_IN_BUILTIN_MIC);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_FM_TUNER, AudioSystem.DEVICE_IN_FM_TUNER);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_TV_TUNER, AudioSystem.DEVICE_IN_TV_TUNER);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_TELEPHONY, AudioSystem.DEVICE_OUT_TELEPHONY_TX);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_AUX_LINE, AudioSystem.DEVICE_OUT_AUX_LINE);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_IP, AudioSystem.DEVICE_OUT_IP);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_BUS, AudioSystem.DEVICE_OUT_BUS);
        EXT_TO_INT_DEVICE_MAPPING.put(TYPE_HEARING_AID, AudioSystem.DEVICE_OUT_HEARING_AID);
    }


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

