//  @   frameworks/base/services/usb/java/com/android/server/usb/descriptors/UsbInterfaceDescriptor.java

public class UsbInterfaceDescriptor extends UsbDescriptor {
    UsbInterfaceDescriptor(int length, byte type) {
        super(length, type);
        /**
         * 用于标志usb信息层级 
         * 表示该类是添加到usb信息层级为2（UsbConfigDescriptor）中
         * */
        mHierarchyLevel = 3;
    }
}