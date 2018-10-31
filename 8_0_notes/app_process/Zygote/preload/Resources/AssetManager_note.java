01-01 01:10:16.613   247   247 V AssetManager: ensureSystemAssets sSystem: null
01-01 01:10:16.613   247   247 V AssetManager: java.lang.Throwable
01-01 01:10:16.613   247   247 V AssetManager:  at android.content.res.AssetManager.ensureSystemAssets(AssetManager.java:111)
01-01 01:10:16.613   247   247 V AssetManager:  at android.content.res.AssetManager.getSystem(AssetManager.java:137)
01-01 01:10:16.613   247   247 V AssetManager:  at android.content.res.Resources.<init>(Resources.java:246)
01-01 01:10:16.613   247   247 V AssetManager:  at android.content.res.Resources.getSystem(Resources.java:179)
01-01 01:10:16.613   247   247 V AssetManager:  at com.android.internal.os.ZygoteInit.preloadResources(ZygoteInit.java:358)
01-01 01:10:16.613   247   247 V AssetManager:  at com.android.internal.os.ZygoteInit.preload(ZygoteInit.java:133)
01-01 01:10:16.613   247   247 V AssetManager:  at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:746)





//	@frameworks/base/core/java/android/content/res/AssetManager.java
/**
 * Provides access to an application's raw asset files; see {@link Resources}
 * for the way most applications will want to retrieve their resource data.
 * This class presents a lower-level API that allows you to open and read raw
 * files that have been bundled with the application as a simple stream of
 * bytes.
 */
public final class AssetManager implements AutoCloseable {

    //第一调用，在Zygote通过
    /**
     * Return a global shared asset manager that provides access to only
     * system assets (no application assets).
     * {@hide}
     */
    public static AssetManager getSystem() {
        ensureSystemAssets();
        return sSystem;
    }

    //注意这里是java层的全局静态变量
    private static void ensureSystemAssets() {
        synchronized (sSync) {
            if (sSystem == null) {
                AssetManager system = new AssetManager(true);
                system.makeStringBlocks(null);
                sSystem = system;
            }
        }
    }

    //这里调用是全局调用
    private AssetManager(boolean isSystem) {
        if (DEBUG_REFS) {
            synchronized (this) {
                mNumRefs = 0;
                incRefsLocked(this.hashCode());
            }
        }
        init(true);//native
        if (localLOGV) Log.v(TAG, "New asset manager: " + this);
    }


    /*package*/ final void makeStringBlocks(StringBlock[] seed) {
        final int seedNum = (seed != null) ? seed.length : 0;//0
        final int num = getStringBlockCount();//native
        mStringBlocks = new StringBlock[num];
        if (localLOGV) Log.v(TAG, "Making string blocks for " + this + ": " + num);
        for (int i=0; i<num; i++) {
            if (i < seedNum) {
                mStringBlocks[i] = seed[i];
            } else {
                mStringBlocks[i] = new StringBlock(getNativeStringBlock(i), true);
            }
        }
    }



}


//  @frameworks/base/core/jni/android_util_AssetManager.cpp
static void android_content_AssetManager_init(JNIEnv* env, jobject clazz, jboolean isSystem = true)
{
    if (isSystem) {
        verifySystemIdmaps();//这里进行fork idmap进程
    }
    AssetManager* am = new AssetManager();//这里创建AssetManager c层类
    if (am == NULL) {
        jniThrowException(env, "java/lang/OutOfMemoryError", "");
        return;
    }

    //添加系统资源/system/framework/framework-res.apk
    //进入c++层分析
    am->addDefaultAssets();

    ALOGV("Created AssetManager %p for Java object %p\n", am, clazz);

    env->SetLongField(clazz, gAssetManagerOffsets.mObject, reinterpret_cast<jlong>(am));
}

//  @frameworks/base/core/jni/android_util_AssetManager.cpp
static void verifySystemIdmaps()
{
    pid_t pid;
    char system_id[10];

    snprintf(system_id, sizeof(system_id), "%d", AID_SYSTEM);

    switch (pid = fork()) {
        case -1:
            ALOGE("failed to fork for idmap: %s", strerror(errno));
            break;
        case 0: // child
            {
                struct __user_cap_header_struct capheader;
                struct __user_cap_data_struct capdata;

                memset(&capheader, 0, sizeof(capheader));
                memset(&capdata, 0, sizeof(capdata));

                capheader.version = _LINUX_CAPABILITY_VERSION;
                capheader.pid = 0;

                if (capget(&capheader, &capdata) != 0) {
                    ALOGE("capget: %s\n", strerror(errno));
                    exit(1);
                }

                capdata.effective = capdata.permitted;
                if (capset(&capheader, &capdata) != 0) {
                    ALOGE("capset: %s\n", strerror(errno));
                    exit(1);
                }

                if (setgid(AID_SYSTEM) != 0) {
                    ALOGE("setgid: %s\n", strerror(errno));
                    exit(1);
                }

                if (setuid(AID_SYSTEM) != 0) {
                    ALOGE("setuid: %s\n", strerror(errno));
                    exit(1);
                }

                // Generic idmap parameters
                const char* argv[8];
                int argc = 0;
                struct stat st;

                memset(argv, NULL, sizeof(argv));
                argv[argc++] = AssetManager::IDMAP_BIN;//   const char* AssetManager::IDMAP_BIN = "/system/bin/idmap";
                argv[argc++] = "--scan";
                argv[argc++] = AssetManager::TARGET_PACKAGE_NAME;   //const char* AssetManager::TARGET_PACKAGE_NAME = "android";
                argv[argc++] = AssetManager::TARGET_APK_PATH;// const char* AssetManager::TARGET_APK_PATH = "/system/framework/framework-res.apk";
                argv[argc++] = AssetManager::IDMAP_DIR; //  const char* AssetManager::IDMAP_DIR = "/data/resource-cache";

                // Directories to scan for overlays: if OVERLAY_THEME_DIR_PROPERTY is defined,
                // use OVERLAY_DIR/<value of OVERLAY_THEME_DIR_PROPERTY> in addition to OVERLAY_DIR.
                char subdir[PROP_VALUE_MAX];
                //由于系统默认OVERLAY_THEME_DIR_PROPERTY属性为空，所以len 为 0
                int len = __system_property_get(AssetManager::OVERLAY_THEME_DIR_PROPERTY, subdir);//    const char* AssetManager::OVERLAY_THEME_DIR_PROPERTY = "ro.boot.vendor.overlay.theme";
                if (len > 0) {
                    String8 overlayPath = String8(AssetManager::OVERLAY_DIR) + "/" + subdir;    //const char* AssetManager::OVERLAY_DIR = "/vendor/overlay";
                    if (stat(overlayPath.string(), &st) == 0) {
                        argv[argc++] = overlayPath.string();
                    }
                }

                ///vendor/overlay目录存在添加到arg[6]中
                if (stat(AssetManager::OVERLAY_DIR, &st) == 0) {
                    argv[argc++] = AssetManager::OVERLAY_DIR;
                }

                // Finally, invoke idmap (if any overlay directory exists)

                /*
                /system/bin/idmap
                --scan
                android
                /system/framework/framework-res.apk"
                /data/resource-cache
                /vendor/overlay
                */
                if (argc > 5) {
                    execv(AssetManager::IDMAP_BIN, (char* const*)argv);//   const char* AssetManager::IDMAP_BIN = "/system/bin/idmap";    //    idmap@frameworks/base/cmds/idmap
                    ALOGE("failed to execv for idmap: %s", strerror(errno));
                    exit(1); // should never get here
                } else {
                    exit(0);
                }
            }
            break;  
        default: // parent
            waitpid(pid, NULL, 0);
            break;
    }
}



static jint android_content_AssetManager_getStringBlockCount(JNIEnv* env, jobject clazz)
{
    AssetManager* am = assetManagerForJavaObject(env, clazz);
    if (am == NULL) {
        return 0;
    }
    return am->getResources().getTableCount();
}

// this guy is exported to other jni routines
AssetManager* assetManagerForJavaObject(JNIEnv* env, jobject obj)
{
    jlong amHandle = env->GetLongField(obj, gAssetManagerOffsets.mObject);
    AssetManager* am = reinterpret_cast<AssetManager*>(amHandle);
    if (am != NULL) {
        return am;
    }
    jniThrowException(env, "java/lang/IllegalStateException", "AssetManager has been finalized!");
    return NULL;
}


const ResTable& AssetManager::getResources(bool required) const
{
    const ResTable* rt = getResTable(required);
    return *rt;
}

const ResTable* AssetManager::getResTable(bool required) const
{
    ResTable* rt = mResources;
    if (rt) {
        return rt;
    }

    // Iterate through all asset packages, collecting resources from each.

    AutoMutex _l(mLock);

    if (mResources != NULL) {
        return mResources;
    }

    if (required) {
        LOG_FATAL_IF(mAssetPaths.size() == 0, "No assets added to AssetManager");
    }

    mResources = new ResTable();
    updateResourceParamsLocked();

    bool onlyEmptyResources = true;
    const size_t N = mAssetPaths.size();
    for (size_t i=0; i<N; i++) {
        bool empty = appendPathToResTable(mAssetPaths.itemAt(i));
        onlyEmptyResources = onlyEmptyResources && empty;
    }

    if (required && onlyEmptyResources) {
        ALOGW("Unable to find resources file resources.arsc");
        delete mResources;
        mResources = NULL;
    }

    return mResources;
}


void AssetManager::updateResourceParamsLocked() const
{
    ATRACE_CALL();
    ResTable* res = mResources;
    if (!res) {
        return;
    }

    if (mLocale) {
        mConfig->setBcp47Locale(mLocale);
    } else {
        mConfig->clearLocale();
    }

    res->setParameters(mConfig);
}



























