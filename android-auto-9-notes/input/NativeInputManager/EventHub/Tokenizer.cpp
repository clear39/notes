//  @   system/core/libutils/Tokenizer.cpp
status_t Tokenizer::open(const String8& filename, Tokenizer** outTokenizer) {
    *outTokenizer = NULL;

    int result = NO_ERROR;
    int fd = ::open(filename.string(), O_RDONLY);
    if (fd < 0) {
        result = -errno;
        ALOGE("Error opening file '%s': %s", filename.string(), strerror(errno));
    } else {
        struct stat stat;
        if (fstat(fd, &stat)) {
            result = -errno;
            ALOGE("Error getting size of file '%s': %s", filename.string(), strerror(errno));
        } else {
            size_t length = size_t(stat.st_size);

            FileMap* fileMap = new FileMap();
            bool ownBuffer = false;
            char* buffer;
            if (fileMap->create(NULL, fd, 0, length, true)) {
                fileMap->advise(FileMap::SEQUENTIAL);
                buffer = static_cast<char*>(fileMap->getDataPtr());
            } else {
                delete fileMap;
                fileMap = NULL;

                // Fall back to reading into a buffer since we can't mmap files in sysfs.
                // The length we obtained from stat is wrong too (it will always be 4096)
                // so we must trust that read will read the entire file.
                buffer = new char[length];
                ownBuffer = true;
                ssize_t nrd = read(fd, buffer, length);
                if (nrd < 0) {
                    result = -errno;
                    ALOGE("Error reading file '%s': %s", filename.string(), strerror(errno));
                    delete[] buffer;
                    buffer = NULL;
                } else {
                    length = size_t(nrd);
                }
            }

            if (!result) {
                *outTokenizer = new Tokenizer(filename, fileMap, buffer, ownBuffer, length);
            }
        }
        close(fd);
    }
    return result;
}