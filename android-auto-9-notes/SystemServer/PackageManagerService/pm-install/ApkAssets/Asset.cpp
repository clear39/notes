

Asset::Asset(void)
    : mAccessMode(ACCESS_UNKNOWN), mNext(NULL), mPrev(NULL)
{
}


/*
 * Create a new Asset from compressed data in a memory mapping.
 */
/*static*/ Asset* Asset::createFromCompressedMap(FileMap* dataMap, size_t uncompressedLen, AccessMode mode)
{
    _CompressedAsset* pAsset;
    status_t result;

    pAsset = new _CompressedAsset;
    result = pAsset->openChunk(dataMap, uncompressedLen);
    if (result != NO_ERROR)
        return NULL;

    pAsset->mAccessMode = mode;
    return pAsset;
}


/*
 * Create a new Asset from a memory mapping.
 */
/*static*/ Asset* Asset::createFromUncompressedMap(FileMap* dataMap, AccessMode mode)
{
    _FileAsset* pAsset;
    status_t result;

    pAsset = new _FileAsset;
    result = pAsset->openChunk(dataMap);
    if (result != NO_ERROR)
        return NULL;

    pAsset->mAccessMode = mode;
    return pAsset;
}