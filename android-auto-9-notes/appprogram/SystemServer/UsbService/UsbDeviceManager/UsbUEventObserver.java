//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbDeviceManager.java
private final class UsbUEventObserver extends UEventObserver {
    @Override
    public void onUEvent(UEventObserver.UEvent event) {
        if (DEBUG) Slog.v(TAG, "USB UEVENT: " + event.toString());

        String state = event.get("USB_STATE");
        String accessory = event.get("ACCESSORY");
        if (state != null) {
            mHandler.updateState(state);
        } else if ("START".equals(accessory)) {
            if (DEBUG) Slog.d(TAG, "got accessory start");
            startAccessoryMode();
        }
    }
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/core/java/android/os/UEventObserver.java
public abstract class UEventObserver {

    public final void startObserving(String match) {
        if (match == null || match.isEmpty()) {
            throw new IllegalArgumentException("match substring must be non-empty");
        }

        final UEventThread t = getThread();
        t.addObserver(match, this);
    }

}