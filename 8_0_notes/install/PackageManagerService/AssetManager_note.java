//	@frameworks/base/core/java/android/content/res/AssetManager.java

/**
 * Create a new AssetManager containing only the basic system assets.
 * Applications will not generally use this method, instead retrieving the
 * appropriate asset manager with {@link Resources#getAssets}.    Not for
 * use by applications.
 * {@hide}
 */
public AssetManager() {
    synchronized (this) {
        if (DEBUG_REFS) {//	private static final boolean DEBUG_REFS = false;
            mNumRefs = 0;
            incRefsLocked(this.hashCode());
        }
        init(false); //native
        if (localLOGV) Log.v(TAG, "New asset manager: " + this);
        ensureSystemAssets();
    }
}

//	@frameworks/base/core/jni/android_util_AssetManager.cpp
static void android_content_AssetManager_init(JNIEnv* env, jobject clazz, jboolean isSystem)
{
    if (isSystem) {
        verifySystemIdmaps();
    }
    AssetManager* am = new AssetManager();
    if (am == NULL) {
        jniThrowException(env, "java/lang/OutOfMemoryError", "");
        return;
    }

    am->addDefaultAssets();

    ALOGV("Created AssetManager %p for Java object %p\n", am, clazz);
    env->SetLongField(clazz, gAssetManagerOffsets.mObject, reinterpret_cast<jlong>(am));
}




private static void ensureSystemAssets() {
    synchronized (sSync) {
        if (sSystem == null) {
            AssetManager system = new AssetManager(true);
            system.makeStringBlocks(null);
            sSystem = system;
        }
    }
}


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
                argv[argc++] = AssetManager::IDMAP_BIN;
                argv[argc++] = "--scan";
                argv[argc++] = AssetManager::TARGET_PACKAGE_NAME;
                argv[argc++] = AssetManager::TARGET_APK_PATH;
                argv[argc++] = AssetManager::IDMAP_DIR;

                // Directories to scan for overlays: if OVERLAY_THEME_DIR_PROPERTY is defined,
                // use OVERLAY_DIR/<value of OVERLAY_THEME_DIR_PROPERTY> in addition to OVERLAY_DIR.
                char subdir[PROP_VALUE_MAX];
                int len = __system_property_get(AssetManager::OVERLAY_THEME_DIR_PROPERTY, subdir);
                if (len > 0) {
                    String8 overlayPath = String8(AssetManager::OVERLAY_DIR) + "/" + subdir;
                    if (stat(overlayPath.string(), &st) == 0) {
                        argv[argc++] = overlayPath.string();
                    }
                }
                if (stat(AssetManager::OVERLAY_DIR, &st) == 0) {
                    argv[argc++] = AssetManager::OVERLAY_DIR;
                }

                // Finally, invoke idmap (if any overlay directory exists)
                if (argc > 5) {
                    execv(AssetManager::IDMAP_BIN, (char* const*)argv);
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


/*package*/ final void makeStringBlocks(StringBlock[] seed) {
    final int seedNum = (seed != null) ? seed.length : 0;
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


static jint android_content_AssetManager_getStringBlockCount(JNIEnv* env, jobject clazz)
{
    AssetManager* am = assetManagerForJavaObject(env, clazz);
    if (am == NULL) {
        return 0;
    }
    return am->getResources().getTableCount();
}