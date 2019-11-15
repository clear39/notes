
/**
 * @    frameworks/base/services/usb/java/com/android/server/usb/UsbAlsaJackDetector.java
 * Detects and reports ALSA jack state and events.
 */
public final class UsbAlsaJackDetector implements Runnable {
    /* use startJackDetect to create a UsbAlsaJackDetector */
    private UsbAlsaJackDetector(UsbAlsaDevice device) {
        mAlsaDevice = device;
    }


    /** If jack detection is detected on the given Alsa Device,
     * create and return a UsbAlsaJackDetector which will update wired device state
     * each time a jack detection event is registered.
     *
     * @returns UsbAlsaJackDetector if jack detect is supported, or null.
     */
    public static UsbAlsaJackDetector startJackDetect(UsbAlsaDevice device) {
        if (!nativeHasJackDetect(device.getCardNum())) {
            return null;
        }
        UsbAlsaJackDetector jackDetector = new UsbAlsaJackDetector(device);

        // This thread will exit once the USB device disappears.
        // It can also be convinced to stop with pleaseStop().
        new Thread(jackDetector, "USB jack detect thread").start();
        return jackDetector;
    }
}