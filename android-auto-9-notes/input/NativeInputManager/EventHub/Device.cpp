
//  @   frameworks/native/include/input/InputDevice.h
/*
 * Identifies a device.
 */
struct InputDeviceIdentifier {
    inline InputDeviceIdentifier() :
            bus(0), vendor(0), product(0), version(0) {
    }

    // Information provided by the kernel.
    String8 name;
    String8 location;
    String8 uniqueId;
    uint16_t bus;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;

    // A composite input device descriptor string that uniquely identifies the device
    // even across reboots or reconnections.  The value of this field is used by
    // upper layers of the input system to associate settings with individual devices.
    // It is hashed from whatever kernel provided information is available.
    // Ideally, the way this value is computed should not change between Android releases
    // because that would invalidate persistent settings that rely on it.
    String8 descriptor;

    // A value added to uniquely identify a device in the absence of a unique id. This
    // is intended to be a minimum way to distinguish from other active devices and may
    // reuse values that are not associated with an input anymore.
    uint16_t nonce;
};

//  @ frameworks/native/services/inputflinger/EventHub.h
struct Device {
    Device* next;

    int fd; // may be -1 if device is closed
    const int32_t id;
    const String8 path;
    const InputDeviceIdentifier identifier;

    uint32_t classes;

    uint8_t keyBitmask[(KEY_MAX + 1) / 8];
    uint8_t absBitmask[(ABS_MAX + 1) / 8];
    uint8_t relBitmask[(REL_MAX + 1) / 8];
    uint8_t swBitmask[(SW_MAX + 1) / 8];
    uint8_t ledBitmask[(LED_MAX + 1) / 8];
    uint8_t ffBitmask[(FF_MAX + 1) / 8];
    uint8_t propBitmask[(INPUT_PROP_MAX + 1) / 8];

    String8 configurationFile;
    PropertyMap* configuration;
    VirtualKeyMap* virtualKeyMap;
    KeyMap keyMap;

    sp<KeyCharacterMap> overlayKeyMap;
    sp<KeyCharacterMap> combinedKeyMap;

    bool ffEffectPlaying;
    int16_t ffEffectId; // initially -1

    int32_t controllerNumber;

    int32_t timestampOverrideSec;
    int32_t timestampOverrideUsec;

    Device(int fd, int32_t id, const String8& path, const InputDeviceIdentifier& identifier);
    ~Device();

    void close();

    bool enabled; // initially true
    status_t enable();
    status_t disable();
    bool hasValidFd();
    const bool isVirtual; // set if fd < 0 is passed to constructor

    const sp<KeyCharacterMap>& getKeyCharacterMap() const {
        if (combinedKeyMap != NULL) {
            return combinedKeyMap;
        }
        return keyMap.keyCharacterMap;
    }
};

/**
 * frameworks/native/services/inputflinger/EventHub.cpp
*/
EventHub::Device::Device(int fd, int32_t id, const String8& path,const InputDeviceIdentifier& identifier) :
        next(NULL),
        fd(fd), id(id), path(path), identifier(identifier),
        classes(0), configuration(NULL), virtualKeyMap(NULL),
        ffEffectPlaying(false), ffEffectId(-1), controllerNumber(0),
        timestampOverrideSec(0), timestampOverrideUsec(0), enabled(true),
        isVirtual(fd < 0) {
    memset(keyBitmask, 0, sizeof(keyBitmask));
    memset(absBitmask, 0, sizeof(absBitmask));
    memset(relBitmask, 0, sizeof(relBitmask));
    memset(swBitmask, 0, sizeof(swBitmask));
    memset(ledBitmask, 0, sizeof(ledBitmask));
    memset(ffBitmask, 0, sizeof(ffBitmask));
    memset(propBitmask, 0, sizeof(propBitmask));
}