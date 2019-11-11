
//  @   frameworks/native/services/inputflinger/EventHub.cpp
/**
 * 
*/
EventHub::EventHub(void) :
        mBuiltInKeyboardId(NO_BUILT_IN_KEYBOARD), mNextDeviceId(1), mControllerNumbers(),
        mOpeningDevices(0), mClosingDevices(0),
        mNeedToSendFinishedDeviceScan(false),
        mNeedToReopenDevices(false), mNeedToScanDevices(true),
        mPendingEventCount(0), mPendingEventIndex(0), mPendingINotify(false) {
    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);

    mEpollFd = epoll_create(EPOLL_SIZE_HINT);
    LOG_ALWAYS_FATAL_IF(mEpollFd < 0, "Could not create epoll instance.  errno=%d", errno);

    mINotifyFd = inotify_init();
    /**
     * static const char *DEVICE_PATH = "/dev/input";
    */
    int result = inotify_add_watch(mINotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE);
    LOG_ALWAYS_FATAL_IF(result < 0, "Could not register INotify for %s.  errno=%d",DEVICE_PATH, errno);

    struct epoll_event eventItem;
    memset(&eventItem, 0, sizeof(eventItem));
    eventItem.events = EPOLLIN;
    eventItem.data.u32 = EPOLL_ID_INOTIFY;
    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add INotify to epoll instance.  errno=%d", errno);

    int wakeFds[2];
    result = pipe(wakeFds);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not create wake pipe.  errno=%d", errno);

    mWakeReadPipeFd = wakeFds[0];
    mWakeWritePipeFd = wakeFds[1];

    result = fcntl(mWakeReadPipeFd, F_SETFL, O_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not make wake read pipe non-blocking.  errno=%d",errno);

    result = fcntl(mWakeWritePipeFd, F_SETFL, O_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not make wake write pipe non-blocking.  errno=%d",errno);

    eventItem.data.u32 = EPOLL_ID_WAKE;
    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add wake read pipe to epoll instance.  errno=%d",errno);

    int major, minor;
    getLinuxRelease(&major, &minor);
    // EPOLLWAKEUP was introduced in kernel 3.5
    mUsingEpollWakeup = major > 3 || (major == 3 && minor >= 5);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
EventHub::Device* EventHub::getDeviceLocked(int32_t deviceId) const {
    if (deviceId == BUILT_IN_KEYBOARD_ID) {
        deviceId = mBuiltInKeyboardId;
    }
    /**
     * KeyedVector<int32_t, Device*> mDevices;
    */
    ssize_t index = mDevices.indexOfKey(deviceId);
    return index >= 0 ? mDevices.valueAt(index) : NULL;
}






//////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * InputReaderThread::threadLoop()
 * --> void InputReader::loopOnce()
 * ---> size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);
*/
size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) {
    ALOG_ASSERT(bufferSize >= 1);

    AutoMutex _l(mLock);

    struct input_event readBuffer[bufferSize];

    RawEvent* event = buffer;
    size_t capacity = bufferSize;
    bool awoken = false;
    for (;;) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);

        // Reopen input devices if needed.
        if (mNeedToReopenDevices) {
            mNeedToReopenDevices = false;

            ALOGI("Reopening all input devices due to a configuration change.");

            closeAllDevicesLocked();
            mNeedToScanDevices = true;
            break; // return to the caller before we actually rescan
        }

        // Report any devices that had last been added/removed.
        while (mClosingDevices) {
            Device* device = mClosingDevices;
            ALOGV("Reporting device closed: id=%d, name=%s\n", device->id, device->path.string());
            mClosingDevices = device->next;
            event->when = now;
            event->deviceId = device->id == mBuiltInKeyboardId ? BUILT_IN_KEYBOARD_ID : device->id;
            event->type = DEVICE_REMOVED;
            event += 1;
            delete device;
            mNeedToSendFinishedDeviceScan = true;
            if (--capacity == 0) {
                break;
            }
        }

        if (mNeedToScanDevices) {
            mNeedToScanDevices = false;
            scanDevicesLocked();
            mNeedToSendFinishedDeviceScan = true;
        }

        while (mOpeningDevices != NULL) {
            Device* device = mOpeningDevices;
            ALOGV("Reporting device opened: id=%d, name=%s\n",device->id, device->path.string());
            mOpeningDevices = device->next;
            event->when = now;
            event->deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;
            event->type = DEVICE_ADDED;
            event += 1;
            mNeedToSendFinishedDeviceScan = true;
            if (--capacity == 0) {
                break;
            }
        }

        if (mNeedToSendFinishedDeviceScan) {
            mNeedToSendFinishedDeviceScan = false;
            event->when = now;
            event->type = FINISHED_DEVICE_SCAN;
            event += 1;
            if (--capacity == 0) {
                break;
            }
        }

        // Grab the next input event.
        bool deviceChanged = false;
        while (mPendingEventIndex < mPendingEventCount) {
            const struct epoll_event& eventItem = mPendingEventItems[mPendingEventIndex++];
            if (eventItem.data.u32 == EPOLL_ID_INOTIFY) {
                if (eventItem.events & EPOLLIN) {
                    mPendingINotify = true;
                } else {
                    ALOGW("Received unexpected epoll event 0x%08x for INotify.", eventItem.events);
                }
                continue;
            }

            if (eventItem.data.u32 == EPOLL_ID_WAKE) {
                if (eventItem.events & EPOLLIN) {
                    ALOGV("awoken after wake()");
                    awoken = true;
                    char buffer[16];
                    ssize_t nRead;
                    do {
                        nRead = read(mWakeReadPipeFd, buffer, sizeof(buffer));
                    } while ((nRead == -1 && errno == EINTR) || nRead == sizeof(buffer));
                } else {
                    ALOGW("Received unexpected epoll event 0x%08x for wake read pipe.",eventItem.events);
                }
                continue;
            }

            ssize_t deviceIndex = mDevices.indexOfKey(eventItem.data.u32);
            if (deviceIndex < 0) {
                ALOGW("Received unexpected epoll event 0x%08x for unknown device id %d.", eventItem.events, eventItem.data.u32);
                continue;
            }

            Device* device = mDevices.valueAt(deviceIndex);
            if (eventItem.events & EPOLLIN) {
                int32_t readSize = read(device->fd, readBuffer,sizeof(struct input_event) * capacity);
                if (readSize == 0 || (readSize < 0 && errno == ENODEV)) {
                    // Device was removed before INotify noticed.
                    ALOGW("could not get event, removed? (fd: %d size: %" PRId32 " bufferSize: %zu capacity: %zu errno: %d)\n",device->fd, readSize, bufferSize, capacity, errno);
                    deviceChanged = true;
                    closeDeviceLocked(device);
                } else if (readSize < 0) {
                    if (errno != EAGAIN && errno != EINTR) {
                        ALOGW("could not get event (errno=%d)", errno);
                    }
                } else if ((readSize % sizeof(struct input_event)) != 0) {
                    ALOGE("could not get event (wrong size: %d)", readSize);
                } else {
                    int32_t deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;

                    size_t count = size_t(readSize) / sizeof(struct input_event);
                    for (size_t i = 0; i < count; i++) {
                        struct input_event& iev = readBuffer[i];
                        ALOGV("%s got: time=%d.%06d, type=%d, code=%d, value=%d",
                                device->path.string(),
                                (int) iev.time.tv_sec, (int) iev.time.tv_usec,
                                iev.type, iev.code, iev.value);

                        // Some input devices may have a better concept of the time
                        // when an input event was actually generated than the kernel
                        // which simply timestamps all events on entry to evdev.
                        // This is a custom Android extension of the input protocol
                        // mainly intended for use with uinput based device drivers.
                        if (iev.type == EV_MSC) {
                            if (iev.code == MSC_ANDROID_TIME_SEC) {
                                device->timestampOverrideSec = iev.value;
                                continue;
                            } else if (iev.code == MSC_ANDROID_TIME_USEC) {
                                device->timestampOverrideUsec = iev.value;
                                continue;
                            }
                        }
                        if (device->timestampOverrideSec || device->timestampOverrideUsec) {
                            iev.time.tv_sec = device->timestampOverrideSec;
                            iev.time.tv_usec = device->timestampOverrideUsec;
                            if (iev.type == EV_SYN && iev.code == SYN_REPORT) {
                                device->timestampOverrideSec = 0;
                                device->timestampOverrideUsec = 0;
                            }
                            ALOGV("applied override time %d.%06d", int(iev.time.tv_sec), int(iev.time.tv_usec));
                        }

                        // Use the time specified in the event instead of the current time
                        // so that downstream code can get more accurate estimates of
                        // event dispatch latency from the time the event is enqueued onto
                        // the evdev client buffer.
                        //
                        // The event's timestamp fortuitously uses the same monotonic clock
                        // time base as the rest of Android.  The kernel event device driver
                        // (drivers/input/evdev.c) obtains timestamps using ktime_get_ts().
                        // The systemTime(SYSTEM_TIME_MONOTONIC) function we use everywhere
                        // calls clock_gettime(CLOCK_MONOTONIC) which is implemented as a
                        // system call that also queries ktime_get_ts().
                        event->when = nsecs_t(iev.time.tv_sec) * 1000000000LL  + nsecs_t(iev.time.tv_usec) * 1000LL;
                        ALOGV("event time %" PRId64 ", now %" PRId64, event->when, now);

                        // Bug 7291243: Add a guard in case the kernel generates timestamps
                        // that appear to be far into the future because they were generated
                        // using the wrong clock source.
                        //
                        // This can happen because when the input device is initially opened
                        // it has a default clock source of CLOCK_REALTIME.  Any input events
                        // enqueued right after the device is opened will have timestamps
                        // generated using CLOCK_REALTIME.  We later set the clock source
                        // to CLOCK_MONOTONIC but it is already too late.
                        //
                        // Invalid input event timestamps can result in ANRs, crashes and
                        // and other issues that are hard to track down.  We must not let them
                        // propagate through the system.
                        //
                        // Log a warning so that we notice the problem and recover gracefully.
                        if (event->when >= now + 10 * 1000000000LL) {
                            // Double-check.  Time may have moved on.
                            nsecs_t time = systemTime(SYSTEM_TIME_MONOTONIC);
                            if (event->when > time) {
                                ALOGW("An input event from %s has a timestamp that appears to "
                                        "have been generated using the wrong clock source "
                                        "(expected CLOCK_MONOTONIC): "
                                        "event time %" PRId64 ", current time %" PRId64
                                        ", call time %" PRId64 ".  "
                                        "Using current time instead.",
                                        device->path.string(), event->when, time, now);
                                event->when = time;
                            } else {
                                ALOGV("Event time is ok but failed the fast path and required "
                                        "an extra call to systemTime: "
                                        "event time %" PRId64 ", current time %" PRId64
                                        ", call time %" PRId64 ".",
                                        event->when, time, now);
                            }
                        }
                        event->deviceId = deviceId;
                        event->type = iev.type;
                        event->code = iev.code;
                        event->value = iev.value;
                        event += 1;
                        capacity -= 1;
                    }
                    if (capacity == 0) {
                        // The result buffer is full.  Reset the pending event index
                        // so we will try to read the device again on the next iteration.
                        mPendingEventIndex -= 1;
                        break;
                    }
                }
            } else if (eventItem.events & EPOLLHUP) {
                ALOGI("Removing device %s due to epoll hang-up event.", device->identifier.name.string());
                deviceChanged = true;
                closeDeviceLocked(device);
            } else {
                ALOGW("Received unexpected epoll event 0x%08x for device %s.", eventItem.events, device->identifier.name.string());
            }
        }

        // readNotify() will modify the list of devices so this must be done after
        // processing all other events to ensure that we read all remaining events
        // before closing the devices.
        if (mPendingINotify && mPendingEventIndex >= mPendingEventCount) {
            mPendingINotify = false;
            readNotifyLocked();
            deviceChanged = true;
        }

        // Report added or removed devices immediately.
        if (deviceChanged) {
            continue;
        }

        // Return now if we have collected any events or if we were explicitly awoken.
        if (event != buffer || awoken) {
            break;
        }

        // Poll for events.  Mind the wake lock dance!
        // We hold a wake lock at all times except during epoll_wait().  This works due to some
        // subtle choreography.  When a device driver has pending (unread) events, it acquires
        // a kernel wake lock.  However, once the last pending event has been read, the device
        // driver will release the kernel wake lock.  To prevent the system from going to sleep
        // when this happens, the EventHub holds onto its own user wake lock while the client
        // is processing events.  Thus the system can only sleep if there are no events
        // pending or currently being processed.
        //
        // The timeout is advisory only.  If the device is asleep, it will not wake just to
        // service the timeout.
        mPendingEventIndex = 0;

        mLock.unlock(); // release lock before poll, must be before release_wake_lock
        release_wake_lock(WAKE_LOCK_ID);

        int pollResult = epoll_wait(mEpollFd, mPendingEventItems, EPOLL_MAX_EVENTS, timeoutMillis);

        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
        mLock.lock(); // reacquire lock after poll, must be after acquire_wake_lock

        if (pollResult == 0) {
            // Timed out.
            mPendingEventCount = 0;
            break;
        }

        if (pollResult < 0) {
            // An error occurred.
            mPendingEventCount = 0;

            // Sleep after errors to avoid locking up the system.
            // Hopefully the error is transient.
            if (errno != EINTR) {
                ALOGW("poll failed (errno=%d)\n", errno);
                usleep(100000);
            }
        } else {
            // Some events occurred.
            mPendingEventCount = size_t(pollResult);
        }
    }

    // All done, return the number of events we read.
    return event - buffer;
}



/**
 * 
 * -->size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) 
 * ---> 
*/
void EventHub::scanDevicesLocked() {
    /**
     * static const char *DEVICE_PATH = "/dev/input";
    */
    status_t res = scanDirLocked(DEVICE_PATH);
    if(res < 0) {
        ALOGE("scan dir failed for %s\n", DEVICE_PATH);
    }
    if (mDevices.indexOfKey(VIRTUAL_KEYBOARD_ID) < 0) {
        createVirtualKeyboardLocked();
    }
}

status_t EventHub::scanDirLocked(const char *dirname)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        openDeviceLocked(devname);
    }
    closedir(dir);
    return 0;
}

status_t EventHub::openDeviceLocked(const char *devicePath) {
    char buffer[80];

    ALOGV("Opening device: %s", devicePath);

    int fd = open(devicePath, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if(fd < 0) {
        ALOGE("could not open %s, %s\n", devicePath, strerror(errno));
        return -1;
    }

    InputDeviceIdentifier identifier;

    // Get device name.
    if(ioctl(fd, EVIOCGNAME(sizeof(buffer) - 1), &buffer) < 1) {
        //fprintf(stderr, "could not get device name for %s, %s\n", devicePath, strerror(errno));
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        identifier.name.setTo(buffer);
    }

    /**
     * void InputReader::refreshConfigurationLocked(uint32_t changes)
     * --> EventHub::setExcludedDevices(const Vector<String8>& devices) 
    */
    // Check to see if the device is on our excluded list
    for (size_t i = 0; i < mExcludedDevices.size(); i++) {
        const String8& item = mExcludedDevices.itemAt(i);
        if (identifier.name == item) {
            ALOGI("ignoring event id %s driver %s\n", devicePath, item.string());
            close(fd);
            return -1;
        }
    }

    // Get device driver version.
    int driverVersion;
    if(ioctl(fd, EVIOCGVERSION, &driverVersion)) {
        ALOGE("could not get driver version for %s, %s\n", devicePath, strerror(errno));
        close(fd);
        return -1;
    }

    // Get device identifier.
    struct input_id inputId;
    if(ioctl(fd, EVIOCGID, &inputId)) {
        ALOGE("could not get device input id for %s, %s\n", devicePath, strerror(errno));
        close(fd);
        return -1;
    }
    identifier.bus = inputId.bustype;
    identifier.product = inputId.product;
    identifier.vendor = inputId.vendor;
    identifier.version = inputId.version;

    // Get device physical location.
    if(ioctl(fd, EVIOCGPHYS(sizeof(buffer) - 1), &buffer) < 1) {
        //fprintf(stderr, "could not get location for %s, %s\n", devicePath, strerror(errno));
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        identifier.location.setTo(buffer);
    }

    // Get device unique id.
    if(ioctl(fd, EVIOCGUNIQ(sizeof(buffer) - 1), &buffer) < 1) {
        //fprintf(stderr, "could not get idstring for %s, %s\n", devicePath, strerror(errno));
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        identifier.uniqueId.setTo(buffer);
    }

    // Fill in the descriptor.
    assignDescriptorLocked(identifier);

    // Allocate device.  (The device object takes ownership of the fd at this point.)
    int32_t deviceId = mNextDeviceId++;
    Device* device = new Device(fd, deviceId, String8(devicePath), identifier);

    ALOGV("add device %d: %s\n", deviceId, devicePath);
    ALOGV("  bus:        %04x\n"
         "  vendor      %04x\n"
         "  product     %04x\n"
         "  version     %04x\n",
        identifier.bus, identifier.vendor, identifier.product, identifier.version);
    ALOGV("  name:       \"%s\"\n", identifier.name.string());
    ALOGV("  location:   \"%s\"\n", identifier.location.string());
    ALOGV("  unique id:  \"%s\"\n", identifier.uniqueId.string());
    ALOGV("  descriptor: \"%s\"\n", identifier.descriptor.string());
    ALOGV("  driver:     v%d.%d.%d\n",driverVersion >> 16, (driverVersion >> 8) & 0xff, driverVersion & 0xff);

    // Load the configuration file for the device.
    /**
     * 加载后缀为 idc 文件，并且解析
    */
    loadConfigurationLocked(device);

    // Figure out the kinds of events the device reports.
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(device->keyBitmask)), device->keyBitmask);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(device->absBitmask)), device->absBitmask);
    ioctl(fd, EVIOCGBIT(EV_REL, sizeof(device->relBitmask)), device->relBitmask);
    ioctl(fd, EVIOCGBIT(EV_SW, sizeof(device->swBitmask)), device->swBitmask);
    ioctl(fd, EVIOCGBIT(EV_LED, sizeof(device->ledBitmask)), device->ledBitmask);
    ioctl(fd, EVIOCGBIT(EV_FF, sizeof(device->ffBitmask)), device->ffBitmask);
    ioctl(fd, EVIOCGPROP(sizeof(device->propBitmask)), device->propBitmask);

    // See if this is a keyboard.  Ignore everything in the button range except for
    // joystick and gamepad buttons which are handled like keyboards for the most part.
    bool haveKeyboardKeys = containsNonZeroByte(device->keyBitmask, 0, sizeof_bit_array(BTN_MISC))
            || containsNonZeroByte(device->keyBitmask, sizeof_bit_array(KEY_OK),
                    sizeof_bit_array(KEY_MAX + 1));
    bool haveGamepadButtons = containsNonZeroByte(device->keyBitmask, sizeof_bit_array(BTN_MISC),
                    sizeof_bit_array(BTN_MOUSE))
            || containsNonZeroByte(device->keyBitmask, sizeof_bit_array(BTN_JOYSTICK),
                    sizeof_bit_array(BTN_DIGI));
    if (haveKeyboardKeys || haveGamepadButtons) {
        device->classes |= INPUT_DEVICE_CLASS_KEYBOARD;
    }

    // See if this is a cursor device such as a trackball or mouse.
    if (test_bit(BTN_MOUSE, device->keyBitmask)
            && test_bit(REL_X, device->relBitmask)
            && test_bit(REL_Y, device->relBitmask)) {
        device->classes |= INPUT_DEVICE_CLASS_CURSOR;
    }

    // See if this is a rotary encoder type device.
    String8 deviceType = String8();
    if (device->configuration &&
        device->configuration->tryGetProperty(String8("device.type"), deviceType)) {
            if (!deviceType.compare(String8("rotaryEncoder"))) {
                device->classes |= INPUT_DEVICE_CLASS_ROTARY_ENCODER;
            }
    }

    // See if this is a touch pad.
    // Is this a new modern multi-touch driver?
    if (test_bit(ABS_MT_POSITION_X, device->absBitmask)
            && test_bit(ABS_MT_POSITION_Y, device->absBitmask)) {
        // Some joysticks such as the PS3 controller report axes that conflict
        // with the ABS_MT range.  Try to confirm that the device really is
        // a touch screen.
        if (test_bit(BTN_TOUCH, device->keyBitmask) || !haveGamepadButtons) {
            device->classes |= INPUT_DEVICE_CLASS_TOUCH | INPUT_DEVICE_CLASS_TOUCH_MT;
        }
    // Is this an old style single-touch driver?
    } else if (test_bit(BTN_TOUCH, device->keyBitmask)
            && test_bit(ABS_X, device->absBitmask)
            && test_bit(ABS_Y, device->absBitmask)) {
        device->classes |= INPUT_DEVICE_CLASS_TOUCH;
    // Is this a BT stylus?
    } else if ((test_bit(ABS_PRESSURE, device->absBitmask) ||
                test_bit(BTN_TOUCH, device->keyBitmask))
            && !test_bit(ABS_X, device->absBitmask)
            && !test_bit(ABS_Y, device->absBitmask)) {
        device->classes |= INPUT_DEVICE_CLASS_EXTERNAL_STYLUS;
        // Keyboard will try to claim some of the buttons but we really want to reserve those so we
        // can fuse it with the touch screen data, so just take them back. Note this means an
        // external stylus cannot also be a keyboard device.
        device->classes &= ~INPUT_DEVICE_CLASS_KEYBOARD;
    }

    // See if this device is a joystick.
    // Assumes that joysticks always have gamepad buttons in order to distinguish them
    // from other devices such as accelerometers that also have absolute axes.
    if (haveGamepadButtons) {
        uint32_t assumedClasses = device->classes | INPUT_DEVICE_CLASS_JOYSTICK;
        for (int i = 0; i <= ABS_MAX; i++) {
            if (test_bit(i, device->absBitmask)
                    && (getAbsAxisUsage(i, assumedClasses) & INPUT_DEVICE_CLASS_JOYSTICK)) {
                device->classes = assumedClasses;
                break;
            }
        }
    }

    // Check whether this device has switches.
    for (int i = 0; i <= SW_MAX; i++) {
        if (test_bit(i, device->swBitmask)) {
            device->classes |= INPUT_DEVICE_CLASS_SWITCH;
            break;
        }
    }

    // Check whether this device supports the vibrator.
    if (test_bit(FF_RUMBLE, device->ffBitmask)) {
        device->classes |= INPUT_DEVICE_CLASS_VIBRATOR;
    }

    // Configure virtual keys.
  
    if ((device->classes & INPUT_DEVICE_CLASS_TOUCH)) {
        // Load the virtual keys for the touch screen, if any.
        // We do this now so that we can make sure to load the keymap if necessary.
        /**
         *  虚拟按键加载，
        */
        status_t status = loadVirtualKeyMapLocked(device);
        if (!status) {
            device->classes |= INPUT_DEVICE_CLASS_KEYBOARD;
        }
    }

    // Load the key map.
    // We need to do this for joysticks too because the key layout may specify axes.
    status_t keyMapStatus = NAME_NOT_FOUND;
    /**
     * frameworks/native/services/inputflinger/EventHub.h:103:    INPUT_DEVICE_CLASS_KEYBOARD      = 0x00000001,
     * frameworks/native/services/inputflinger/EventHub.h:127:    INPUT_DEVICE_CLASS_JOYSTICK      = 0x00000100,
    */
    if (device->classes & (INPUT_DEVICE_CLASS_KEYBOARD | INPUT_DEVICE_CLASS_JOYSTICK)) {
        // Load the keymap for the device.
        keyMapStatus = loadKeyMapLocked(device);
    }

    // Configure the keyboard, gamepad or virtual keyboard.
    if (device->classes & INPUT_DEVICE_CLASS_KEYBOARD) {
        // Register the keyboard as a built-in keyboard if it is eligible.
        if (!keyMapStatus
                && mBuiltInKeyboardId == NO_BUILT_IN_KEYBOARD
                && isEligibleBuiltInKeyboard(device->identifier,device->configuration, &device->keyMap)) {
            mBuiltInKeyboardId = device->id;
        }

        // 'Q' key support = cheap test of whether this is an alpha-capable kbd
        if (hasKeycodeLocked(device, AKEYCODE_Q)) {
            device->classes |= INPUT_DEVICE_CLASS_ALPHAKEY;
        }

        // See if this device has a DPAD.
        if (hasKeycodeLocked(device, AKEYCODE_DPAD_UP) &&
                hasKeycodeLocked(device, AKEYCODE_DPAD_DOWN) &&
                hasKeycodeLocked(device, AKEYCODE_DPAD_LEFT) &&
                hasKeycodeLocked(device, AKEYCODE_DPAD_RIGHT) &&
                hasKeycodeLocked(device, AKEYCODE_DPAD_CENTER)) {
            device->classes |= INPUT_DEVICE_CLASS_DPAD;
        }

        // See if this device has a gamepad.
        for (size_t i = 0; i < sizeof(GAMEPAD_KEYCODES)/sizeof(GAMEPAD_KEYCODES[0]); i++) {
            if (hasKeycodeLocked(device, GAMEPAD_KEYCODES[i])) {
                device->classes |= INPUT_DEVICE_CLASS_GAMEPAD;
                break;
            }
        }
    }

    // If the device isn't recognized as something we handle, don't monitor it.
    if (device->classes == 0) {
        ALOGV("Dropping device: id=%d, path='%s', name='%s'",deviceId, devicePath, device->identifier.name.string());
        delete device;
        return -1;
    }

    // Determine whether the device has a mic.
    if (deviceHasMicLocked(device)) {
        device->classes |= INPUT_DEVICE_CLASS_MIC;
    }

    // Determine whether the device is external or internal.
    if (isExternalDeviceLocked(device)) {
        device->classes |= INPUT_DEVICE_CLASS_EXTERNAL;
    }

    if (device->classes & (INPUT_DEVICE_CLASS_JOYSTICK | INPUT_DEVICE_CLASS_DPAD)
            && device->classes & INPUT_DEVICE_CLASS_GAMEPAD) {
        device->controllerNumber = getNextControllerNumberLocked(device);
        setLedForControllerLocked(device);
    }


    if (registerDeviceForEpollLocked(device) != OK) {
        delete device;
        return -1;
    }

    configureFd(device);

    ALOGI("New device: id=%d, fd=%d, path='%s', name='%s', classes=0x%x, "
            "configuration='%s', keyLayout='%s', keyCharacterMap='%s', builtinKeyboard=%s, ",
         deviceId, fd, devicePath, device->identifier.name.string(),
         device->classes,
         device->configurationFile.string(),
         device->keyMap.keyLayoutFile.string(),
         device->keyMap.keyCharacterMapFile.string(),
         toString(mBuiltInKeyboardId == deviceId));

    addDeviceLocked(device);
    return OK;
}


void EventHub::loadConfigurationLocked(Device* device) {
    /**
     * frameworks/native/include/input/InputDevice.h:138:    
     * INPUT_DEVICE_CONFIGURATION_FILE_TYPE_CONFIGURATION = 0,     // .idc file 
     * 
     * 
     * 获取路径
     * 首先查找 /odm/usr/idc,/vendor/usr/idc,/system/usr/idc 中的 .idc 后缀文件
     * 如果上述目录没有对应文件，则查找/data/system/device/idc 中的 .idc 后缀文件
     * 
     * 这里 /vendor/usr/idc 和 /system/usr/idc 存在
     * 
     * 这里 会拼凑 .idc 文件名 如果 deviceIdentifier.vendor 和 deviceIdentifier.product 不为空 
     * 且deviceIdentifier.version不为空则 Vendor_%04x_Product_%04x_Version_%04x
     * 如果deviceIdentifier.version为空 Vendor_%04x_Product_%04x
     * 
     * 
     * 如果 deviceIdentifier.vendor 和 deviceIdentifier.product 为空，
     * 则以 deviceIdentifier.name 为文件名
    */
    device->configurationFile = getInputDeviceConfigurationFilePathByDeviceIdentifier(device->identifier, INPUT_DEVICE_CONFIGURATION_FILE_TYPE_CONFIGURATION);
    if (device->configurationFile.isEmpty()) {
        ALOGD("No input device configuration file found for device '%s'.",device->identifier.name.string());
    } else {
        /**
         * PropertyMap* configuration;
        */
        status_t status = PropertyMap::load(device->configurationFile,&device->configuration);
        if (status) {
            ALOGE("Error loading input device configuration file for device '%s'.  " "Using default configuration.",device->identifier.name.string());
        }
    }
}



status_t EventHub::loadVirtualKeyMapLocked(Device* device) {
    /**
     * 
     * 虚拟按键加载，目前该文件不存在
    */
    // The virtual key map is supplied by the kernel as a system board property file.
    String8 path;
    path.append("/sys/board_properties/virtualkeys.");
    path.append(device->identifier.name);
    if (access(path.string(), R_OK)) {
        return NAME_NOT_FOUND;
    }
    return VirtualKeyMap::load(path, &device->virtualKeyMap);
}



//////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
 * -->size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) 
 * ---> EventHub::scanDevicesLocked()
 * ----> 
*/
void EventHub::createVirtualKeyboardLocked() {
    InputDeviceIdentifier identifier;
    identifier.name = "Virtual";
    identifier.uniqueId = "<virtual>";
    assignDescriptorLocked(identifier);

    Device* device = new Device(-1, VIRTUAL_KEYBOARD_ID, String8("<virtual>"), identifier);
    device->classes = INPUT_DEVICE_CLASS_KEYBOARD
            | INPUT_DEVICE_CLASS_ALPHAKEY
            | INPUT_DEVICE_CLASS_DPAD
            | INPUT_DEVICE_CLASS_VIRTUAL;
    loadKeyMapLocked(device);
    addDeviceLocked(device);
}

void EventHub::assignDescriptorLocked(InputDeviceIdentifier& identifier) {
    // Compute a device descriptor that uniquely identifies the device.
    // The descriptor is assumed to be a stable identifier.  Its value should not
    // change between reboots, reconnections, firmware updates or new releases
    // of Android. In practice we sometimes get devices that cannot be uniquely
    // identified. In this case we enforce uniqueness between connected devices.
    // Ideally, we also want the descriptor to be short and relatively opaque.

    identifier.nonce = 0;
    /**
     * generateDescriptor @  frameworks/native/services/inputflinger/EventHub.cpp
     * 给 identifier.descriptor 赋值
    */
    String8 rawDescriptor = generateDescriptor(identifier); //  
    if (identifier.uniqueId.isEmpty()) { //"<virtual>"
        // If it didn't have a unique id check for conflicts and enforce
        // uniqueness if necessary.
        while(getDeviceByDescriptorLocked(identifier.descriptor) != NULL) {
            identifier.nonce++;
            rawDescriptor = generateDescriptor(identifier);
        }
    }
    ALOGV("Created descriptor: raw=%s, cooked=%s", rawDescriptor.string(),identifier.descriptor.string());
}

status_t EventHub::loadKeyMapLocked(Device* device) {
    /**
     * @    frameworks/native/libs/input/Keyboard.cpp
     * 
    */
    return device->keyMap.load(device->identifier, device->configuration);
}

void EventHub::addDeviceLocked(Device* device) {
    mDevices.add(device->id, device);
    device->next = mOpeningDevices;
    mOpeningDevices = device;
}










int32_t EventHub::getDeviceControllerNumber(int32_t deviceId) const {
    AutoMutex _l(mLock);
    Device* device = getDeviceLocked(deviceId);
    if (device == NULL) return 0;
    return device->controllerNumber;
}
