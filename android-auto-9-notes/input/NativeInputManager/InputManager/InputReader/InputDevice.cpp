//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/services/inputflinger/InputReader.cpp
InputDevice::InputDevice(InputReaderContext* context, int32_t id, int32_t generation,
        int32_t controllerNumber, const InputDeviceIdentifier& identifier, uint32_t classes) :
        mContext(context), mId(id), mGeneration(generation), mControllerNumber(controllerNumber),
        mIdentifier(identifier), mClasses(classes),
        mSources(0), mIsExternal(false), mHasMic(false), mDropUntilNextSync(false) {
}

/**
 * 对于 cyttsp6_mt1 设备 # INPUT_DEVICE_CLASS_EXTERNAL | INPUT_DEVICE_CLASS_TOUCH_MT | INPUT_DEVICE_CLASS_TOUCH
 * mIsExternal 为 true
*/
inline bool InputDevice::isExternal() { return mIsExternal; }
inline void InputDevice::setExternal(bool external) { mIsExternal = external; }


void InputDevice::addMapper(InputMapper* mapper) {
    mMappers.add(mapper);
}

inline bool InputDevice::isIgnored() { 
    return mMappers.isEmpty(); 
}


inline InputReaderContext* InputDevice::getContext() { return mContext; }




void InputDevice::configure(nsecs_t when, const InputReaderConfiguration* config, uint32_t changes) {
    mSources = 0;

    if (!isIgnored()) {
        if (!changes) { // first time only
            mContext->getEventHub()->getConfiguration(mId, &mConfiguration);
        }

        if (!changes || (changes & InputReaderConfiguration::CHANGE_KEYBOARD_LAYOUTS)) {
            /**
             * 
            */
            if (!(mClasses & INPUT_DEVICE_CLASS_VIRTUAL)) {
                sp<KeyCharacterMap> keyboardLayout = mContext->getPolicy()->getKeyboardLayoutOverlay(mIdentifier);
                if (mContext->getEventHub()->setKeyboardLayoutOverlay(mId, keyboardLayout)) {
                    bumpGeneration();
                }
            }
        }

        if (!changes || (changes & InputReaderConfiguration::CHANGE_DEVICE_ALIAS)) {
            /**
             * 
            */
            if (!(mClasses & INPUT_DEVICE_CLASS_VIRTUAL)) {
                String8 alias = mContext->getPolicy()->getDeviceAlias(mIdentifier);
                if (mAlias != alias) {
                    mAlias = alias;
                    bumpGeneration();
                }
            }
        }

        if (!changes || (changes & InputReaderConfiguration::CHANGE_ENABLED_STATE)) {
            ssize_t index = config->disabledDevices.indexOf(mId);
            bool enabled = index < 0;
            setEnabled(enabled, when);
        }

        size_t numMappers = mMappers.size();
        for (size_t i = 0; i < numMappers; i++) {
            InputMapper* mapper = mMappers[i];
            mapper->configure(when, config, changes);
            mSources |= mapper->getSources();
        }
    }
}










/**
 * InputReaderThread::threadLoop()
 * --> void InputReader::loopOnce()
 * ---> void InputReader::processEventsLocked(const RawEvent* rawEvents, size_t count)
 * ----> void InputReader::processEventsForDeviceLocked(int32_t deviceId,const RawEvent* rawEvents, size_t count)
 * ----->  device->process(rawEvents, count);
 * 
*/
void InputDevice::process(const RawEvent* rawEvents, size_t count) {
    // Process all of the events in order for each mapper.
    // We cannot simply ask each mapper to process them in bulk because mappers may
    // have side-effects that must be interleaved.  For example, joystick movement events and
    // gamepad button presses are handled by different mappers but they should be dispatched
    // in the order received.
    size_t numMappers = mMappers.size();
    for (const RawEvent* rawEvent = rawEvents; count != 0; rawEvent++) {
#if DEBUG_RAW_EVENTS
        ALOGD("Input event: device=%d type=0x%04x code=0x%04x value=0x%08x when=%" PRId64,
                rawEvent->deviceId, rawEvent->type, rawEvent->code, rawEvent->value,
                rawEvent->when);
#endif

        if (mDropUntilNextSync) {
            if (rawEvent->type == EV_SYN && rawEvent->code == SYN_REPORT) {
                mDropUntilNextSync = false;
#if DEBUG_RAW_EVENTS
                ALOGD("Recovered from input event buffer overrun.");
#endif
            } else {
#if DEBUG_RAW_EVENTS
                ALOGD("Dropped input event while waiting for next input sync.");
#endif
            }
        } else if (rawEvent->type == EV_SYN && rawEvent->code == SYN_DROPPED) {
            ALOGI("Detected input event buffer overrun for device %s.", getName().string());
            mDropUntilNextSync = true;
            reset(rawEvent->when);
        } else {
            for (size_t i = 0; i < numMappers; i++) {
                InputMapper* mapper = mMappers[i];
                mapper->process(rawEvent);
            }
        }
        --count;
    }
}