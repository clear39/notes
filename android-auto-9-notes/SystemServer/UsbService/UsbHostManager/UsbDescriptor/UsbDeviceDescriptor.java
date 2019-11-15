

//  @   frameworks/base/services/usb/java/com/android/server/usb/descriptors/UsbDeviceDescriptor.java
public final class UsbDeviceDescriptor extends UsbDescriptor {

    UsbDeviceDescriptor(int length, byte type) {
        super(length, type);
        /**用于标志usb信息层级 */
        mHierarchyLevel = 1;
    }

    @Override
    public int parseRawDescriptors(ByteStream stream) {
        mSpec = stream.unpackUsbShort();
        mDevClass = stream.getUnsignedByte();
        mDevSubClass = stream.getUnsignedByte();
        mProtocol = stream.getUnsignedByte();
        mPacketSize = stream.getByte();
        mVendorID = stream.unpackUsbShort();
        mProductID = stream.unpackUsbShort();
        mDeviceRelease = stream.unpackUsbShort();
        mMfgIndex = stream.getByte();
        mProductIndex = stream.getByte();
        mSerialIndex = stream.getByte();
        mNumConfigs = stream.getByte();

        return mLength;
    }

    void addConfigDescriptor(UsbConfigDescriptor config) {
        /***
         *  private ArrayList<UsbConfigDescriptor> mConfigDescriptors = new ArrayList<UsbConfigDescriptor>();
         */
        mConfigDescriptors.add(config);
    }


    /**
     * @hide
     */
    public UsbDevice toAndroid(UsbDescriptorParser parser) {
        if (DEBUG) {
            Log.d(TAG, "toAndroid()");
        }

        String mfgName = getMfgString(parser);
        String prodName = getProductString(parser);
        if (DEBUG) {
            Log.d(TAG, "  mfgName:" + mfgName + " prodName:" + prodName);
        }

        String versionString = getDeviceReleaseString();
        String serialStr = getSerialString(parser);
        if (DEBUG) {
            Log.d(TAG, "  versionString:" + versionString + " serialStr:" + serialStr);
        }

        UsbDevice device = new UsbDevice(parser.getDeviceAddr(), mVendorID, mProductID,
                mDevClass, mDevSubClass,
                mProtocol, mfgName, prodName,
                versionString, serialStr);
        UsbConfiguration[] configs = new UsbConfiguration[mConfigDescriptors.size()];
        Log.d(TAG, "  " + configs.length + " configs");
        for (int index = 0; index < mConfigDescriptors.size(); index++) {
            configs[index] = mConfigDescriptors.get(index).toAndroid(parser);
        }
        device.setConfigurations(configs);

        return device;
    }

}