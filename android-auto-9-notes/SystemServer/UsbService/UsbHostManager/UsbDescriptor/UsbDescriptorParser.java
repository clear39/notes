//  @   frameworks/base/services/usb/java/com/android/server/usb/descriptors/UsbDescriptorParser.java
public final class UsbDescriptorParser {
    /**
     * Connect this parser to an byte array containing unparsed (raw) device descriptors
     * to be parsed (and parse them). Useful for parsing a stored descriptor buffer.
     */
    public UsbDescriptorParser(String deviceAddr, byte[] rawDescriptors) {
        mDeviceAddr = deviceAddr;
        /**
         * private static final int DESCRIPTORS_ALLOC_SIZE = 128;
         */
        mDescriptors = new ArrayList<UsbDescriptor>(DESCRIPTORS_ALLOC_SIZE);
        parseDescriptors(rawDescriptors);
    }


    /**
     * @hide
     */
    public void parseDescriptors(byte[] descriptors) {
        if (DEBUG) {
            Log.d(TAG, "parseDescriptors() - start");
        }

        ByteStream stream = new ByteStream(descriptors);
        while (stream.available() > 0) {
            UsbDescriptor descriptor = null;
            try {
                descriptor = allocDescriptor(stream);
            } catch (Exception ex) {
                Log.e(TAG, "Exception allocating USB descriptor.", ex);
            }

            if (descriptor != null) {
                // Parse
                try {
                    descriptor.parseRawDescriptors(stream);

                    // Clean up
                    descriptor.postParse(stream);
                } catch (Exception ex) {
                    Log.e(TAG, "Exception parsing USB descriptors.", ex);

                    // Clean up
                    descriptor.setStatus(UsbDescriptor.STATUS_PARSE_EXCEPTION);
                } finally {
                    /**
                     * allocDescriptor 中 UsbDescriptor.DESCRIPTORTYPE_DEVICE
                     */
                    mDescriptors.add(descriptor);
                }
            }
        }
        if (DEBUG) {
            Log.d(TAG, "parseDescriptors() - end " + mDescriptors.size() + " descriptors.");
        }
    }


    private UsbDescriptor allocDescriptor(ByteStream stream) throws UsbDescriptorsStreamFormatException {
        stream.resetReadCount();

        /**
         * 读取一个无符号字节
         */
        int length = stream.getUnsignedByte();
        /**
         * 读取一个字节
         */
        byte type = stream.getByte();

        UsbDescriptor descriptor = null;
        switch (type) {
            /*
             * Standard
             */
            case UsbDescriptor.DESCRIPTORTYPE_DEVICE://设备描述符
                descriptor = mDeviceDescriptor = new UsbDeviceDescriptor(length, type);
                break;

            case UsbDescriptor.DESCRIPTORTYPE_CONFIG://配置描述符
                descriptor = mCurConfigDescriptor = new UsbConfigDescriptor(length, type);
                if (mDeviceDescriptor != null) {
                    /***
                     * 添加配置描述符
                     */
                    mDeviceDescriptor.addConfigDescriptor(mCurConfigDescriptor);
                } else {
                    Log.e(TAG, "Config Descriptor found with no associated Device Descriptor!");
                    throw new UsbDescriptorsStreamFormatException("Config Descriptor found with no associated Device Descriptor!");
                }
                break;

            case UsbDescriptor.DESCRIPTORTYPE_INTERFACE://接口描述符
                /**
                 * 注意这里设置了当前接口描述符 mCurInterfaceDescriptor
                 */
                descriptor = mCurInterfaceDescriptor = new UsbInterfaceDescriptor(length, type);
                if (mCurConfigDescriptor != null) {
                    mCurConfigDescriptor.addInterfaceDescriptor(mCurInterfaceDescriptor);
                } else {
                    Log.e(TAG, "Interface Descriptor found with no associated Config Descriptor!");
                    throw new UsbDescriptorsStreamFormatException("Interface Descriptor found with no associated Config Descriptor!");
                }
                break;
            
            case UsbDescriptor.DESCRIPTORTYPE_ENDPOINT:// 端点描述符
                descriptor = new UsbEndpointDescriptor(length, type);
                if (mCurInterfaceDescriptor != null) {
                    /**
                     * 将端点描述符添加到当前接口描述符表中
                     */
                    mCurInterfaceDescriptor.addEndpointDescriptor((UsbEndpointDescriptor) descriptor);
                } else {
                    Log.e(TAG,"Endpoint Descriptor found with no associated Interface Descriptor!");
                    throw new UsbDescriptorsStreamFormatException("Endpoint Descriptor found with no associated Interface Descriptor!");
                }
                break;

            /*
             * HID
             */
            case UsbDescriptor.DESCRIPTORTYPE_HID:
                /**
                 * 和 UsbInterfaceDescriptor 一个层级
                 */
                descriptor = new UsbHIDDescriptor(length, type);
                break;

            /*
             * Other
             */
            case UsbDescriptor.DESCRIPTORTYPE_INTERFACEASSOC:
                descriptor = new UsbInterfaceAssoc(length, type);
                break;

            /*
             * Audio Class Specific
             */
            case UsbDescriptor.DESCRIPTORTYPE_AUDIO_INTERFACE:
                descriptor = UsbACInterface.allocDescriptor(this, stream, length, type);
                break;

            case UsbDescriptor.DESCRIPTORTYPE_AUDIO_ENDPOINT:
                descriptor = UsbACEndpoint.allocDescriptor(this, length, type);
                break;

            default:
                break;
        }

        if (descriptor == null) {
            // Unknown Descriptor
            Log.i(TAG, "Unknown Descriptor len: " + length + " type:0x" + Integer.toHexString(type));
            descriptor = new UsbUnknown(length, type);
        }

        return descriptor;
    }


      /**
     * @hide
     */
    public UsbDevice toAndroidUsbDevice() {
        if (mDeviceDescriptor == null) {
            Log.e(TAG, "toAndroidUsbDevice() ERROR - No Device Descriptor");
            return null;
        }

        UsbDevice device = mDeviceDescriptor.toAndroid(this);
        if (device == null) {
            Log.e(TAG, "toAndroidUsbDevice() ERROR Creating Device");
        }
        return device;
    }

}