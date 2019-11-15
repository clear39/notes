//  @   frameworks/base/services/usb/java/com/android/server/usb/descriptors/UsbHIDDescriptor.java

public final class UsbHIDDescriptor extends UsbDescriptor {
    public UsbHIDDescriptor(int length, byte type) {
        super(length, type);
        /**
         * 用于标志usb信息层级 
         * 和 UsbInterfaceDescriptor 一个层级
         * */
        mHierarchyLevel = 3;
    }
}