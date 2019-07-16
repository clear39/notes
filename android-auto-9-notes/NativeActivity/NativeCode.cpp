struct NativeCode : public ANativeActivity {

    ANativeActivityCallbacks callbacks;
    
    void* dlhandle;
    ANativeActivity_createFunc* createActivityFunc;
    
    String8 internalDataPathObj;
    String8 externalDataPathObj;
    String8 obbPathObj;
    
    sp<ANativeWindow> nativeWindow;
    int32_t lastWindowWidth;
    int32_t lastWindowHeight;

    // These are used to wake up the main thread to process work.
    int mainWorkRead;
    int mainWorkWrite;
    sp<MessageQueue> messageQueue;

    // Need to hold on to a reference here in case the upper layers destroy our
    // AssetManager.
    jobject javaAssetManager;
    
}

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/base/core/jni/android_app_NativeActivity.cpp


NativeCode::NativeCode(void* _dlhandle, ANativeActivity_createFunc* _createFunc) {
    memset((ANativeActivity*)this, 0, sizeof(ANativeActivity));
    memset(&callbacks, 0, sizeof(callbacks));
    dlhandle = _dlhandle;
    createActivityFunc = _createFunc; //函数入口
    nativeWindow = NULL;
    mainWorkRead = mainWorkWrite = -1;
}


//  @/work/workcodes/aosp-p9.x-auto-ga/frameworks/native/include/android/native_activity.h
/**
 * This structure defines the native side of an android.app.NativeActivity.
 * It is created by the framework, and handed to the application's native
 * code as it is being launched.
 */
typedef struct ANativeActivity {
    /**
     * Pointer to the callback function table of the native application.
     * You can set the functions here to your own callbacks.  The callbacks
     * pointer itself here should not be changed; it is allocated and managed
     * for you by the framework.
     */
    struct ANativeActivityCallbacks* callbacks;

    /**
     * The global handle on the process's Java VM.
     */
    JavaVM* vm;

    /**
     * JNI context for the main thread of the app.  Note that this field
     * can ONLY be used from the main thread of the process; that is, the
     * thread that calls into the ANativeActivityCallbacks.
     */
    JNIEnv* env;

    /**
     * The NativeActivity object handle.
     *
     * IMPORTANT NOTE: This member is mis-named. It should really be named
     * 'activity' instead of 'clazz', since it's a reference to the
     * NativeActivity instance created by the system for you.
     *
     * We unfortunately cannot change this without breaking NDK
     * source-compatibility.
     */
    jobject clazz;

    /**
     * Path to this application's internal data directory.
     */
    const char* internalDataPath;

    /**
     * Path to this application's external (removable/mountable) data directory.
     */
    const char* externalDataPath;

    /**
     * The platform's SDK version code.
     */
    int32_t sdkVersion;

    /**
     * This is the native instance of the application.  It is not used by
     * the framework, but can be set by the application to its own instance
     * state.
     */
    void* instance;

    /**
     * Pointer to the Asset Manager instance for the application.  The application
     * uses this to access binary assets bundled inside its own .apk file.
     */
    AAssetManager* assetManager;

    /**
     * Available starting with Honeycomb: path to the directory containing
     * the application's OBB files (if any).  If the app doesn't have any
     * OBB files, this directory may not exist.
     */
    const char* obbPath;
} ANativeActivity;



