

//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/services/core/java/com/android/server/input/InputManagerService.java


public class InputManagerService extends IInputManager.Stub implements Watchdog.Monitor {




    /**
     * 有native层调用
     * 
     * 
     */
    // Native callback.
    private void notifyInputDevicesChanged(InputDevice[] inputDevices) {
        synchronized (mInputDevicesLock) {
            if (!mInputDevicesChangedPending) {
                mInputDevicesChangedPending = true;
                mHandler.obtainMessage(MSG_DELIVER_INPUT_DEVICES_CHANGED,mInputDevices).sendToTarget();
            }

            mInputDevices = inputDevices;
        }
    }


    /**
     * Gets all input devices in the system.
     * @return The array of input devices.
     */
    public InputDevice[] getInputDevices() {
        synchronized (mInputDevicesLock) {
            return mInputDevices;
        }
    } 
}