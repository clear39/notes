
//  @   frameworks/native/libs/input/KeyLayoutMap.cpp

status_t KeyLayoutMap::load(const String8& filename, sp<KeyLayoutMap>* outMap) {
    outMap->clear();

    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(filename, &tokenizer);
    if (status) {
        ALOGE("Error %d opening key layout map file %s.", status, filename.string());
    } else {
        sp<KeyLayoutMap> map = new KeyLayoutMap();
        if (!map.get()) {
            ALOGE("Error allocating key layout map.");
            status = NO_MEMORY;
        } else {
#if DEBUG_PARSER_PERFORMANCE
            nsecs_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
            Parser parser(map.get(), tokenizer);
            status = parser.parse();
#if DEBUG_PARSER_PERFORMANCE
            nsecs_t elapsedTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
            ALOGD("Parsed key layout map file '%s' %d lines in %0.3fms.",
                    tokenizer->getFilename().string(), tokenizer->getLineNumber(),
                    elapsedTime / 1000000.0);
#endif
            if (!status) {
                *outMap = map;
            }
        }
        delete tokenizer;
    }
    return status;
}


status_t KeyLayoutMap::mapKey(int32_t scanCode, int32_t usageCode,int32_t* outKeyCode, uint32_t* outFlags) const {
    const Key* key = getKey(scanCode, usageCode);
    if (!key) {
#if DEBUG_MAPPING
        ALOGD("mapKey: scanCode=%d, usageCode=0x%08x ~ Failed.", scanCode, usageCode);
#endif
        *outKeyCode = AKEYCODE_UNKNOWN;
        *outFlags = 0;
        return NAME_NOT_FOUND;
    }

    *outKeyCode = key->keyCode;
    *outFlags = key->flags;

#if DEBUG_MAPPING
    ALOGD("mapKey: scanCode=%d, usageCode=0x%08x ~ Result keyCode=%d, outFlags=0x%08x.",
            scanCode, usageCode, *outKeyCode, *outFlags);
#endif
    return NO_ERROR;
}

const KeyLayoutMap::Key* KeyLayoutMap::getKey(int32_t scanCode, int32_t usageCode) const {
    if (usageCode) {
        ssize_t index = mKeysByUsageCode.indexOfKey(usageCode);
        if (index >= 0) {
            return &mKeysByUsageCode.valueAt(index);
        }
    }
    if (scanCode) {
        ssize_t index = mKeysByScanCode.indexOfKey(scanCode);
        if (index >= 0) {
            return &mKeysByScanCode.valueAt(index);
        }
    }
    return NULL;
}