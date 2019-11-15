//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbHostManager.java
/*
    * ConnectionRecord
    * Stores connection/disconnection data.
    */
class ConnectionRecord {
    ConnectionRecord(String deviceAddress, int mode, byte[] descriptors) {
        mTimestamp = System.currentTimeMillis();
        mDeviceAddress = deviceAddress;
        mMode = mode;
        mDescriptors = descriptors;
    }
}