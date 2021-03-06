//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/native/libs/input/KeyCharacterMap.cpp
status_t KeyCharacterMap::load(const String8& filename,Format format, sp<KeyCharacterMap>* outMap) {
    outMap->clear();

    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(filename, &tokenizer);
    if (status) {
        ALOGE("Error %d opening key character map file %s.", status, filename.string());
    } else {
        status = load(tokenizer, format, outMap);
        delete tokenizer;
    }
    return status;
}

status_t KeyCharacterMap::load(Tokenizer* tokenizer,Format format, sp<KeyCharacterMap>* outMap) {
    status_t status = OK;
    sp<KeyCharacterMap> map = new KeyCharacterMap();
    if (!map.get()) {
        ALOGE("Error allocating key character map.");
        status = NO_MEMORY;
    } else {
#if DEBUG_PARSER_PERFORMANCE
        nsecs_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
        Parser parser(map.get(), tokenizer, format);
        status = parser.parse();
#if DEBUG_PARSER_PERFORMANCE
        nsecs_t elapsedTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
        ALOGD("Parsed key character map file '%s' %d lines in %0.3fms.",
                tokenizer->getFilename().string(), tokenizer->getLineNumber(),
                elapsedTime / 1000000.0);
#endif
        if (!status) {
            *outMap = map;
        }
    }
    return status;
}


status_t KeyLayoutMap::Parser::parse() {
    while (!mTokenizer->isEof()) {
#if DEBUG_PARSER
        ALOGD("Parsing %s: '%s'.", mTokenizer->getLocation().string(),mTokenizer->peekRemainderOfLine().string());
#endif

        mTokenizer->skipDelimiters(WHITESPACE);

        if (!mTokenizer->isEol() && mTokenizer->peekChar() != '#') {
            String8 keywordToken = mTokenizer->nextToken(WHITESPACE);
            if (keywordToken == "key") {
                mTokenizer->skipDelimiters(WHITESPACE);
                status_t status = parseKey();
                if (status) return status;
            } else if (keywordToken == "axis") {
                mTokenizer->skipDelimiters(WHITESPACE);
                status_t status = parseAxis();
                if (status) return status;
            } else if (keywordToken == "led") {
                mTokenizer->skipDelimiters(WHITESPACE);
                status_t status = parseLed();
                if (status) return status;
            } else {
                ALOGE("%s: Expected keyword, got '%s'.", mTokenizer->getLocation().string(),keywordToken.string());
                return BAD_VALUE;
            }

            mTokenizer->skipDelimiters(WHITESPACE);
            if (!mTokenizer->isEol() && mTokenizer->peekChar() != '#') {
                ALOGE("%s: Expected end of line or trailing comment, got '%s'.",mTokenizer->getLocation().string(),mTokenizer->peekRemainderOfLine().string());
                return BAD_VALUE;
            }
        }

        mTokenizer->nextLine();
    }
    return NO_ERROR;
}


status_t KeyLayoutMap::Parser::parseKey() {
    String8 codeToken = mTokenizer->nextToken(WHITESPACE);
    bool mapUsage = false;
    if (codeToken == "usage") {
        mapUsage = true;
        mTokenizer->skipDelimiters(WHITESPACE);
        codeToken = mTokenizer->nextToken(WHITESPACE);
    }

    char* end;
    int32_t code = int32_t(strtol(codeToken.string(), &end, 0));
    if (*end) {
        ALOGE("%s: Expected key %s number, got '%s'.", mTokenizer->getLocation().string(),
                mapUsage ? "usage" : "scan code", codeToken.string());
        return BAD_VALUE;
    }
    KeyedVector<int32_t, Key>& map = mapUsage ? mMap->mKeysByUsageCode : mMap->mKeysByScanCode;
    if (map.indexOfKey(code) >= 0) {
        ALOGE("%s: Duplicate entry for key %s '%s'.", mTokenizer->getLocation().string(),
                mapUsage ? "usage" : "scan code", codeToken.string());
        return BAD_VALUE;
    }

    mTokenizer->skipDelimiters(WHITESPACE);
    String8 keyCodeToken = mTokenizer->nextToken(WHITESPACE);
    int32_t keyCode = getKeyCodeByLabel(keyCodeToken.string());
    if (!keyCode) {
        ALOGE("%s: Expected key code label, got '%s'.", mTokenizer->getLocation().string(),
                keyCodeToken.string());
        return BAD_VALUE;
    }

    uint32_t flags = 0;
    for (;;) {
        mTokenizer->skipDelimiters(WHITESPACE);
        if (mTokenizer->isEol() || mTokenizer->peekChar() == '#') break;

        String8 flagToken = mTokenizer->nextToken(WHITESPACE);
        uint32_t flag = getKeyFlagByLabel(flagToken.string());
        if (!flag) {
            ALOGE("%s: Expected key flag label, got '%s'.", mTokenizer->getLocation().string(),
                    flagToken.string());
            return BAD_VALUE;
        }
        if (flags & flag) {
            ALOGE("%s: Duplicate key flag '%s'.", mTokenizer->getLocation().string(),
                    flagToken.string());
            return BAD_VALUE;
        }
        flags |= flag;
    }

#if DEBUG_PARSER
    ALOGD("Parsed key %s: code=%d, keyCode=%d, flags=0x%08x.",
            mapUsage ? "usage" : "scan code", code, keyCode, flags);
#endif
    Key key;
    key.keyCode = keyCode;
    key.flags = flags;
    map.add(code, key);
    return NO_ERROR;
}


status_t KeyLayoutMap::Parser::parseAxis() {
    String8 scanCodeToken = mTokenizer->nextToken(WHITESPACE);
    char* end;
    int32_t scanCode = int32_t(strtol(scanCodeToken.string(), &end, 0));
    if (*end) {
        ALOGE("%s: Expected axis scan code number, got '%s'.", mTokenizer->getLocation().string(),
                scanCodeToken.string());
        return BAD_VALUE;
    }
    if (mMap->mAxes.indexOfKey(scanCode) >= 0) {
        ALOGE("%s: Duplicate entry for axis scan code '%s'.", mTokenizer->getLocation().string(),
                scanCodeToken.string());
        return BAD_VALUE;
    }

    AxisInfo axisInfo;

    mTokenizer->skipDelimiters(WHITESPACE);
    String8 token = mTokenizer->nextToken(WHITESPACE);
    if (token == "invert") {
        axisInfo.mode = AxisInfo::MODE_INVERT;

        mTokenizer->skipDelimiters(WHITESPACE);
        String8 axisToken = mTokenizer->nextToken(WHITESPACE);
        axisInfo.axis = getAxisByLabel(axisToken.string());
        if (axisInfo.axis < 0) {
            ALOGE("%s: Expected inverted axis label, got '%s'.",
                    mTokenizer->getLocation().string(), axisToken.string());
            return BAD_VALUE;
        }
    } else if (token == "split") {
        axisInfo.mode = AxisInfo::MODE_SPLIT;

        mTokenizer->skipDelimiters(WHITESPACE);
        String8 splitToken = mTokenizer->nextToken(WHITESPACE);
        axisInfo.splitValue = int32_t(strtol(splitToken.string(), &end, 0));
        if (*end) {
            ALOGE("%s: Expected split value, got '%s'.",
                    mTokenizer->getLocation().string(), splitToken.string());
            return BAD_VALUE;
        }

        mTokenizer->skipDelimiters(WHITESPACE);
        String8 lowAxisToken = mTokenizer->nextToken(WHITESPACE);
        axisInfo.axis = getAxisByLabel(lowAxisToken.string());
        if (axisInfo.axis < 0) {
            ALOGE("%s: Expected low axis label, got '%s'.",
                    mTokenizer->getLocation().string(), lowAxisToken.string());
            return BAD_VALUE;
        }

        mTokenizer->skipDelimiters(WHITESPACE);
        String8 highAxisToken = mTokenizer->nextToken(WHITESPACE);
        axisInfo.highAxis = getAxisByLabel(highAxisToken.string());
        if (axisInfo.highAxis < 0) {
            ALOGE("%s: Expected high axis label, got '%s'.",
                    mTokenizer->getLocation().string(), highAxisToken.string());
            return BAD_VALUE;
        }
    } else {
        axisInfo.axis = getAxisByLabel(token.string());
        if (axisInfo.axis < 0) {
            ALOGE("%s: Expected axis label, 'split' or 'invert', got '%s'.",
                    mTokenizer->getLocation().string(), token.string());
            return BAD_VALUE;
        }
    }

    for (;;) {
        mTokenizer->skipDelimiters(WHITESPACE);
        if (mTokenizer->isEol() || mTokenizer->peekChar() == '#') {
            break;
        }
        String8 keywordToken = mTokenizer->nextToken(WHITESPACE);
        if (keywordToken == "flat") {
            mTokenizer->skipDelimiters(WHITESPACE);
            String8 flatToken = mTokenizer->nextToken(WHITESPACE);
            axisInfo.flatOverride = int32_t(strtol(flatToken.string(), &end, 0));
            if (*end) {
                ALOGE("%s: Expected flat value, got '%s'.",
                        mTokenizer->getLocation().string(), flatToken.string());
                return BAD_VALUE;
            }
        } else {
            ALOGE("%s: Expected keyword 'flat', got '%s'.",
                    mTokenizer->getLocation().string(), keywordToken.string());
            return BAD_VALUE;
        }
    }

#if DEBUG_PARSER
    ALOGD("Parsed axis: scanCode=%d, mode=%d, axis=%d, highAxis=%d, "
            "splitValue=%d, flatOverride=%d.",
            scanCode,
            axisInfo.mode, axisInfo.axis, axisInfo.highAxis,
            axisInfo.splitValue, axisInfo.flatOverride);
#endif
    mMap->mAxes.add(scanCode, axisInfo);
    return NO_ERROR;
}



status_t KeyLayoutMap::Parser::parseLed() {
    String8 codeToken = mTokenizer->nextToken(WHITESPACE);
    bool mapUsage = false;
    if (codeToken == "usage") {
        mapUsage = true;
        mTokenizer->skipDelimiters(WHITESPACE);
        codeToken = mTokenizer->nextToken(WHITESPACE);
    }
    char* end;
    int32_t code = int32_t(strtol(codeToken.string(), &end, 0));
    if (*end) {
        ALOGE("%s: Expected led %s number, got '%s'.", mTokenizer->getLocation().string(),
                mapUsage ? "usage" : "scan code", codeToken.string());
        return BAD_VALUE;
    }

    KeyedVector<int32_t, Led>& map = mapUsage ? mMap->mLedsByUsageCode : mMap->mLedsByScanCode;
    if (map.indexOfKey(code) >= 0) {
        ALOGE("%s: Duplicate entry for led %s '%s'.", mTokenizer->getLocation().string(),
                mapUsage ? "usage" : "scan code", codeToken.string());
        return BAD_VALUE;
    }

    mTokenizer->skipDelimiters(WHITESPACE);
    String8 ledCodeToken = mTokenizer->nextToken(WHITESPACE);
    int32_t ledCode = getLedByLabel(ledCodeToken.string());
    if (ledCode < 0) {
        ALOGE("%s: Expected LED code label, got '%s'.", mTokenizer->getLocation().string(),
                ledCodeToken.string());
        return BAD_VALUE;
    }

#if DEBUG_PARSER
    ALOGD("Parsed led %s: code=%d, ledCode=%d.",
            mapUsage ? "usage" : "scan code", code, ledCode);
#endif

    Led led;
    led.ledCode = ledCode;
    map.add(code, led);
    return NO_ERROR;
}