//  @frameworks/av/media/libstagefright/include/media/stagefright/FileSource.h
class FileSource : public DataSource {}


FileSource::FileSource(int fd, int64_t offset, int64_t length)
    : mFd(fd),
      mOffset(offset),
      mLength(length),
      mName("<null>"),
      mDecryptHandle(NULL),
      mDrmManagerClient(NULL),
      mDrmBufOffset(0),
      mDrmBufSize(0),
      mDrmBuf(NULL) {
    ALOGV("fd=%d (%s), offset=%lld, length=%lld", fd, nameForFd(fd).c_str(), (long long) offset, (long long) length);

    if (mOffset < 0) {
        mOffset = 0;
    }
    if (mLength < 0) {
        mLength = 0;
    }
    if (mLength > INT64_MAX - mOffset) {
        mLength = INT64_MAX - mOffset;
    }
    struct stat s;
    if (fstat(fd, &s) == 0) {
        if (mOffset > s.st_size) {
            mOffset = s.st_size;
            mLength = 0;
        }
        if (mOffset + mLength > s.st_size) {
            mLength = s.st_size - mOffset;
        }
    }
    if (mOffset != offset || mLength != length) {
        ALOGW("offset/length adjusted from %lld/%lld to %lld/%lld",(long long) offset, (long long) length, (long long) mOffset, (long long) mLength);
    }

    mName = String8::format( "FileSource(fd(%s), %lld, %lld)", nameForFd(fd).c_str(), (long long) mOffset, (long long) mLength);

}



virtual uint32_t FileSource::flags() {
    return kIsLocalFileSource;
}



// nameForFd @frameworks/av/media/libstagefright/Utils.cpp
AString nameForFd(int fd) {
    const size_t SIZE = 256;
    char buffer[SIZE];
    AString result;
    snprintf(buffer, SIZE, "/proc/%d/fd/%d", getpid(), fd);
    struct stat s;
    if (lstat(buffer, &s) == 0) {
        if ((s.st_mode & S_IFMT) == S_IFLNK) {
            char linkto[256];
            int len = readlink(buffer, linkto, sizeof(linkto));
            if(len > 0) {
                if(len > 255) {
                    linkto[252] = '.';
                    linkto[253] = '.';
                    linkto[254] = '.';
                    linkto[255] = 0;
                } else {
                    linkto[len] = 0;
                }
                result.append(linkto);
            }
        } else {
            result.append("unexpected type for ");
            result.append(buffer);
        }
    } else {
        result.append("couldn't open ");
        result.append(buffer);
    }
    return result;
}
