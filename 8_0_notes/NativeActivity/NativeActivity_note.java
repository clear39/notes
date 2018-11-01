//	@frameworks/base/core/java/android/app/NativeActivity.java

public class NativeActivity extends Activity implements SurfaceHolder.Callback2,InputQueue.Callback, OnGlobalLayoutListener {

	 @Override
    protected void onCreate(Bundle savedInstanceState) {
        String libname = "main";
        String funcname = "ANativeActivity_onCreate";	//默认函数入口
        ActivityInfo ai;

        mIMM = getSystemService(InputMethodManager.class);

        getWindow().takeSurface(this);
        getWindow().takeInputQueue(this);
        getWindow().setFormat(PixelFormat.RGB_565);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        mNativeContentView = new NativeContentView(this);
        mNativeContentView.mActivity = this;
        setContentView(mNativeContentView);
        mNativeContentView.requestFocus();
        mNativeContentView.getViewTreeObserver().addOnGlobalLayoutListener(this);
        
        try {
            ai = getPackageManager().getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
            if (ai.metaData != null) {
                String ln = ai.metaData.getString(META_DATA_LIB_NAME);//public static final String META_DATA_LIB_NAME = "android.app.lib_name";
                if (ln != null) libname = ln;
                ln = ai.metaData.getString(META_DATA_FUNC_NAME);//public static final String META_DATA_FUNC_NAME = "android.app.func_name";
                if (ln != null) funcname = ln;
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Error getting activity info", e);
        }

        BaseDexClassLoader classLoader = (BaseDexClassLoader) getClassLoader();
        String path = classLoader.findLibrary(libname);

        if (path == null) {
            throw new IllegalArgumentException("Unable to find native library " + libname + " using classloader: " + classLoader.toString());
        }
        
        byte[] nativeSavedState = savedInstanceState != null ? savedInstanceState.getByteArray(KEY_NATIVE_SAVED_STATE) : null;

        mNativeHandle = loadNativeCode(path, funcname, Looper.myQueue(),
                getAbsolutePath(getFilesDir()), getAbsolutePath(getObbDir()),
                getAbsolutePath(getExternalFilesDir(null)),
                Build.VERSION.SDK_INT, getAssets(), nativeSavedState,
                classLoader, classLoader.getLdLibraryPath());

        if (mNativeHandle == 0) {
            throw new UnsatisfiedLinkError("Unable to load native library \"" + path + "\": " + getDlError());
        }
        super.onCreate(savedInstanceState);
    }
}



//	@frameworks/base/core/jni/android_app_NativeActivity.cpp
static jlong
loadNativeCode_native(JNIEnv* env, jobject clazz, jstring path, jstring funcName,
        jobject messageQueue, jstring internalDataDir, jstring obbDir,
        jstring externalDataDir, jint sdkVersion, jobject jAssetMgr,
        jbyteArray savedState, jobject classLoader, jstring libraryPath) {
    if (kLogTrace) {	//	static const bool kLogTrace = false;
        ALOGD("loadNativeCode_native");
    }

    ScopedUtfChars pathStr(env, path);

    //	ANativeActivity @frameworks/native/include/android/native_activity.h:
    std::unique_ptr<NativeCode> code; //frameworks/base/core/jni/android_app_NativeActivity.cpp NativeCode继承ANativeActivity
    bool needs_native_bridge = false;
    std::string error_msg;

    //	@system/core/libnativeloader/native_loader.cpp
    void* handle = OpenNativeLibrary(env,sdkVersion,pathStr.c_str(),classLoader,libraryPath,&needs_native_bridge,&error_msg);

    if (handle == nullptr) {
        ALOGW("NativeActivity LoadNativeLibrary(\"%s\") failed: %s",pathStr.c_str(),error_msg.c_str());
        return 0;
    }

    void* funcPtr = NULL;
    const char* funcStr = env->GetStringUTFChars(funcName, NULL);
    if (needs_native_bridge) {
        funcPtr = NativeBridgeGetTrampoline(handle, funcStr, NULL, 0);
    } else {
        funcPtr = dlsym(handle, funcStr);
    }

    code.reset(new NativeCode(handle, (ANativeActivity_createFunc*)funcPtr));
    env->ReleaseStringUTFChars(funcName, funcStr);

    if (code->createActivityFunc == NULL) {
        ALOGW("ANativeActivity_onCreate not found");
        return 0;
    }

    code->messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueue);
    if (code->messageQueue == NULL) {
        ALOGW("Unable to retrieve native MessageQueue");
        return 0;
    }

    int msgpipe[2];
    if (pipe(msgpipe)) {
        ALOGW("could not create pipe: %s", strerror(errno));
        return 0;
    }
    code->mainWorkRead = msgpipe[0];
    code->mainWorkWrite = msgpipe[1];
    int result = fcntl(code->mainWorkRead, F_SETFL, O_NONBLOCK);
    SLOGW_IF(result != 0, "Could not make main work read pipe " "non-blocking: %s", strerror(errno));
    result = fcntl(code->mainWorkWrite, F_SETFL, O_NONBLOCK);
    SLOGW_IF(result != 0, "Could not make main work write pipe " "non-blocking: %s", strerror(errno));
    code->messageQueue->getLooper()->addFd( code->mainWorkRead, 0, ALOOPER_EVENT_INPUT, mainWorkCallback, code.get());

    code->ANativeActivity::callbacks = &code->callbacks;
    if (env->GetJavaVM(&code->vm) < 0) {
        ALOGW("NativeActivity GetJavaVM failed");
        return 0;
    }
    code->env = env;
    code->clazz = env->NewGlobalRef(clazz);

    const char* dirStr = env->GetStringUTFChars(internalDataDir, NULL);
    code->internalDataPathObj = dirStr;
    code->internalDataPath = code->internalDataPathObj.string();
    env->ReleaseStringUTFChars(internalDataDir, dirStr);

    if (externalDataDir != NULL) {
        dirStr = env->GetStringUTFChars(externalDataDir, NULL);
        code->externalDataPathObj = dirStr;
        env->ReleaseStringUTFChars(externalDataDir, dirStr);
    }
    code->externalDataPath = code->externalDataPathObj.string();

    code->sdkVersion = sdkVersion;

    code->javaAssetManager = env->NewGlobalRef(jAssetMgr);
    code->assetManager = assetManagerForJavaObject(env, jAssetMgr);

    if (obbDir != NULL) {
        dirStr = env->GetStringUTFChars(obbDir, NULL);
        code->obbPathObj = dirStr;
        env->ReleaseStringUTFChars(obbDir, dirStr);
    }
    code->obbPath = code->obbPathObj.string();

    jbyte* rawSavedState = NULL;
    jsize rawSavedSize = 0;
    if (savedState != NULL) {
        rawSavedState = env->GetByteArrayElements(savedState, NULL);
        rawSavedSize = env->GetArrayLength(savedState);
    }

    code->createActivityFunc(code.get(), rawSavedState, rawSavedSize);

    if (rawSavedState != NULL) {
        env->ReleaseByteArrayElements(savedState, rawSavedState, 0);
    }

    return (jlong)code.release();
}




void* OpenNativeLibrary(JNIEnv* env,int32_t target_sdk_version,const char* path,jobject class_loader,jstring library_path,bool* needs_native_bridge,std::string* error_msg) {
#if defined(__ANDROID__)
  UNUSED(target_sdk_version);
  if (class_loader == nullptr) {//	class_loader 不为空
    *needs_native_bridge = false;
    return dlopen(path, RTLD_NOW);
  }

  std::lock_guard<std::mutex> guard(g_namespaces_mutex);
  NativeLoaderNamespace ns;

  //	static LibraryNamespaces* g_namespaces = new LibraryNamespaces;	//LibraryNamespaces	@system/core/libnativeloader/native_loader.cpp
  if (!g_namespaces->FindNamespaceByClassLoader(env, class_loader, &ns)) {
    // This is the case where the classloader was not created by ApplicationLoaders
    // In this case we create an isolated not-shared namespace for it.
    if (!g_namespaces->Create(env,target_sdk_version,class_loader,false,library_path,nullptr,&ns,error_msg)) {
      return nullptr;
    }
  }

  if (ns.is_android_namespace()) {
    android_dlextinfo extinfo;
    extinfo.flags = ANDROID_DLEXT_USE_NAMESPACE;
    extinfo.library_namespace = ns.get_android_ns();

    void* handle = android_dlopen_ext(path, RTLD_NOW, &extinfo);
    if (handle == nullptr) {
      *error_msg = dlerror();
    }
    *needs_native_bridge = false;
    return handle;
  } else {
    void* handle = NativeBridgeLoadLibraryExt(path, RTLD_NOW, ns.get_native_bridge_ns());
    if (handle == nullptr) {
      *error_msg = NativeBridgeGetError();
    }
    *needs_native_bridge = true;
    return handle;
  }
#else
  .......
#endif
}






//  @frameworks/native/include/android/native_activity.h
/**
 * These are the callbacks the framework makes into a native application.
 * All of these callbacks happen on the main thread of the application.
 * By default, all callbacks are NULL; set to a pointer to your own function
 * to have it called.
 */
typedef struct ANativeActivityCallbacks {
    /**
     * NativeActivity has started.  See Java documentation for Activity.onStart()
     * for more information.
     */
    void (*onStart)(ANativeActivity* activity);

    /**
     * NativeActivity has resumed.  See Java documentation for Activity.onResume()
     * for more information.
     */
    void (*onResume)(ANativeActivity* activity);

    /**
     * Framework is asking NativeActivity to save its current instance state.
     * See Java documentation for Activity.onSaveInstanceState() for more
     * information.  The returned pointer needs to be created with malloc();
     * the framework will call free() on it for you.  You also must fill in
     * outSize with the number of bytes in the allocation.  Note that the
     * saved state will be persisted, so it can not contain any active
     * entities (pointers to memory, file descriptors, etc).
     */
    void* (*onSaveInstanceState)(ANativeActivity* activity, size_t* outSize);

    /**
     * NativeActivity has paused.  See Java documentation for Activity.onPause()
     * for more information.
     */
    void (*onPause)(ANativeActivity* activity);

    /**
     * NativeActivity has stopped.  See Java documentation for Activity.onStop()
     * for more information.
     */
    void (*onStop)(ANativeActivity* activity);

    /**
     * NativeActivity is being destroyed.  See Java documentation for Activity.onDestroy()
     * for more information.
     */
    void (*onDestroy)(ANativeActivity* activity);

    /**
     * Focus has changed in this NativeActivity's window.  This is often used,
     * for example, to pause a game when it loses input focus.
     */
    void (*onWindowFocusChanged)(ANativeActivity* activity, int hasFocus);

    /**
     * The drawing window for this native activity has been created.  You
     * can use the given native window object to start drawing.
     */
    void (*onNativeWindowCreated)(ANativeActivity* activity, ANativeWindow* window);

    /**
     * The drawing window for this native activity has been resized.  You should
     * retrieve the new size from the window and ensure that your rendering in
     * it now matches.
     */
    void (*onNativeWindowResized)(ANativeActivity* activity, ANativeWindow* window);

    /**
     * The drawing window for this native activity needs to be redrawn.  To avoid
     * transient artifacts during screen changes (such resizing after rotation),
     * applications should not return from this function until they have finished
     * drawing their window in its current state.
     */
    void (*onNativeWindowRedrawNeeded)(ANativeActivity* activity, ANativeWindow* window);

    /**
     * The drawing window for this native activity is going to be destroyed.
     * You MUST ensure that you do not touch the window object after returning
     * from this function: in the common case of drawing to the window from
     * another thread, that means the implementation of this callback must
     * properly synchronize with the other thread to stop its drawing before
     * returning from here.
     */
    void (*onNativeWindowDestroyed)(ANativeActivity* activity, ANativeWindow* window);

    /**
     * The input queue for this native activity's window has been created.
     * You can use the given input queue to start retrieving input events.
     */
    void (*onInputQueueCreated)(ANativeActivity* activity, AInputQueue* queue);

    /**
     * The input queue for this native activity's window is being destroyed.
     * You should no longer try to reference this object upon returning from this
     * function.
     */
    void (*onInputQueueDestroyed)(ANativeActivity* activity, AInputQueue* queue);

    /**
     * The rectangle in the window in which content should be placed has changed.
     */
    void (*onContentRectChanged)(ANativeActivity* activity, const ARect* rect);

    /**
     * The current device AConfiguration has changed.  The new configuration can
     * be retrieved from assetManager.
     */
    void (*onConfigurationChanged)(ANativeActivity* activity);

    /**
     * The system is running low on memory.  Use this callback to release
     * resources you do not need, to help the system avoid killing more
     * important processes.
     */
    void (*onLowMemory)(ANativeActivity* activity);
} ANativeActivityCallbacks;




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



//  @frameworks/base/core/jni/android_app_NativeActivity.cpp
/*
 * Native state for interacting with the NativeActivity class.
 */
struct NativeCode : public ANativeActivity {
    NativeCode(void* _dlhandle, ANativeActivity_createFunc* _createFunc) {
        memset((ANativeActivity*)this, 0, sizeof(ANativeActivity));
        memset(&callbacks, 0, sizeof(callbacks));
        dlhandle = _dlhandle;
        createActivityFunc = _createFunc;
        nativeWindow = NULL;
        mainWorkRead = mainWorkWrite = -1;
    }
    
    ~NativeCode() {
        if (callbacks.onDestroy != NULL) {
            callbacks.onDestroy(this);
        }
        if (env != NULL) {
          if (clazz != NULL) {
            env->DeleteGlobalRef(clazz);
          }
          if (javaAssetManager != NULL) {
            env->DeleteGlobalRef(javaAssetManager);
          }
        }
        if (messageQueue != NULL && mainWorkRead >= 0) {
            messageQueue->getLooper()->removeFd(mainWorkRead);
        }
        setSurface(NULL);
        if (mainWorkRead >= 0) close(mainWorkRead);
        if (mainWorkWrite >= 0) close(mainWorkWrite);
        if (dlhandle != NULL) {
            // for now don't unload...  we probably should clean this
            // up and only keep one open dlhandle per proc, since there
            // is really no benefit to unloading the code.
            //dlclose(dlhandle);
        }
    }
    
    void setSurface(jobject _surface) {
        if (_surface != NULL) {
            nativeWindow = android_view_Surface_getNativeWindow(env, _surface);
        } else {
            nativeWindow = NULL;
        }
    }
    
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
};