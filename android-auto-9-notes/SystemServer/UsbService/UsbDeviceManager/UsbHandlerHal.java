


private static final class UsbHandlerLegacy extends UsbHandler {

    UsbHandlerLegacy(Looper looper, Context context, UsbDeviceManager deviceManager,
                UsbDebuggingManager debuggingManager, UsbAlsaManager alsaManager,
                UsbSettingsManager settingsManager) {
        super(looper, context, deviceManager, debuggingManager, alsaManager, settingsManager);
        try {
            readOemUsbOverrideConfig(context);
            // Restore default functions.
            mCurrentOemFunctions = getSystemProperty(getPersistProp(false),UsbManager.USB_FUNCTION_NONE);
            if (isNormalBoot()) {
                mCurrentFunctionsStr = getSystemProperty(USB_CONFIG_PROPERTY,UsbManager.USB_FUNCTION_NONE);
                mCurrentFunctionsApplied = mCurrentFunctionsStr.equals(getSystemProperty(USB_STATE_PROPERTY, UsbManager.USB_FUNCTION_NONE));
            } else {
                mCurrentFunctionsStr = getSystemProperty(getPersistProp(true),UsbManager.USB_FUNCTION_NONE);
                mCurrentFunctionsApplied = getSystemProperty(USB_CONFIG_PROPERTY,UsbManager.USB_FUNCTION_NONE).equals(getSystemProperty(USB_STATE_PROPERTY, UsbManager.USB_FUNCTION_NONE));
            }
            mCurrentFunctions = UsbManager.FUNCTION_NONE;
            mCurrentUsbFunctionsReceived = true;
            /**
             * private static final String STATE_PATH = "/sys/class/android_usb/android0/state";
             */
            String state = FileUtils.readTextFile(new File(STATE_PATH), 0, null).trim();
            updateState(state);
        } catch (Exception e) {
            Slog.e(TAG, "Error initializing UsbHandler", e);
        }
    }


}