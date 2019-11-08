//  @   system/core/libutils/PropertyMap.cpp
status_t PropertyMap::load(const String8& filename, PropertyMap** outMap) {
    *outMap = NULL;

    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(filename, &tokenizer);
    if (status) {
        ALOGE("Error %d opening property file %s.", status, filename.string());
    } else {
        PropertyMap* map = new PropertyMap();
        if (!map) {
            ALOGE("Error allocating property map.");
            status = NO_MEMORY;
        } else {
#if DEBUG_PARSER_PERFORMANCE
            nsecs_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
            Parser parser(map, tokenizer);
            status = parser.parse();
#if DEBUG_PARSER_PERFORMANCE
            nsecs_t elapsedTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
            ALOGD("Parsed property file '%s' %d lines in %0.3fms.",
                    tokenizer->getFilename().string(), tokenizer->getLineNumber(),
                    elapsedTime / 1000000.0);
#endif
            if (status) {
                delete map;
            } else {
                *outMap = map;
            }
        }
        delete tokenizer;
    }
    return status;
}


void PropertyMap::addProperty(const String8& key, const String8& value) {
    mProperties.add(key, value);
}

bool PropertyMap::tryGetProperty(const String8& key, String8& outValue) const {
    ssize_t index = mProperties.indexOfKey(key);
    if (index < 0) {
        return false;
    }

    outValue = mProperties.valueAt(index);
    return true;
}

bool PropertyMap::tryGetProperty(const String8& key, bool& outValue) const {
    int32_t intValue;
    if (!tryGetProperty(key, intValue)) {
        return false;
    }

    outValue = intValue;
    return true;
}




PropertyMap::Parser::Parser(PropertyMap* map, Tokenizer* tokenizer) :
        mMap(map), mTokenizer(tokenizer) {
}


status_t PropertyMap::Parser::parse() {
    while (!mTokenizer->isEof()) {
#if DEBUG_PARSER
        ALOGD("Parsing %s: '%s'.", mTokenizer->getLocation().string(),
                mTokenizer->peekRemainderOfLine().string());
#endif

        mTokenizer->skipDelimiters(WHITESPACE);

        if (!mTokenizer->isEol() && mTokenizer->peekChar() != '#') {
            /**
             * system/core/libutils/PropertyMap.cpp:31:
             * static const char* WHITESPACE_OR_PROPERTY_DELIMITER = " \t\r=";
            */
            String8 keyToken = mTokenizer->nextToken(WHITESPACE_OR_PROPERTY_DELIMITER);
            if (keyToken.isEmpty()) {
                ALOGE("%s: Expected non-empty property key.", mTokenizer->getLocation().string());
                return BAD_VALUE;
            }

            mTokenizer->skipDelimiters(WHITESPACE);

            if (mTokenizer->nextChar() != '=') {
                ALOGE("%s: Expected '=' between property key and value.",mTokenizer->getLocation().string());
                return BAD_VALUE;
            }

            mTokenizer->skipDelimiters(WHITESPACE);

            String8 valueToken = mTokenizer->nextToken(WHITESPACE);
            if (valueToken.find("\\", 0) >= 0 || valueToken.find("\"", 0) >= 0) {
                ALOGE("%s: Found reserved character '\\' or '\"' in property value.",
                        mTokenizer->getLocation().string());
                return BAD_VALUE;
            }

            mTokenizer->skipDelimiters(WHITESPACE);
            if (!mTokenizer->isEol()) {
                ALOGE("%s: Expected end of line, got '%s'.",
                        mTokenizer->getLocation().string(),
                        mTokenizer->peekRemainderOfLine().string());
                return BAD_VALUE;
            }

            if (mMap->hasProperty(keyToken)) {
                ALOGE("%s: Duplicate property value for key '%s'.",
                        mTokenizer->getLocation().string(), keyToken.string());
                return BAD_VALUE;
            }

            mMap->addProperty(keyToken, valueToken);
        }

        mTokenizer->nextLine();
    }
    return NO_ERROR;
}