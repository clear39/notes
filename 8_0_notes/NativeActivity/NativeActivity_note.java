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
