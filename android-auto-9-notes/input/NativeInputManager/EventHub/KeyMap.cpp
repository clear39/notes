
status_t KeyMap::load(const InputDeviceIdentifier& deviceIdenfifier,const PropertyMap* deviceConfiguration) {
    // Use the configured key layout if available.
    /**
     * 如果为虚拟键盘这里 deviceConfiguration 为 NULL
     * 
     * 
    */
    if (deviceConfiguration) {
        String8 keyLayoutName;
        if (deviceConfiguration->tryGetProperty(String8("keyboard.layout"),keyLayoutName)) {
            status_t status = loadKeyLayout(deviceIdenfifier, keyLayoutName);
            if (status == NAME_NOT_FOUND) {
                ALOGE("Configuration for keyboard device '%s' requested keyboard layout '%s' but " "it was not found.", deviceIdenfifier.name.string(), keyLayoutName.string());
            }
        }

        String8 keyCharacterMapName;
        if (deviceConfiguration->tryGetProperty(String8("keyboard.characterMap"),keyCharacterMapName)) {
            status_t status = loadKeyCharacterMap(deviceIdenfifier, keyCharacterMapName);
            if (status == NAME_NOT_FOUND) {
                ALOGE("Configuration for keyboard device '%s' requested keyboard character " "map '%s' but it was not found.", deviceIdenfifier.name.string(), keyLayoutName.string());
            }
        }

        if (isComplete()) {
            return OK;
        }
    }

    // Try searching by device identifier.
    if (probeKeyMap(deviceIdenfifier, String8::empty())) {
        return OK;
    }

    // Fall back on the Generic key map.
    // TODO Apply some additional heuristics here to figure out what kind of
    //      generic key map to use (US English, etc.) for typical external keyboards.
    if (probeKeyMap(deviceIdenfifier, String8("Generic"))) {
        return OK;
    }

    // Try the Virtual key map as a last resort.
    if (probeKeyMap(deviceIdenfifier, String8("Virtual"))) {
        return OK;
    }

    // Give up!
    ALOGE("Could not determine key map for device '%s' and no default key maps were found!", deviceIdenfifier.name.string());

    return NAME_NOT_FOUND;
}


bool KeyMap::probeKeyMap(const InputDeviceIdentifier& deviceIdentifier,const String8& keyMapName) {
    if (!haveKeyLayout()) {
        loadKeyLayout(deviceIdentifier, keyMapName);
    }
    if (!haveKeyCharacterMap()) {
        loadKeyCharacterMap(deviceIdentifier, keyMapName);
    }
    return isComplete();
}

inline bool KeyMap::haveKeyLayout() const {
    return !keyLayoutFile.isEmpty();
}

status_t KeyMap::loadKeyLayout(const InputDeviceIdentifier& deviceIdentifier,const String8& name) {
    /**
     * frameworks/native/include/input/InputDevice.h:139:    
     * INPUT_DEVICE_CONFIGURATION_FILE_TYPE_KEY_LAYOUT = 1,        // .kl file 
     *
     * 这里name为空是时:
     * 
     * 获取路径
     * 首先查找 /odm/usr/keylayout,/vendor/usr/keylayout,/system/usr/keylayout 中的 .kl 后缀文件
     * 如果上述目录没有对应文件，则查找/data/system/device/keylayout 中的 .kl 后缀文件
     * 
     * 这里 /vendor/usr/keylayout 和 /system/usr/keylayout 存在
     * 
     * 这里 会拼凑 .kl文件名 如果 deviceIdentifier.vendor 和 deviceIdentifier.product 不为空 
     * 且deviceIdentifier.version不为空则 Vendor_%04x_Product_%04x_Version_%04x
     * 如果deviceIdentifier.version为空 Vendor_%04x_Product_%04x
     * 
     * 
     * 如果 deviceIdentifier.vendor 和 deviceIdentifier.product 为空，
     * 则以 deviceIdentifier.name 为文件名
     * 
     * 
     * 
     * 如果这里name不为空时，则直接去以上目录中查找以name为文件名的.kl文件
     * 
    */
    String8 path(getPath(deviceIdentifier, name,INPUT_DEVICE_CONFIGURATION_FILE_TYPE_KEY_LAYOUT));
    if (path.isEmpty()) {
        return NAME_NOT_FOUND;
    }

    status_t status = KeyLayoutMap::load(path, &keyLayoutMap);
    if (status) {
        return status;
    }
    /**
     * 保存对应文件路径到 keyLayoutFile
    */
    keyLayoutFile.setTo(path);
    return OK;
}


inline bool KeyMap::haveKeyCharacterMap() const {
    return !keyCharacterMapFile.isEmpty();
}

status_t KeyMap::loadKeyCharacterMap(const InputDeviceIdentifier& deviceIdentifier,const String8& name) {
    /**
     * frameworks/native/include/input/InputDevice.h:140:    
     * INPUT_DEVICE_CONFIGURATION_FILE_TYPE_KEY_CHARACTER_MAP = 2, // .kcm file 
     * 
     *
     * 这里name为空是时:
     * 
     * 获取路径
     * 首先查找 /odm/usr/keychars,/vendor/usr/keychars,/system/usr/keychars 中的 .kcm 后缀文件
     * 如果上述目录没有对应文件，则查找/data/system/device/keychars 中的 .kcm 后缀文件
     * 
     * 这里 /vendor/usr/keychars 和 /system/usr/keychars 存在
     * 
     * 这里 会拼凑 .kcm 文件名 如果 deviceIdentifier.vendor 和 deviceIdentifier.product 不为空 
     * 且deviceIdentifier.version不为空则 Vendor_%04x_Product_%04x_Version_%04x
     * 如果deviceIdentifier.version为空 Vendor_%04x_Product_%04x
     * 
     * 
     * 如果 deviceIdentifier.vendor 和 deviceIdentifier.product 为空，
     * 则以 deviceIdentifier.name 为文件名
     * 
     * 
     * 
     * 如果这里name不为空时，则直接去以上目录中查找以name为文件名的.kcm文件
     * 
    */
    String8 path(getPath(deviceIdentifier, name,INPUT_DEVICE_CONFIGURATION_FILE_TYPE_KEY_CHARACTER_MAP));
    if (path.isEmpty()) {
        return NAME_NOT_FOUND;
    }

    status_t status = KeyCharacterMap::load(path,KeyCharacterMap::FORMAT_BASE, &keyCharacterMap);
    if (status) {
        return status;
    }

     /**
     * 保存对应文件路径到 keyLayoutFile
    */
    keyCharacterMapFile.setTo(path);
    return OK;
}

inline bool KeyMap::isComplete() const {
    return haveKeyLayout() && haveKeyCharacterMap();
}
