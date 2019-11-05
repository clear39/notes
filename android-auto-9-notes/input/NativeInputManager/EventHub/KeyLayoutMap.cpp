
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
