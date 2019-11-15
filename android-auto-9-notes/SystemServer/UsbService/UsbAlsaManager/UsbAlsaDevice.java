//  @   frameworks/base/services/usb/java/com/android/server/usb/UsbAlsaDevice.java
public final class UsbAlsaDevice {

    public UsbAlsaDevice(IAudioService audioService, int card, int device, String deviceAddress,
        boolean hasOutput, boolean hasInput, boolean isInputHeadset, boolean isOutputHeadset) {
        mAudioService = audioService;
        mCardNum = card;
        mDeviceNum = device;
        mDeviceAddress = deviceAddress;
        mHasOutput = hasOutput;
        mHasInput = hasInput;
        mIsInputHeadset = isInputHeadset;
        mIsOutputHeadset = isOutputHeadset;
    }



    /** Start using this device as the selected USB Audio Device. */
    public synchronized void start() {
        mSelected = true;
        mInputState = 0;
        mOutputState = 0;
        startJackDetect();
        updateWiredDeviceConnectionState(true);
    }

    /** Begins a jack-detection thread. */
    private synchronized void startJackDetect() {
        // If no jack detect capabilities exist, mJackDetector will be null.
        mJackDetector = UsbAlsaJackDetector.startJackDetect(this);
    }

    /** Updates AudioService with the connection state of the alsaDevice.
     *  Checks ALSA Jack state for inputs and outputs before reporting.
     */
    public synchronized void updateWiredDeviceConnectionState(boolean enable) {
        if (!mSelected) {
            Slog.e(TAG, "updateWiredDeviceConnectionState on unselected AlsaDevice!");
            return;
        }
        String alsaCardDeviceString = getAlsaCardDeviceString();
        if (alsaCardDeviceString == null) {
            return;
        }
        try {
            // Output Device
            if (mHasOutput) {
                int device = mIsOutputHeadset
                        ? AudioSystem.DEVICE_OUT_USB_HEADSET
                        : AudioSystem.DEVICE_OUT_USB_DEVICE;
                if (DEBUG) {
                    Slog.d(TAG, "pre-call device:0x" + Integer.toHexString(device)
                            + " addr:" + alsaCardDeviceString
                            + " name:" + mDeviceName);
                }
                boolean connected = isOutputJackConnected();
                Slog.i(TAG, "OUTPUT JACK connected: " + connected);
                int outputState = (enable && connected) ? 1 : 0;
                if (outputState != mOutputState) {
                    mOutputState = outputState;
                    mAudioService.setWiredDeviceConnectionState(device, outputState,
                                                                alsaCardDeviceString,
                                                                mDeviceName, TAG);
                }
            }

            // Input Device
            if (mHasInput) {
                int device = mIsInputHeadset ? AudioSystem.DEVICE_IN_USB_HEADSET
                        : AudioSystem.DEVICE_IN_USB_DEVICE;
                boolean connected = isInputJackConnected();
                Slog.i(TAG, "INPUT JACK connected: " + connected);
                int inputState = (enable && connected) ? 1 : 0;
                if (inputState != mInputState) {
                    mInputState = inputState;
                    mAudioService.setWiredDeviceConnectionState(
                            device, inputState, alsaCardDeviceString,
                            mDeviceName, TAG);
                }
            }
        } catch (RemoteException e) {
            Slog.e(TAG, "RemoteException in setWiredDeviceConnectionState");
        }
    }

}