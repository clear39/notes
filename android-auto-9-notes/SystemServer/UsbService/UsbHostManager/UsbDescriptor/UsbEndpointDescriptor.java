

public class UsbEndpointDescriptor extends UsbDescriptor {
    public UsbEndpointDescriptor(int length, byte type) {
        super(length, type);
        /**
         * 用于标志usb信息层级 
         * 表示该类是添加到usb信息层级为3（UsbInterfaceDescriptor）中
         * */
        mHierarchyLevel = 4;
    }
}