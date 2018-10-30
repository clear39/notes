//	@frameworks/base/libs/androidfw/AssetManager.cpp

AssetManager::AssetManager() :mLocale(NULL), mResources(NULL), mConfig(new ResTable_config) {
    int count = android_atomic_inc(&gCount) + 1;
    if (kIsDebug) {
        ALOGI("Creating AssetManager %p #%d\n", this, count);
    }
    memset(mConfig, 0, sizeof(ResTable_config));
}


bool AssetManager::addDefaultAssets()
{
    const char* root = getenv("ANDROID_ROOT");//    /system
    LOG_ALWAYS_FATAL_IF(root == NULL, "ANDROID_ROOT not set");

    String8 path(root);
    path.appendPath(kSystemAssets);//	static const char* kSystemAssets = "framework/framework-res.apk";

    //			path = "/system/framework/framework-res.apk"
    return addAssetPath(path, NULL, false /* appAsLib */, true /* isSystemAsset */);
}

//  @frameworks/base/libs/androidfw/include/androidfw/AssetManager.h
struct asset_path
{
    asset_path() : path(""), type(kFileTypeRegular), idmap(""),
                   isSystemOverlay(false), isSystemAsset(false) {}
    String8 path;
    FileType type;
    String8 idmap;
    bool isSystemOverlay;
    bool isSystemAsset;
};


bool AssetManager::addAssetPath(const String8& path, int32_t* cookie = NULL, bool appAsLib = false, bool isSystemAsset = true) {
    AutoMutex _l(mLock);

    asset_path ap;

    String8 realPath(path);//			realPath = "/system/framework/framework-res.apk"
    if (kAppZipName) {//	static const char* kAppZipName = NULL; //"classes.jar";
        realPath.appendPath(kAppZipName);
    }

    //  @frameworks/base/libs/androidfw/misc.cpp
    ap.type = ::getFileType(realPath.string()); //返回kFileTypeRegular
    if (ap.type == kFileTypeRegular) {
        ap.path = realPath;
    } else {
        ap.path = path;
        ap.type = ::getFileType(path.string());
        if (ap.type != kFileTypeDirectory && ap.type != kFileTypeRegular) {
            ALOGW("Asset path %s is neither a directory nor file (type=%d).",path.string(), (int)ap.type);
            return false;
        }
    }

    // Skip if we have it already.
    for (size_t i=0; i<mAssetPaths.size(); i++) {
        if (mAssetPaths[i].path == ap.path) {
            if (cookie) {
                *cookie = static_cast<int32_t>(i+1);
            }
            return true;
        }
    }

    ALOGV("In %p Asset %s path: %s", this,ap.type == kFileTypeDirectory ? "dir" : "zip", ap.path.string());

    ap.isSystemAsset = isSystemAsset;
    mAssetPaths.add(ap);

    // new paths are always added at the end
    if (cookie) {// NULL
        *cookie = static_cast<int32_t>(mAssetPaths.size());
    }

#ifdef __ANDROID__
    // Load overlays, if any
    asset_path oap;
    //ZipSet mZipSet; @frameworks/base/libs/androidfw/include/androidfw/AssetManager.h
    for (size_t idx = 0; mZipSet.getOverlay(ap.path, idx, &oap); idx++) {
        oap.isSystemAsset = isSystemAsset;
        mAssetPaths.add(oap);
    }
#endif

    if (mResources != NULL) {
        appendPathToResTable(ap, appAsLib);
    }

    return true;
}

bool AssetManager::ZipSet::getOverlay(const String8& path, size_t idx, asset_path* out) const
{
    sp<SharedZip> zip = SharedZip::get(path, false);
    if (zip == NULL) {
        return false;
    }
    return zip->getOverlay(idx, out);
}

sp<AssetManager::SharedZip> AssetManager::SharedZip::get(const String8& path,bool createIfNotPresent)
{
    AutoMutex _l(gLock);
    time_t modWhen = getFileModDate(path);
    sp<SharedZip> zip = gOpen.valueFor(path).promote();//   DefaultKeyedVector<String8, wp<AssetManager::SharedZip> > AssetManager::SharedZip::gOpen;
    if (zip != NULL && zip->mModWhen == modWhen) {
        return zip;
    }
    if (zip == NULL && !createIfNotPresent) {
        return NULL;
    }
    zip = new SharedZip(path, modWhen);
    gOpen.add(path, zip);
    return zip;
}



bool AssetManager::appendPathToResTable(const asset_path& ap, bool appAsLib = false) const {
    // skip those ap's that correspond to system overlays
    if (ap.isSystemOverlay) {// false
        return true;
    }

    Asset* ass = NULL;
    ResTable* sharedRes = NULL;
    bool shared = true;
    bool onlyEmptyResources = true;
    ATRACE_NAME(ap.path.string());
    Asset* idmap = openIdmapLocked(ap);
    size_t nextEntryIdx = mResources->getTableCount();
    ALOGV("Looking for resource asset in '%s'\n", ap.path.string());
    if (ap.type != kFileTypeDirectory) {
        if (nextEntryIdx == 0) {
            // The first item is typically the framework resources,
            // which we want to avoid parsing every time.
            // 在mZipSet查找对应的SharedZip成员，如果没有，创建一个加入集合
            sharedRes = const_cast<AssetManager*>(this)->mZipSet.getZipResourceTable(ap.path);
            if (sharedRes != NULL) {//这里sharedRes返回 NULL
                // skip ahead the number of system overlay packages preloaded
                nextEntryIdx = sharedRes->getTableCount();
            }
        }
        if (sharedRes == NULL) {
            //同样查找对应SharedZip成员，由于前面已经创建，这里获取ass为空
            ass = const_cast<AssetManager*>(this)->mZipSet.getZipResourceTableAsset(ap.path);
            if (ass == NULL) {
                ALOGV("loading resource table %s\n", ap.path.string());
                ass = const_cast<AssetManager*>(this)->openNonAssetInPathLocked("resources.arsc",Asset::ACCESS_BUFFER,ap);
                if (ass != NULL && ass != kExcludedAsset) {
                    ass = const_cast<AssetManager*>(this)->mZipSet.setZipResourceTableAsset(ap.path, ass);
                }
            }
            
            if (nextEntryIdx == 0 && ass != NULL) {
                // If this is the first resource table in the asset
                // manager, then we are going to cache it so that we
                // can quickly copy it out for others.
                ALOGV("Creating shared resources for %s", ap.path.string());
                sharedRes = new ResTable();
                sharedRes->add(ass, idmap, nextEntryIdx + 1, false);
#ifdef __ANDROID__
                const char* data = getenv("ANDROID_DATA");
                LOG_ALWAYS_FATAL_IF(data == NULL, "ANDROID_DATA not set");
                String8 overlaysListPath(data);
                overlaysListPath.appendPath(kResourceCache);
                overlaysListPath.appendPath("overlays.list");
                addSystemOverlays(overlaysListPath.string(), ap.path, sharedRes, nextEntryIdx);
#endif
                sharedRes = const_cast<AssetManager*>(this)->mZipSet.setZipResourceTable(ap.path, sharedRes);
            }
        }
    } else {
        。。。。。。
    }

    if ((ass != NULL || sharedRes != NULL) && ass != kExcludedAsset) {
        ALOGV("Installing resource asset %p in to table %p\n", ass, mResources);
        if (sharedRes != NULL) {
            ALOGV("Copying existing resources for %s", ap.path.string());
            mResources->add(sharedRes, ap.isSystemAsset);
        } else {
            ALOGV("Parsing resources for %s", ap.path.string());
            mResources->add(ass, idmap, nextEntryIdx + 1, !shared, appAsLib, ap.isSystemAsset);
        }
        onlyEmptyResources = false;

        if (!shared) {
            delete ass;
        }
    } else {
        ALOGV("Installing empty resources in to table %p\n", mResources);
        mResources->addEmpty(nextEntryIdx + 1);
    }

    if (idmap != NULL) {
        delete idmap;
    }
    return onlyEmptyResources;
}


//  @frameworks/base/libs/androidfw/misc.cpp
/*
 * Get a file's type.
 */
FileType getFileType(const char* fileName)
{
    struct stat sb;

    if (stat(fileName, &sb) < 0) {
        if (errno == ENOENT || errno == ENOTDIR)
            return kFileTypeNonexistent;
        else {
            fprintf(stderr, "getFileType got errno=%d on '%s'\n", errno, fileName);
            return kFileTypeUnknown;
        }
    } else {
        if (S_ISREG(sb.st_mode))
            return kFileTypeRegular;
        else if (S_ISDIR(sb.st_mode))
            return kFileTypeDirectory;
        else if (S_ISCHR(sb.st_mode))
            return kFileTypeCharDev;
        else if (S_ISBLK(sb.st_mode))
            return kFileTypeBlockDev;
        else if (S_ISFIFO(sb.st_mode))
            return kFileTypeFifo;
#if defined(S_ISLNK)
        else if (S_ISLNK(sb.st_mode))
            return kFileTypeSymlink;
#endif
#if defined(S_ISSOCK)
        else if (S_ISSOCK(sb.st_mode))
            return kFileTypeSocket;
#endif
        else
            return kFileTypeUnknown;
    }
}


Asset* AssetManager::openNonAssetInPathLocked(const char* fileName = "resources.arsc", AccessMode mode,const asset_path& ap)
{
    Asset* pAsset = NULL;

    /* look at the filesystem on disk */
    if (ap.type == kFileTypeDirectory) {
        。。。。。。
    /* look inside the zip file */
    } else {
        String8 path(fileName);

        /* check the appropriate Zip file */
        ZipFileRO* pZip = getZipFileLocked(ap);//ZipFileRO   @frameworks/base/libs/androidfw/ZipFileRO.cpp
        if (pZip != NULL) {
            //printf("GOT zip, checking NA '%s'\n", (const char*) path);
            ZipEntryRO entry = pZip->findEntryByName(path.string());
            if (entry != NULL) {
                //printf("FOUND NA in Zip file for %s\n", appName ? appName : kAppCommon);
                pAsset = openAssetFromZipLocked(pZip, entry, mode, path);
                pZip->releaseEntry(entry);
            }
        }

        if (pAsset != NULL) {
            /* create a "source" name, for debug/display */
            pAsset->setAssetSource(createZipSourceNameLocked(ZipSet::getPathName(ap.path.string()), String8(""),String8(fileName)));
        }
    }

    return pAsset;
}


ZipFileRO* AssetManager::getZipFileLocked(const asset_path& ap)
{
    ALOGV("getZipFileLocked() in %p\n", this);

    return mZipSet.getZip(ap.path);
}

/*
 * Retrieve the appropriate Zip file from the set.
 */
ZipFileRO* AssetManager::ZipSet::getZip(const String8& path)
{
    int idx = getIndex(path);
    sp<SharedZip> zip = mZipFile[idx];
    if (zip == NULL) {
        zip = SharedZip::get(path);
        mZipFile.editItemAt(idx) = zip;
    }
    return zip->getZip();
}

ZipFileRO* AssetManager::SharedZip::getZip()
{
    return mZipFile;//在创建的时候初始化，回到初始化中看一下
}

AssetManager::SharedZip::SharedZip(const String8& path, time_t modWhen)
    : mPath(path), mZipFile(NULL), mModWhen(modWhen),
      mResourceTableAsset(NULL), mResourceTable(NULL)
{
    if (kIsDebug) {
        ALOGI("Creating SharedZip %p %s\n", this, (const char*)mPath);
    }
    ALOGV("+++ opening zip '%s'\n", mPath.string());//  /system/framework/framework-res.apk
    mZipFile = ZipFileRO::open(mPath.string());//   @frameworks/base/libs/androidfw/ZipFileRO.cpp
    if (mZipFile == NULL) {
        ALOGD("failed to open Zip archive '%s'\n", mPath.string());
    }
}


//   @frameworks/base/libs/androidfw/ZipFileRO.cpp
/*
 * Open the specified file read-only.  We memory-map the entire thing and
 * close the file before returning.
 */
/* static */ ZipFileRO* ZipFileRO::open(const char* zipFileName)
{
    ZipArchiveHandle handle;
    const int32_t error = OpenArchive(zipFileName, &handle);
    if (error) {
        ALOGW("Error opening archive %s: %s", zipFileName, ErrorCodeString(error));
        CloseArchive(handle);
        return NULL;
    }

    return new ZipFileRO(handle, strdup(zipFileName));
}

//  @system/core/libziparchive/zip_archive.cc
int32_t OpenArchive(const char* fileName = "/system/framework/framework-res.apk", ZipArchiveHandle* handle) {
  const int fd = open(fileName, O_RDONLY | O_BINARY, 0);
  ZipArchive* archive = new ZipArchive(fd, true);// @system/core/libziparchive/zip_archive_private.h
  *handle = archive;

  if (fd < 0) {
    ALOGW("Unable to open '%s': %s", fileName, strerror(errno));
    return kIoError;
  }

  return OpenArchiveInternal(archive, fileName);
}


static int32_t OpenArchiveInternal(ZipArchive* archive,const char* debug_file_name = "/system/framework/framework-res.apk") {
  int32_t result = -1;
  if ((result = MapCentralDirectory(debug_file_name, archive)) != 0) {
    return result;
  }

  if ((result = ParseZipArchive(archive))) {
    return result;
  }

  return 0;
}

/*
 * Find the zip Central Directory and memory-map it.
 *
 * On success, returns 0 after populating fields from the EOCD area:
 *   directory_offset
 *   directory_ptr
 *   num_entries
 */
static int32_t MapCentralDirectory(const char* debug_file_name, ZipArchive* archive) {

  // Test file length. We use lseek64 to make sure the file
  // is small enough to be a zip file (Its size must be less than
  // 0xffffffff bytes).
  off64_t file_length = archive->mapped_zip.GetFileLength();
  if (file_length == -1) {
    return kInvalidFile;
  }

  if (file_length > static_cast<off64_t>(0xffffffff)) {
    ALOGV("Zip: zip file too long %" PRId64, static_cast<int64_t>(file_length));
    return kInvalidFile;
  }

  if (file_length < static_cast<off64_t>(sizeof(EocdRecord))) {
    ALOGV("Zip: length %" PRId64 " is too small to be zip", static_cast<int64_t>(file_length));
    return kInvalidFile;
  }

  /*
   * Perform the traditional EOCD snipe hunt.
   *
   * We're searching for the End of Central Directory magic number,
   * which appears at the start of the EOCD block.  It's followed by
   * 18 bytes of EOCD stuff and up to 64KB of archive comment.  We
   * need to read the last part of the file into a buffer, dig through
   * it to find the magic number, parse some values out, and use those
   * to determine the extent of the CD.
   *
   * We start by pulling in the last part of the file.
   */
  //    system/core/libziparchive/zip_archive_common.h:177:static const uint32_t kMaxCommentLen = 65535;
  //system/core/libziparchive/zip_archive.cc:58:static const uint32_t kMaxEOCDSearch = kMaxCommentLen + sizeof(EocdRecord);
  // 如果 apk 文件大于 kMaxEOCDSearch ，则读取尾部kMaxEOCDSearch字节；
  // 如果 apk 文件小于 kMaxEOCDSearch ，则读取整个文件所有字节；
  off64_t read_amount = kMaxEOCDSearch字节；
  // 如果 apk 文件小于;
  if (file_length < read_amount) {
    read_amount = file_length;
  }

  std::vector<uint8_t> scan_buffer(read_amount);
  int32_t result = MapCentralDirectory0(debug_file_name, archive, file_length, read_amount,scan_buffer.data());
  return result;
}


static int32_t MapCentralDirectory0(const char* debug_file_name, ZipArchive* archive,off64_t file_length, off64_t read_amount,uint8_t* scan_buffer) {
  const off64_t search_start = file_length - read_amount;

  //ReadAtOffset   @system/core/libziparchive/zip_archive.cc
  if(!archive->mapped_zip.ReadAtOffset(scan_buffer, read_amount, search_start)) {
    ALOGE("Zip: read %" PRId64 " from offset %" PRId64 " failed", static_cast<int64_t>(read_amount), static_cast<int64_t>(search_start));
    return kIoError; 
  }

  /*
   * Scan backward for the EOCD magic.  In an archive without a trailing
   * comment, we'll find it on the first try.  (We may want to consider
   * doing an initial minimal read; if we don't find it, retry with a
   * second read as above.)
   */
  int i = read_amount - sizeof(EocdRecord); //跳过  sizeof(EocdRecord) 大小 
  for (; i >= 0; i--) {
    if (scan_buffer[i] == 0x50) {//查找第一个 0x50
      uint32_t* sig_addr = reinterpret_cast<uint32_t*>(&scan_buffer[i]);
      if (get_unaligned<uint32_t>(sig_addr) == EocdRecord::kSignature) {
        ALOGV("+++ Found EOCD at buf+%d", i);
        break;
      }
    }
  }
  if (i < 0) {
    ALOGD("Zip: EOCD not found, %s is not zip", debug_file_name);
    return kInvalidFile;
  }

  const off64_t eocd_offset = search_start + i;
  const EocdRecord* eocd = reinterpret_cast<const EocdRecord*>(scan_buffer + i);
  /*
   * Verify that there's no trailing space at the end of the central directory
   * and its comment.
   */
  const off64_t calculated_length = eocd_offset + sizeof(EocdRecord) + eocd->comment_length;
  if (calculated_length != file_length) {
    ALOGW("Zip: %" PRId64 " extraneous bytes at the end of the central directory",static_cast<int64_t>(file_length - calculated_length));
    return kInvalidFile;
  }

  /*
   * Grab the CD offset and size, and the number of entries in the
   * archive and verify that they look reasonable.
   */
  if (static_cast<off64_t>(eocd->cd_start_offset) + eocd->cd_size > eocd_offset) {
    ALOGW("Zip: bad offsets (dir %" PRIu32 ", size %" PRIu32 ", eocd %" PRId64 ")",eocd->cd_start_offset, eocd->cd_size, static_cast<int64_t>(eocd_offset));
#if defined(__ANDROID__)
    if (eocd->cd_start_offset + eocd->cd_size <= eocd_offset) {
      android_errorWriteLog(0x534e4554, "31251826");
    }
#endif
    return kInvalidOffset;
  }
  if (eocd->num_records == 0) {
    ALOGW("Zip: empty archive?");
    return kEmptyArchive;
  }

  ALOGV("+++ num_entries=%" PRIu32 " dir_size=%" PRIu32 " dir_offset=%" PRIu32,eocd->num_records, eocd->cd_size, eocd->cd_start_offset);

  /*
   * It all looks good.  Create a mapping for the CD, and set the fields
   * in archive.
   */

  if (!archive->InitializeCentralDirectory(debug_file_name,static_cast<off64_t>(eocd->cd_start_offset),static_cast<size_t>(eocd->cd_size))) {
    ALOGE("Zip: failed to intialize central directory.\n");
    return kMmapFailed;
  }

  archive->num_entries = eocd->num_records;
  archive->directory_offset = eocd->cd_start_offset;

  return 0;
}

// Attempts to read |len| bytes into |buf| at offset |off|.
bool MappedZipFile::ReadAtOffset(uint8_t* buf, size_t len, off64_t off) {
#if !defined(_WIN32)
  if (has_fd_) {//has_fd_ == true
    if (static_cast<size_t>(TEMP_FAILURE_RETRY(pread64(fd_, buf, len, off))) != len) {
      ALOGE("Zip: failed to read at offset %" PRId64 "\n", off);
      return false;
    }
    return true;
  }
#endif

  ......
}






static int32_t ParseZipArchive(ZipArchive* archive) {
  const uint8_t* const cd_ptr = archive->central_directory.GetBasePtr();
  const size_t cd_length = archive->central_directory.GetMapLength();
  const uint16_t num_entries = archive->num_entries;

  /*
   * Create hash table.  We have a minimum 75% load factor, possibly as
   * low as 50% after we round off to a power of 2.  There must be at
   * least one unused entry to avoid an infinite loop during creation.
   */
  archive->hash_table_size = RoundUpPower2(1 + (num_entries * 4) / 3);
  archive->hash_table = reinterpret_cast<ZipString*>(calloc(archive->hash_table_size,
      sizeof(ZipString)));
  if (archive->hash_table == nullptr) {
    ALOGW("Zip: unable to allocate the %u-entry hash_table, entry size: %zu",
          archive->hash_table_size, sizeof(ZipString));
    return -1;
  }

  /*
   * Walk through the central directory, adding entries to the hash
   * table and verifying values.
   */
  const uint8_t* const cd_end = cd_ptr + cd_length;
  const uint8_t* ptr = cd_ptr;
  for (uint16_t i = 0; i < num_entries; i++) {
    if (ptr > cd_end - sizeof(CentralDirectoryRecord)) {
      ALOGW("Zip: ran off the end (at %" PRIu16 ")", i);
#if defined(__ANDROID__)
      android_errorWriteLog(0x534e4554, "36392138");
#endif
      return -1;
    }

    const CentralDirectoryRecord* cdr =
        reinterpret_cast<const CentralDirectoryRecord*>(ptr);
    if (cdr->record_signature != CentralDirectoryRecord::kSignature) {
      ALOGW("Zip: missed a central dir sig (at %" PRIu16 ")", i);
      return -1;
    }

    const off64_t local_header_offset = cdr->local_file_header_offset;
    if (local_header_offset >= archive->directory_offset) {
      ALOGW("Zip: bad LFH offset %" PRId64 " at entry %" PRIu16,
          static_cast<int64_t>(local_header_offset), i);
      return -1;
    }

    const uint16_t file_name_length = cdr->file_name_length;
    const uint16_t extra_length = cdr->extra_field_length;
    const uint16_t comment_length = cdr->comment_length;
    const uint8_t* file_name = ptr + sizeof(CentralDirectoryRecord);

    if (file_name + file_name_length > cd_end) {
      ALOGW("Zip: file name boundary exceeds the central directory range, file_name_length: " "%" PRIx16 ", cd_length: %zu", file_name_length, cd_length);
      return -1;
    }
    /* check that file name is valid UTF-8 and doesn't contain NUL (U+0000) characters */
    if (!IsValidEntryName(file_name, file_name_length)) {
      return -1;
    }

    /* add the CDE filename to the hash table */
    ZipString entry_name;
    entry_name.name = file_name;
    entry_name.name_length = file_name_length;
    const int add_result = AddToHash(archive->hash_table,
        archive->hash_table_size, entry_name);
    if (add_result != 0) {
      ALOGW("Zip: Error adding entry to hash table %d", add_result);
      return add_result;
    }

    ptr += sizeof(CentralDirectoryRecord) + file_name_length + extra_length + comment_length;
    if ((ptr - cd_ptr) > static_cast<int64_t>(cd_length)) {
      ALOGW("Zip: bad CD advance (%tu vs %zu) at entry %" PRIu16,
          ptr - cd_ptr, cd_length, i);
      return -1;
    }
  }
  ALOGV("+++ zip good scan %" PRIu16 " entries", num_entries);

  return 0;
}