

//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/base/services/core/java/com/android/server/input/InputManagerService.java


public class InputManagerService extends IInputManager.Stub implements Watchdog.Monitor {


    public InputManagerService(Context context) {
        this.mContext = context;
        this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper());
        
        /***
         * 
         */
        mUseDevInputEventForAudioJack = context.getResources().getBoolean(R.bool.config_useDevInputEventForAudioJack);
        Slog.i(TAG, "Initializing input manager, mUseDevInputEventForAudioJack=" + mUseDevInputEventForAudioJack);
         /***
         * nativeInit   //  frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp
         * --> new NativeInputManager
         */
        mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue());

         /***
         * 
         */
        String doubleTouchGestureEnablePath = context.getResources().getString(R.string.config_doubleTouchGestureEnableFile);
        mDoubleTouchGestureEnableFile = TextUtils.isEmpty(doubleTouchGestureEnablePath) ? null : new File(doubleTouchGestureEnablePath);

        LocalServices.addService(InputManagerInternal.class, new LocalService());
    }




















    /**
     * 由native层调用
     * 
     * NativeInputManager::notifyInputDevicesChanged(const Vector<InputDeviceInfo>& inputDevices)
     * --> 
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

    // Must be called on handler.
    private void deliverInputDevicesChanged(InputDevice[] oldInputDevices) {
        // Scan for changes.
        int numFullKeyboardsAdded = 0;
        mTempInputDevicesChangedListenersToNotify.clear();
        mTempFullKeyboards.clear();
        final int numListeners;
        final int[] deviceIdAndGeneration;
        synchronized (mInputDevicesLock) {
            if (!mInputDevicesChangedPending) {
                return;
            }
            mInputDevicesChangedPending = false;

            /***
             * mInputDevicesChangedListeners 提供给想监听输入设备信息变化的客户端
             */
            numListeners = mInputDevicesChangedListeners.size();
            for (int i = 0; i < numListeners; i++) {
                mTempInputDevicesChangedListenersToNotify.add(mInputDevicesChangedListeners.valueAt(i));
            }

            final int numDevices = mInputDevices.length;
            deviceIdAndGeneration = new int[numDevices * 2];
            for (int i = 0; i < numDevices; i++) {
                final InputDevice inputDevice = mInputDevices[i];
                /**
                 * 
                 */
                deviceIdAndGeneration[i * 2] = inputDevice.getId();
                /**
                 * 
                 */
                deviceIdAndGeneration[i * 2 + 1] = inputDevice.getGeneration();
                
                /**
                 * 非虚拟键盘 且 
                 */
                if (!inputDevice.isVirtual() && inputDevice.isFullKeyboard()) {
                    if (!containsInputDeviceWithDescriptor(oldInputDevices,inputDevice.getDescriptor())) {
                        mTempFullKeyboards.add(numFullKeyboardsAdded++, inputDevice);
                    } else {
                        mTempFullKeyboards.add(inputDevice);
                    }
                }
            }
        }

        // Notify listeners.
        for (int i = 0; i < numListeners; i++) {
            mTempInputDevicesChangedListenersToNotify.get(i).notifyInputDevicesChanged(deviceIdAndGeneration);
        }
        mTempInputDevicesChangedListenersToNotify.clear();

        // Check for missing keyboard layouts.
        List<InputDevice> keyboardsMissingLayout = new ArrayList<>();
        final int numFullKeyboards = mTempFullKeyboards.size();
        synchronized (mDataStore) {
            for (int i = 0; i < numFullKeyboards; i++) {
                final InputDevice inputDevice = mTempFullKeyboards.get(i);
                String layout = getCurrentKeyboardLayoutForInputDevice(inputDevice.getIdentifier());
                if (layout == null) {
                    layout = getDefaultKeyboardLayout(inputDevice);
                    if (layout != null) {
                        setCurrentKeyboardLayoutForInputDevice(inputDevice.getIdentifier(), layout);
                    }
                }
                if (layout == null) {
                    keyboardsMissingLayout.add(inputDevice);
                }
            }
        }

        if (mNotificationManager != null) {
            if (!keyboardsMissingLayout.isEmpty()) {
                if (keyboardsMissingLayout.size() > 1) {
                    // We have more than one keyboard missing a layout, so drop the
                    // user at the generic input methods page so they can pick which
                    // one to set.
                    showMissingKeyboardLayoutNotification(null);
                } else {
                    showMissingKeyboardLayoutNotification(keyboardsMissingLayout.get(0));
                }
            } else if (mKeyboardLayoutNotificationShown) {
                hideMissingKeyboardLayoutNotification();
            }
        }
        mTempFullKeyboards.clear();
    }


    @Override // Binder call
    public String getCurrentKeyboardLayoutForInputDevice(InputDeviceIdentifier identifier) {

        /***
         * identifier 中保存 mVendorId 和 mProductId
         * getLayoutDescriptor 中 如果 mVendorId 和 mProductId 多为0，这 返回 InputDeviceIdentifier.mDescriptor
         * 总则返回 vendor:mVendorId，product:mProductId
         */
        String key = getLayoutDescriptor(identifier);
        synchronized (mDataStore) {
            String layout = null;
            // try loading it using the layout descriptor if we have it
            layout = mDataStore.getCurrentKeyboardLayout(key);
            if (layout == null && !key.equals(identifier.getDescriptor())) {
                // if it doesn't exist fall back to the device descriptor
                layout = mDataStore.getCurrentKeyboardLayout(identifier.getDescriptor());
            }
            if (DEBUG) {
                Slog.d(TAG, "Loaded keyboard layout id for " + key + " and got " + layout);
            }
            return layout;
        }
    }

    rivate String getDefaultKeyboardLayout(final InputDevice d) {
        final Locale systemLocale = mContext.getResources().getConfiguration().locale;
        // If our locale doesn't have a language for some reason, then we don't really have a
        // reasonable default.
        if (TextUtils.isEmpty(systemLocale.getLanguage())) {
            return null;
        }
        final List<KeyboardLayout> layouts = new ArrayList<>();
        visitAllKeyboardLayouts(new KeyboardLayoutVisitor() {
            @Override
            public void visitKeyboardLayout(Resources resources,int keyboardLayoutResId, KeyboardLayout layout) {
                // Only select a default when we know the layout is appropriate. For now, this
                // means its a custom layout for a specific keyboard.
                if (layout.getVendorId() != d.getVendorId() || layout.getProductId() != d.getProductId()) {
                    return;
                }
                final LocaleList locales = layout.getLocales();
                final int numLocales = locales.size();
                for (int localeIndex = 0; localeIndex < numLocales; ++localeIndex) {
                    if (isCompatibleLocale(systemLocale, locales.get(localeIndex))) {
                        layouts.add(layout);
                        break;
                    }
                }
            }
        });

        if (layouts.isEmpty()) {
            return null;
        }

        // First sort so that ones with higher priority are listed at the top
        Collections.sort(layouts);
        // Next we want to try to find an exact match of language, country and variant.
        final int N = layouts.size();
        for (int i = 0; i < N; i++) {
            KeyboardLayout layout = layouts.get(i);
            final LocaleList locales = layout.getLocales();
            final int numLocales = locales.size();
            for (int localeIndex = 0; localeIndex < numLocales; ++localeIndex) {
                final Locale locale = locales.get(localeIndex);
                if (locale.getCountry().equals(systemLocale.getCountry()) && locale.getVariant().equals(systemLocale.getVariant())) {
                    return layout.getDescriptor();
                }
            }
        }
        // Then try an exact match of language and country
        for (int i = 0; i < N; i++) {
            KeyboardLayout layout = layouts.get(i);
            final LocaleList locales = layout.getLocales();
            final int numLocales = locales.size();
            for (int localeIndex = 0; localeIndex < numLocales; ++localeIndex) {
                final Locale locale = locales.get(localeIndex);
                if (locale.getCountry().equals(systemLocale.getCountry())) {
                    return layout.getDescriptor();
                }
            }
        }

        // Give up and just use the highest priority layout with matching language
        return layouts.get(0).getDescriptor();
    }


    private void visitAllKeyboardLayouts(KeyboardLayoutVisitor visitor) {
        final PackageManager pm = mContext.getPackageManager();
        Intent intent = new Intent(InputManager.ACTION_QUERY_KEYBOARD_LAYOUTS);
        for (ResolveInfo resolveInfo : pm.queryBroadcastReceivers(intent, PackageManager.GET_META_DATA | PackageManager.MATCH_DIRECT_BOOT_AWARE | PackageManager.MATCH_DIRECT_BOOT_UNAWARE)) {
            final ActivityInfo activityInfo = resolveInfo.activityInfo;
            final int priority = resolveInfo.priority;
            visitKeyboardLayoutsInPackage(pm, activityInfo, null, priority, visitor);
        }
    }


    private void visitKeyboardLayoutsInPackage(PackageManager pm, ActivityInfo receiver,
            String keyboardName, int requestedPriority, KeyboardLayoutVisitor visitor) {
        Bundle metaData = receiver.metaData;
        if (metaData == null) {
            return;
        }

        int configResId = metaData.getInt(InputManager.META_DATA_KEYBOARD_LAYOUTS);
        if (configResId == 0) {
            Slog.w(TAG, "Missing meta-data '" + InputManager.META_DATA_KEYBOARD_LAYOUTS
                    + "' on receiver " + receiver.packageName + "/" + receiver.name);
            return;
        }

        CharSequence receiverLabel = receiver.loadLabel(pm);
        String collection = receiverLabel != null ? receiverLabel.toString() : "";
        int priority;
        if ((receiver.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
            priority = requestedPriority;
        } else {
            priority = 0;
        }

        try {
            Resources resources = pm.getResourcesForApplication(receiver.applicationInfo);
            XmlResourceParser parser = resources.getXml(configResId);
            try {
                XmlUtils.beginDocument(parser, "keyboard-layouts");

                for (;;) {
                    XmlUtils.nextElement(parser);
                    String element = parser.getName();
                    if (element == null) {
                        break;
                    }
                    if (element.equals("keyboard-layout")) {
                        TypedArray a = resources.obtainAttributes(parser, com.android.internal.R.styleable.KeyboardLayout);
                        try {
                            String name = a.getString(
                                    com.android.internal.R.styleable.KeyboardLayout_name);
                            String label = a.getString(
                                    com.android.internal.R.styleable.KeyboardLayout_label);
                            int keyboardLayoutResId = a.getResourceId(
                                    com.android.internal.R.styleable.KeyboardLayout_keyboardLayout,
                                    0);
                            String languageTags = a.getString(
                                    com.android.internal.R.styleable.KeyboardLayout_locale);
                            LocaleList locales = getLocalesFromLanguageTags(languageTags);
                            int vid = a.getInt(
                                    com.android.internal.R.styleable.KeyboardLayout_vendorId, -1);
                            int pid = a.getInt(
                                    com.android.internal.R.styleable.KeyboardLayout_productId, -1);

                            if (name == null || label == null || keyboardLayoutResId == 0) {
                                Slog.w(TAG, "Missing required 'name', 'label' or 'keyboardLayout' "
                                        + "attributes in keyboard layout "
                                        + "resource from receiver "
                                        + receiver.packageName + "/" + receiver.name);
                            } else {
                                String descriptor = KeyboardLayoutDescriptor.format(
                                        receiver.packageName, receiver.name, name);
                                if (keyboardName == null || name.equals(keyboardName)) {
                                    KeyboardLayout layout = new KeyboardLayout(
                                            descriptor, label, collection, priority,
                                            locales, vid, pid);
                                    visitor.visitKeyboardLayout(resources, keyboardLayoutResId, layout);
                                }
                            }
                        } finally {
                            a.recycle();
                        }
                    } else {
                        Slog.w(TAG, "Skipping unrecognized element '" + element
                                + "' in keyboard layout resource from receiver "
                                + receiver.packageName + "/" + receiver.name);
                    }
                }
            } finally {
                parser.close();
            }
        } catch (Exception ex) {
            Slog.w(TAG, "Could not parse keyboard layout resource from receiver "
                    + receiver.packageName + "/" + receiver.name, ex);
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