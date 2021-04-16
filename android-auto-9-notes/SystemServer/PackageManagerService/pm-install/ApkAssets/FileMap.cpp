
//	@	system/core/libutils/FileMap.cpp
// Constructor.  Create an empty object.
FileMap::FileMap(void)
    : mFileName(NULL),
      mBasePtr(NULL),
      mBaseLength(0),
      mDataPtr(NULL),
      mDataLength(0)
#if defined(__MINGW32__)
      ,
      mFileHandle(INVALID_HANDLE_VALUE),
      mFileMapping(NULL)
#endif
{
}


// Closing the file descriptor does not unmap the pages, so we don't
// claim ownership of the fd.
//
// Returns "false" on failure.
bool FileMap::create(const char* origFileName, int fd, off64_t offset, size_t length, bool readOnly)
{
#if defined(__MINGW32__)
    int     adjust;
    off64_t adjOffset;
    size_t  adjLength;

    if (mPageSize == -1) {
        SYSTEM_INFO  si;

        GetSystemInfo( &si );
        mPageSize = si.dwAllocationGranularity;
    }

    DWORD  protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;

    mFileHandle  = (HANDLE) _get_osfhandle(fd);
    mFileMapping = CreateFileMapping( mFileHandle, NULL, protect, 0, 0, NULL);
    if (mFileMapping == NULL) {
        ALOGE("CreateFileMapping(%p, %lx) failed with error %lu\n",
              mFileHandle, protect, GetLastError() );
        return false;
    }

    adjust    = offset % mPageSize;
    adjOffset = offset - adjust;
    adjLength = length + adjust;

    mBasePtr = MapViewOfFile( mFileMapping,
                              readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
                              0,
                              (DWORD)(adjOffset),
                              adjLength );
    if (mBasePtr == NULL) {
        ALOGE("MapViewOfFile(%" PRId64 ", %zu) failed with error %lu\n",
              adjOffset, adjLength, GetLastError() );
        CloseHandle(mFileMapping);
        mFileMapping = NULL;
        return false;
    }
#else // !defined(__MINGW32__)
	int     prot, flags, adjust;
    off64_t adjOffset;
    size_t  adjLength;

    void* ptr;

    assert(fd >= 0);
    assert(offset >= 0);
    assert(length > 0);

    // init on first use
    if (mPageSize == -1) {
        mPageSize = sysconf(_SC_PAGESIZE);
        if (mPageSize == -1) {
            ALOGE("could not get _SC_PAGESIZE\n");
            return false;
        }
    }

    adjust = offset % mPageSize;
    adjOffset = offset - adjust;
    adjLength = length + adjust;

    flags = MAP_SHARED;
    prot = PROT_READ;
    if (!readOnly)
        prot |= PROT_WRITE;

    ptr = mmap(NULL, adjLength, prot, flags, fd, adjOffset);
    if (ptr == MAP_FAILED) {
        ALOGE("mmap(%lld,%zu) failed: %s\n", (long long)adjOffset, adjLength, strerror(errno));
        return false;
    }
    mBasePtr = ptr;
#endif // !defined(__MINGW32__)

    mFileName = origFileName != NULL ? strdup(origFileName) : NULL;
    mBaseLength = adjLength;
    mDataOffset = offset;
    mDataPtr = (char*) mBasePtr + adjust;
    mDataLength = length;

    assert(mBasePtr != NULL);

    ALOGV("MAP: base %p/%zu data %p/%zu\n", mBasePtr, mBaseLength, mDataPtr, mDataLength);

    return true;
}

