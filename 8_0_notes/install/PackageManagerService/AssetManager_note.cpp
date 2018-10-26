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
    const char* root = getenv("ANDROID_ROOT");
    LOG_ALWAYS_FATAL_IF(root == NULL, "ANDROID_ROOT not set");

    String8 path(root);
    path.appendPath(kSystemAssets);//	static const char* kSystemAssets = "framework/framework-res.apk";

    //			/system/framework/framework-res.apk
    return addAssetPath(path, NULL, false /* appAsLib */, true /* isSystemAsset */);
}

bool AssetManager::addAssetPath(const String8& path, int32_t* cookie, bool appAsLib, bool isSystemAsset) {
    AutoMutex _l(mLock);

    asset_path ap;

    String8 realPath(path);//			realPath = "/system/framework/framework-res.apk"
    if (kAppZipName) {//	static const char* kAppZipName = NULL; //"classes.jar";
        realPath.appendPath(kAppZipName);
    }
    ap.type = ::getFileType(realPath.string());
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
    if (cookie) {
        *cookie = static_cast<int32_t>(mAssetPaths.size());
    }

#ifdef __ANDROID__
    // Load overlays, if any
    asset_path oap;
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



/*
 * Get the type of a file in the asset namespace.
 *
 * This currently only works for regular files.  All others (including
 * directories) will return kFileTypeNonexistent.
 */
FileType AssetManager::getFileType(const char* fileName)//	fileName = "/system/framework/framework-res.apk"
{
    Asset* pAsset = NULL;

    /*
     * Open the asset.  This is less efficient than simply finding the
     * file, but it's not too bad (we don't uncompress or mmap data until
     * the first read() call).
     */
    pAsset = open(fileName, Asset::ACCESS_STREAMING);
    delete pAsset;

    if (pAsset == NULL) {
        return kFileTypeNonexistent;
    } else {
        return kFileTypeRegular;
    }
}


/*
 * Open an asset.
 *
 * The data could be in any asset path. Each asset path could be:
 *  - A directory on disk.
 *  - A Zip archive, uncompressed or compressed.
 *
 * If the file is in a directory, it could have a .gz suffix, meaning it is compressed.
 *
 * We should probably reject requests for "illegal" filenames, e.g. those
 * with illegal characters or "../" backward relative paths.
 */
Asset* AssetManager::open(const char* fileName, AccessMode mode)
{
    AutoMutex _l(mLock);

    LOG_FATAL_IF(mAssetPaths.size() == 0, "No assets added to AssetManager");

    String8 assetName(kAssetsRoot);	//static const char* kAssetsRoot = "assets";
    assetName.appendPath(fileName);

    /*
     * For each top-level asset path, search for the asset.
     */

    size_t i = mAssetPaths.size();
    while (i > 0) {
        i--;
        ALOGV("Looking for asset '%s' in '%s'\n",assetName.string(), mAssetPaths.itemAt(i).path.string());
        Asset* pAsset = openNonAssetInPathLocked(assetName.string(), mode, mAssetPaths.itemAt(i));
        if (pAsset != NULL) {
            return pAsset != kExcludedAsset ? pAsset : NULL;
        }
    }

    return NULL;
}

Asset* AssetManager::openNonAssetInPathLocked(const char* fileName, AccessMode mode,const asset_path& ap)
{
    Asset* pAsset = NULL;

    /* look at the filesystem on disk */
    if (ap.type == kFileTypeDirectory) {
        String8 path(ap.path);
        path.appendPath(fileName);

        pAsset = openAssetFromFileLocked(path, mode);

        if (pAsset == NULL) {
            /* try again, this time with ".gz" */
            path.append(".gz");
            pAsset = openAssetFromFileLocked(path, mode);
        }

        if (pAsset != NULL) {
            //printf("FOUND NA '%s' on disk\n", fileName);
            pAsset->setAssetSource(path);
        }

    /* look inside the zip file */
    } else {
        String8 path(fileName);

        /* check the appropriate Zip file */
        ZipFileRO* pZip = getZipFileLocked(ap);
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
            pAsset->setAssetSource(
                    createZipSourceNameLocked(ZipSet::getPathName(ap.path.string()), String8(""),String8(fileName)));
        }
    }

    return pAsset;
}