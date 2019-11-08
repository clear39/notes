
//  @   frameworks/native/libs/input/VirtualKeyMap.cpp

status_t VirtualKeyMap::load(const String8& filename, VirtualKeyMap** outMap) {
    *outMap = NULL;

    Tokenizer* tokenizer;
    status_t status = Tokenizer::open(filename, &tokenizer);
    if (status) {
        ALOGE("Error %d opening virtual key map file %s.", status, filename.string());
    } else {
        VirtualKeyMap* map = new VirtualKeyMap();
        if (!map) {
            ALOGE("Error allocating virtual key map.");
            status = NO_MEMORY;
        } else {
#if DEBUG_PARSER_PERFORMANCE
            nsecs_t startTime = systemTime(SYSTEM_TIME_MONOTONIC);
#endif
            Parser parser(map, tokenizer);
            status = parser.parse();
#if DEBUG_PARSER_PERFORMANCE
            nsecs_t elapsedTime = systemTime(SYSTEM_TIME_MONOTONIC) - startTime;
            ALOGD("Parsed key character map file '%s' %d lines in %0.3fms.",
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


VirtualKeyMap::Parser::Parser(VirtualKeyMap* map, Tokenizer* tokenizer) :
        mMap(map), mTokenizer(tokenizer) {
}

status_t VirtualKeyMap::Parser::parse() {
    while (!mTokenizer->isEof()) {
#if DEBUG_PARSER
        ALOGD("Parsing %s: '%s'.", mTokenizer->getLocation().string(),
                mTokenizer->peekRemainderOfLine().string());
#endif

        mTokenizer->skipDelimiters(WHITESPACE);

        if (!mTokenizer->isEol() && mTokenizer->peekChar() != '#') {
            // Multiple keys can appear on one line or they can be broken up across multiple lines.
            do {
                String8 token = mTokenizer->nextToken(WHITESPACE_OR_FIELD_DELIMITER);
                if (token != "0x01") {
                    ALOGE("%s: Unknown virtual key type, expected 0x01.",mTokenizer->getLocation().string());
                    return BAD_VALUE;
                }

                VirtualKeyDefinition defn;
                bool success = parseNextIntField(&defn.scanCode)
                        && parseNextIntField(&defn.centerX)
                        && parseNextIntField(&defn.centerY)
                        && parseNextIntField(&defn.width)
                        && parseNextIntField(&defn.height);
                if (!success) {
                    ALOGE("%s: Expected 5 colon-delimited integers in virtual key definition.",mTokenizer->getLocation().string());
                    return BAD_VALUE;
                }

#if DEBUG_PARSER
                ALOGD("Parsed virtual key: scanCode=%d, centerX=%d, centerY=%d, "
                        "width=%d, height=%d",
                        defn.scanCode, defn.centerX, defn.centerY, defn.width, defn.height);
#endif
                mMap->mVirtualKeys.push(defn);
            } while (consumeFieldDelimiterAndSkipWhitespace());

            if (!mTokenizer->isEol()) {
                ALOGE("%s: Expected end of line, got '%s'.",
                        mTokenizer->getLocation().string(),
                        mTokenizer->peekRemainderOfLine().string());
                return BAD_VALUE;
            }
        }

        mTokenizer->nextLine();
    }

    return NO_ERROR;
}