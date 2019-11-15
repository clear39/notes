//  @   frameworks/base/services/usb/java/com/android/server/usb/descriptors/UsbDescriptor.java

public abstract class UsbDescriptor implements Reporting {
     /**
     * @throws IllegalArgumentException
     */
    UsbDescriptor(int length, byte type) {
        // a descriptor has at least a length byte and type byte
        // one could imagine an empty one otherwise
        if (length < 2) {
            // huh?
            throw new IllegalArgumentException();
        }

        mLength = length;
        mType = type;
    }
}