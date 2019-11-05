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