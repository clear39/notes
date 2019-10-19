

//  @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/inputflinger/InputReader.cpp

bool InputReaderThread::threadLoop() {
    mReader->loopOnce();
    return true;
}


void InputReader::loopOnce() {
    int32_t oldGeneration;
    int32_t timeoutMillis;
    bool inputDevicesChanged = false;
    Vector<InputDeviceInfo> inputDevices;
    { // acquire lock
        AutoMutex _l(mLock);

        oldGeneration = mGeneration;
        timeoutMillis = -1;

        uint32_t changes = mConfigurationChangesToRefresh;
        if (changes) {
            mConfigurationChangesToRefresh = 0;
            timeoutMillis = 0;
            refreshConfigurationLocked(changes);
        } else if (mNextTimeout != LLONG_MAX) {
            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
            timeoutMillis = toMillisecondTimeoutDelay(now, mNextTimeout);
        }
    } // release lock

    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);

    { // acquire lock
        AutoMutex _l(mLock);
        mReaderIsAliveCondition.broadcast();

        if (count) {
            processEventsLocked(mEventBuffer, count);
        }

        if (mNextTimeout != LLONG_MAX) {
            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
            if (now >= mNextTimeout) {
#if DEBUG_RAW_EVENTS
                ALOGD("Timeout expired, latency=%0.3fms", (now - mNextTimeout) * 0.000001f);
#endif
                mNextTimeout = LLONG_MAX;
                timeoutExpiredLocked(now);
            }
        }

        if (oldGeneration != mGeneration) {
            inputDevicesChanged = true;
            getInputDevicesLocked(inputDevices);
        }
    } // release lock

    // Send out a message that the describes the changed input devices.
    if (inputDevicesChanged) {
        mPolicy->notifyInputDevicesChanged(inputDevices);
    }

    // Flush queued events out to the listener.
    // This must happen outside of the lock because the listener could potentially call
    // back into the InputReader's methods, such as getScanCodeState, or become blocked
    // on another thread similarly waiting to acquire the InputReader lock thereby
    // resulting in a deadlock.  This situation is actually quite plausible because the
    // listener is actually the input dispatcher, which calls into the window manager,
    // which occasionally calls into the input reader.
    mQueuedListener->flush();
}


void InputReader::processEventsLocked(const RawEvent* rawEvents, size_t count) {
    for (const RawEvent* rawEvent = rawEvents; count;) {
        int32_t type = rawEvent->type;
        size_t batchSize = 1;
        if (type < EventHubInterface::FIRST_SYNTHETIC_EVENT) {
            int32_t deviceId = rawEvent->deviceId;
            while (batchSize < count) {
                if (rawEvent[batchSize].type >= EventHubInterface::FIRST_SYNTHETIC_EVENT || rawEvent[batchSize].deviceId != deviceId) {
                    break;
                }
                batchSize += 1;
            }
#if DEBUG_RAW_EVENTS
            ALOGD("BatchSize: %zu Count: %zu", batchSize, count);
#endif
            processEventsForDeviceLocked(deviceId, rawEvent, batchSize);
        } else {
            switch (rawEvent->type) {
            case EventHubInterface::DEVICE_ADDED:
                addDeviceLocked(rawEvent->when, rawEvent->deviceId);
                break;
            case EventHubInterface::DEVICE_REMOVED:
                removeDeviceLocked(rawEvent->when, rawEvent->deviceId);
                break;
            case EventHubInterface::FINISHED_DEVICE_SCAN:
                handleConfigurationChangedLocked(rawEvent->when);
                break;
            default:
                ALOG_ASSERT(false); // can't happen
                break;
            }
        }
        count -= batchSize;
        rawEvent += batchSize;
    }
}


void InputReader::addDeviceLocked(nsecs_t when, int32_t deviceId) {
    ssize_t deviceIndex = mDevices.indexOfKey(deviceId);
    if (deviceIndex >= 0) {
        ALOGW("Ignoring spurious device added event for deviceId %d.", deviceId);
        return;
    }

    InputDeviceIdentifier identifier = mEventHub->getDeviceIdentifier(deviceId);
    uint32_t classes = mEventHub->getDeviceClasses(deviceId);
    int32_t controllerNumber = mEventHub->getDeviceControllerNumber(deviceId);

    InputDevice* device = createDeviceLocked(deviceId, controllerNumber, identifier, classes);
    device->configure(when, &mConfig, 0);
    device->reset(when);

    if (device->isIgnored()) {
        ALOGI("Device added: id=%d, name='%s' (ignored non-input device)", deviceId, identifier.name.string());
    } else {
        ALOGI("Device added: id=%d, name='%s', sources=0x%08x", deviceId,identifier.name.string(), device->getSources());
    }

    mDevices.add(deviceId, device);
    bumpGenerationLocked();

    if (device->getClasses() & INPUT_DEVICE_CLASS_EXTERNAL_STYLUS) {
        notifyExternalStylusPresenceChanged();
    }
}


InputDevice* InputReader::createDeviceLocked(int32_t deviceId, int32_t controllerNumber,const InputDeviceIdentifier& identifier, uint32_t classes) {
    InputDevice* device = new InputDevice(&mContext, deviceId, bumpGenerationLocked(),controllerNumber, identifier, classes);

    // External devices.
    if (classes & INPUT_DEVICE_CLASS_EXTERNAL) {
        device->setExternal(true);
    }

    // Devices with mics.
    if (classes & INPUT_DEVICE_CLASS_MIC) {
        device->setMic(true);
    }

    // Switch-like devices.
    if (classes & INPUT_DEVICE_CLASS_SWITCH) {
        device->addMapper(new SwitchInputMapper(device));
    }

    // Scroll wheel-like devices.
    if (classes & INPUT_DEVICE_CLASS_ROTARY_ENCODER) {
        device->addMapper(new RotaryEncoderInputMapper(device));
    }

    // Vibrator-like devices.
    if (classes & INPUT_DEVICE_CLASS_VIBRATOR) {
        device->addMapper(new VibratorInputMapper(device));
    }

    // Keyboard-like devices.
    uint32_t keyboardSource = 0;
    int32_t keyboardType = AINPUT_KEYBOARD_TYPE_NON_ALPHABETIC;
    if (classes & INPUT_DEVICE_CLASS_KEYBOARD) {
        keyboardSource |= AINPUT_SOURCE_KEYBOARD;
    }
    if (classes & INPUT_DEVICE_CLASS_ALPHAKEY) {
        keyboardType = AINPUT_KEYBOARD_TYPE_ALPHABETIC;
    }
    if (classes & INPUT_DEVICE_CLASS_DPAD) {
        keyboardSource |= AINPUT_SOURCE_DPAD;
    }
    if (classes & INPUT_DEVICE_CLASS_GAMEPAD) {
        keyboardSource |= AINPUT_SOURCE_GAMEPAD;
    }

    if (keyboardSource != 0) {
        device->addMapper(new KeyboardInputMapper(device, keyboardSource, keyboardType));
    }

    // Cursor-like devices.
    if (classes & INPUT_DEVICE_CLASS_CURSOR) {
        device->addMapper(new CursorInputMapper(device));
    }

    // Touchscreens and touchpad devices.
    if (classes & INPUT_DEVICE_CLASS_TOUCH_MT) {
        device->addMapper(new MultiTouchInputMapper(device));
    } else if (classes & INPUT_DEVICE_CLASS_TOUCH) {
        device->addMapper(new SingleTouchInputMapper(device));
    }

    // Joystick-like devices.
    if (classes & INPUT_DEVICE_CLASS_JOYSTICK) {
        device->addMapper(new JoystickInputMapper(device));
    }

    // External stylus-like devices.
    if (classes & INPUT_DEVICE_CLASS_EXTERNAL_STYLUS) {
        device->addMapper(new ExternalStylusInputMapper(device));
    }

    return device;
}



void InputReader::getInputDevicesLocked(Vector<InputDeviceInfo>& outInputDevices) {
    outInputDevices.clear();

    /**
     *  KeyedVector<int32_t, InputDevice*> mDevices;
    */
    size_t numDevices = mDevices.size();
    for (size_t i = 0; i < numDevices; i++) {
        InputDevice* device = mDevices.valueAt(i);
        if (!device->isIgnored()) {
            outInputDevices.push();
            device->getDeviceInfo(&outInputDevices.editTop());
        }
    }
}



