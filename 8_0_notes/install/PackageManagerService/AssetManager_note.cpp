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

    return addAssetPath(path, NULL, false /* appAsLib */, true /* isSystemAsset */);
}