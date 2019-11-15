

abstract class UsbACEndpoint extends UsbDescriptor {

    public static UsbDescriptor allocDescriptor(UsbDescriptorParser parser,int length, byte type) {
        
        UsbInterfaceDescriptor interfaceDesc = parser.getCurInterface();
        int subClass = interfaceDesc.getUsbSubclass();

        switch (subClass) {
            case AUDIO_AUDIOCONTROL:
                return new UsbACAudioControlEndpoint(length, type, subClass);

            case AUDIO_AUDIOSTREAMING:
                return new UsbACAudioStreamEndpoint(length, type, subClass);

            case AUDIO_MIDISTREAMING:
                return new UsbACMidiEndpoint(length, type, subClass);

            default:
                Log.w(TAG, "Unknown Audio Class Endpoint id:0x" + Integer.toHexString(subClass));
                return null;
        }
    }
}