/**
 * @    frameworks/base/services/usb/java/com/android/server/usb/descriptors/UsbConfigDescriptor.java
 */


public final class UsbConfigDescriptor extends UsbDescriptor {
    UsbConfigDescriptor(int length, byte type) {
        super(length, type);
        /**用于标志usb信息层级 
         * 表示该类是添加到usb信息层级为1（UsbDeviceDescriptor）中
        */
        mHierarchyLevel = 2;
    }
}