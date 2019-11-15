public final class UsbAlsaManager {

    /* package */ UsbAlsaManager(Context context) {
        mContext = context;
        /***
         * public static final String FEATURE_MIDI = "android.software.midi";
         */
        mHasMidiFeature = context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI);
    }

    public void systemReady() {
        mAudioService = IAudioService.Stub.asInterface(ServiceManager.getService(Context.AUDIO_SERVICE));
    }



    /**
     * UsbHostManager.usbDeviceAdded
     * --> mUsbAlsaManager.usbDeviceAdded(deviceAddress, newDevice, parser);
     */
    /* package */ void usbDeviceAdded(String deviceAddress, UsbDevice usbDevice,UsbDescriptorParser parser) {
        if (DEBUG) {
            Slog.d(TAG, "usbDeviceAdded(): " + usbDevice.getManufacturerName()  + " nm:" + usbDevice.getProductName());
        }

        // Scan the Alsa File Space
        mCardsParser.scan();

        // Find the ALSA spec for this device address
        AlsaCardsParser.AlsaCardRecord cardRec = mCardsParser.findCardNumFor(deviceAddress);
        if (cardRec == null) {
            return;
        }

        // Add it to the devices list
        boolean hasInput = parser.hasInput();
        boolean hasOutput = parser.hasOutput();
        if (DEBUG) {
            Slog.d(TAG, "hasInput: " + hasInput + " hasOutput:" + hasOutput);
        }
        if (hasInput || hasOutput) {
            boolean isInputHeadset = parser.isInputHeadset();
            boolean isOutputHeadset = parser.isOutputHeadset();

            if (mAudioService == null) {
                Slog.e(TAG, "no AudioService");
                return;
            }

            UsbAlsaDevice alsaDevice = new UsbAlsaDevice(mAudioService, cardRec.getCardNum(), 0 /*device*/, deviceAddress, hasOutput, hasInput,isInputHeadset, isOutputHeadset);
            if (alsaDevice != null) {
                alsaDevice.setDeviceNameAndDescription(cardRec.getCardName(), cardRec.getCardDescription());
                mAlsaDevices.add(0, alsaDevice);
                selectAlsaDevice(alsaDevice);
            }
        }

        // look for MIDI devices
        boolean hasMidi = parser.hasMIDIInterface();
        if (DEBUG) {
            Slog.d(TAG, "hasMidi: " + hasMidi + " mHasMidiFeature:" + mHasMidiFeature);
        }
        if (hasMidi && mHasMidiFeature) {
            int device = 0;
            Bundle properties = new Bundle();
            String manufacturer = usbDevice.getManufacturerName();
            String product = usbDevice.getProductName();
            String version = usbDevice.getVersion();
            String name;
            if (manufacturer == null || manufacturer.isEmpty()) {
                name = product;
            } else if (product == null || product.isEmpty()) {
                name = manufacturer;
            } else {
                name = manufacturer + " " + product;
            }
            properties.putString(MidiDeviceInfo.PROPERTY_NAME, name);
            properties.putString(MidiDeviceInfo.PROPERTY_MANUFACTURER, manufacturer);
            properties.putString(MidiDeviceInfo.PROPERTY_PRODUCT, product);
            properties.putString(MidiDeviceInfo.PROPERTY_VERSION, version);
            properties.putString(MidiDeviceInfo.PROPERTY_SERIAL_NUMBER,usbDevice.getSerialNumber());
            properties.putInt(MidiDeviceInfo.PROPERTY_ALSA_CARD, cardRec.getCardNum());
            properties.putInt(MidiDeviceInfo.PROPERTY_ALSA_DEVICE, 0 /*deviceNum*/);
            properties.putParcelable(MidiDeviceInfo.PROPERTY_USB_DEVICE, usbDevice);

            UsbMidiDevice usbMidiDevice = UsbMidiDevice.create(mContext, properties,cardRec.getCardNum(), 0 /*device*/);
            if (usbMidiDevice != null) {
                mMidiDevices.put(deviceAddress, usbMidiDevice);
            }
        }

        if (DEBUG) {
            Slog.d(TAG, "deviceAdded() - done");
        }
    }

    /**
     * Select the AlsaDevice to be used for AudioService.
     * AlsaDevice.start() notifies AudioService of it's connected state.
     *
     * @param alsaDevice The selected UsbAlsaDevice for system USB audio.
     */
    private synchronized void selectAlsaDevice(UsbAlsaDevice alsaDevice) {
        if (DEBUG) {
            Slog.d(TAG, "selectAlsaDevice " + alsaDevice);
        }

        if (mSelectedDevice != null) {
            deselectAlsaDevice();
        }

        // FIXME Does not yet handle the case where the setting is changed
        // after device connection.  Ideally we should handle the settings change
        // in SettingsObserver. Here we should log that a USB device is connected
        // and disconnected with its address (card , device) and force the
        // connection or disconnection when the setting changes.
        int isDisabled = Settings.Secure.getInt(mContext.getContentResolver(),Settings.Secure.USB_AUDIO_AUTOMATIC_ROUTING_DISABLED, 0);
        if (isDisabled != 0) {
            return;
        }

        mSelectedDevice = alsaDevice;
        alsaDevice.start();
    }

    private synchronized void deselectAlsaDevice() {
        if (mSelectedDevice != null) {
            mSelectedDevice.stop();
            mSelectedDevice = null;
        }
    }


}