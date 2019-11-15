public abstract class UsbACInterface extends UsbDescriptor {
    /**
     * Allocates an audio class interface subtype based on subtype and subclass.
     */
    public static UsbDescriptor allocDescriptor(UsbDescriptorParser parser, ByteStream stream,int length, byte type) {
        byte subtype = stream.getByte();
        UsbInterfaceDescriptor interfaceDesc = parser.getCurInterface();
        int subClass = interfaceDesc.getUsbSubclass();
        switch (subClass) {
            case AUDIO_AUDIOCONTROL:
                return allocAudioControlDescriptor(parser, stream, length, type, subtype, subClass);

            case AUDIO_AUDIOSTREAMING:
                return allocAudioStreamingDescriptor(parser, stream, length, type, subtype, subClass);

            case AUDIO_MIDISTREAMING:
                return allocMidiStreamingDescriptor(length, type, subtype, subClass);

            default:
                Log.w(TAG, "Unknown Audio Class Interface Subclass: 0x" + Integer.toHexString(subClass));
                return null;
        }
    }


    private static UsbDescriptor allocAudioControlDescriptor(UsbDescriptorParser parser,ByteStream stream, int length, byte type, byte subtype, int subClass) {
        switch (subtype) {
            case ACI_HEADER:
            {
                int acInterfaceSpec = stream.unpackUsbShort();
                parser.setACInterfaceSpec(acInterfaceSpec);
                if (acInterfaceSpec == UsbDeviceDescriptor.USBSPEC_2_0) {
                    return new Usb20ACHeader(length, type, subtype, subClass, acInterfaceSpec);
                } else {
                    return new Usb10ACHeader(length, type, subtype, subClass, acInterfaceSpec);
                }
            }

            case ACI_INPUT_TERMINAL:
            {
                int acInterfaceSpec = parser.getACInterfaceSpec();
                if (acInterfaceSpec == UsbDeviceDescriptor.USBSPEC_2_0) {
                    return new Usb20ACInputTerminal(length, type, subtype, subClass);
                } else {
                    return new Usb10ACInputTerminal(length, type, subtype, subClass);
                }
            }

            case ACI_OUTPUT_TERMINAL:
            {
                int acInterfaceSpec = parser.getACInterfaceSpec();
                if (acInterfaceSpec == UsbDeviceDescriptor.USBSPEC_2_0) {
                    return new Usb20ACOutputTerminal(length, type, subtype, subClass);
                } else {
                    return new Usb10ACOutputTerminal(length, type, subtype, subClass);
                }
            }

            case ACI_SELECTOR_UNIT:
                return new UsbACSelectorUnit(length, type, subtype, subClass);

            case ACI_FEATURE_UNIT:
                return new UsbACFeatureUnit(length, type, subtype, subClass);

            case ACI_MIXER_UNIT:
            {
                int acInterfaceSpec = parser.getACInterfaceSpec();
                if (acInterfaceSpec == UsbDeviceDescriptor.USBSPEC_2_0) {
                    return new Usb20ACMixerUnit(length, type, subtype, subClass);
                } else {
                    return new Usb10ACMixerUnit(length, type, subtype, subClass);
                }
            }

            case ACI_PROCESSING_UNIT:
            case ACI_EXTENSION_UNIT:
            case ACI_UNDEFINED:
                // break; Fall through until we implement this descriptor
            default:
                Log.w(TAG, "Unknown Audio Class Interface subtype:0x" + Integer.toHexString(subtype));
                return new UsbACInterfaceUnparsed(length, type, subtype, subClass);
        }
    }

    private static UsbDescriptor allocAudioStreamingDescriptor(UsbDescriptorParser parser,ByteStream stream, int length, byte type, byte subtype, int subClass) {
        //int spec = parser.getUsbSpec();
        int acInterfaceSpec = parser.getACInterfaceSpec();
        switch (subtype) {
            case ASI_GENERAL:
                if (acInterfaceSpec == UsbDeviceDescriptor.USBSPEC_2_0) {
                    return new Usb20ASGeneral(length, type, subtype, subClass);
                } else {
                    return new Usb10ASGeneral(length, type, subtype, subClass);
                }

            case ASI_FORMAT_TYPE:
                return UsbASFormat.allocDescriptor(parser, stream, length, type, subtype, subClass);

            case ASI_FORMAT_SPECIFIC:
            case ASI_UNDEFINED:
                // break; Fall through until we implement this descriptor
            default:
                Log.w(TAG, "Unknown Audio Streaming Interface subtype:0x"+ Integer.toHexString(subtype));
                return null;
        }
    }

    private static UsbDescriptor allocMidiStreamingDescriptor(int length, byte type,byte subtype, int subClass) {
        switch (subtype) {
            case MSI_HEADER:
                return new UsbMSMidiHeader(length, type, subtype, subClass);

            case MSI_IN_JACK:
                return new UsbMSMidiInputJack(length, type, subtype, subClass);

            case MSI_OUT_JACK:
                return new UsbMSMidiOutputJack(length, type, subtype, subClass);

            case MSI_ELEMENT:
                // break;
                // Fall through until we implement that descriptor

            case MSI_UNDEFINED:
            default:
                Log.w(TAG, "Unknown MIDI Streaming Interface subtype:0x" + Integer.toHexString(subtype));
                return null;
        }
    }
}