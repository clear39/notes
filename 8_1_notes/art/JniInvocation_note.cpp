
//初始化虚拟机库，这里就是dalvik切到art的核心
class JniInvocation {
public:
  JniInvocation();

  ~JniInvocation();

  // Initialize JNI invocation API. library should specifiy a valid
  // shared library for opening via dlopen providing a JNI invocation
  // implementation, or null to allow defaulting via
  // persist.sys.dalvik.vm.lib.
  bool Init(const char* library);

  // Exposes which library is actually loaded from the given name. The
  // buffer of size PROPERTY_VALUE_MAX will be used to load the system
  // property for the default library, if necessary. If no buffer is
  // provided, the fallback value will be used.
  static const char* GetLibrary(const char* library, char* buffer);

private:

  bool FindSymbol(void** pointer, const char* symbol);

  static JniInvocation& GetJniInvocation();

  jint JNI_GetDefaultJavaVMInitArgs(void* vmargs);
  jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args);
  jint JNI_GetCreatedJavaVMs(JavaVM** vms, jsize size, jsize* vm_count);

  static JniInvocation* jni_invocation_;

  void* handle_;//这里so库句柄
  //以下是三个函数指针
  jint (*JNI_GetDefaultJavaVMInitArgs_)(void*);
  jint (*JNI_CreateJavaVM_)(JavaVM**, JNIEnv**, void*);
  jint (*JNI_GetCreatedJavaVMs_)(JavaVM**, jsize, jsize*);

  //注意这里是申明以下三个函数为该类的友缘函数，那以下三个函数就能直接范围该类的私有函数
  friend jint JNI_GetDefaultJavaVMInitArgs(void* vm_args);
  friend jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args);
  friend jint JNI_GetCreatedJavaVMs(JavaVM** vms, jsize size, jsize* vm_count);
};

//到处三个全局函数，这三个函数已经申明为 JniInvocation 的友缘函数，
//所以可以访问JNI_GetDefaultJavaVMInitArgs、JNI_CreateJavaVM、JNI_GetCreatedJavaVMs私有函数
//
extern "C" jint JNI_GetDefaultJavaVMInitArgs(void* vm_args) {
  return JniInvocation::GetJniInvocation().JNI_GetDefaultJavaVMInitArgs(vm_args);
}

extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
  return JniInvocation::GetJniInvocation().JNI_CreateJavaVM(p_vm, p_env, vm_args);
}

extern "C" jint JNI_GetCreatedJavaVMs(JavaVM** vms, jsize size, jsize* vm_count) {
  return JniInvocation::GetJniInvocation().JNI_GetCreatedJavaVMs(vms, size, vm_count);
}

//这里再次调用初始化加载so，查找到的函数指针
jint JniInvocation::JNI_GetDefaultJavaVMInitArgs(void* vmargs) {
  return JNI_GetDefaultJavaVMInitArgs_(vmargs);
}

jint JniInvocation::JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
  return JNI_CreateJavaVM_(p_vm, p_env, vm_args);
}

jint JniInvocation::JNI_GetCreatedJavaVMs(JavaVM** vms, jsize size, jsize* vm_count) {
  return JNI_GetCreatedJavaVMs_(vms, size, vm_count);
}



//这里library为NULL
bool JniInvocation::Init(const char* library == NULL) {
#ifdef __ANDROID__
  char buffer[PROP_VALUE_MAX];
#else
  char* buffer = NULL;
#endif
  library = GetLibrary(library, buffer);//这里  library = libart.so
  // Load with RTLD_NODELETE in order to ensure that libart.so is not unmapped when it is closed.
  // This is due to the fact that it is possible that some threads might have yet to finish
  // exiting even after JNI_DeleteJavaVM returns, which can lead to segfaults if the library is
  // unloaded.
  const int kDlopenFlags = RTLD_NOW | RTLD_NODELETE;
  handle_ = dlopen(library, kDlopenFlags);
  if (handle_ == NULL) {
    if (strcmp(library, kLibraryFallback) == 0) {
      // Nothing else to try.
      ALOGE("Failed to dlopen %s: %s", library, dlerror());
      return false;
    }
    // Note that this is enough to get something like the zygote
    // running, we can't property_set here to fix this for the future
    // because we are root and not the system user. See
    // RuntimeInit.commonInit for where we fix up the property to
    // avoid future fallbacks. http://b/11463182
    ALOGW("Falling back from %s to %s after dlopen error: %s",library, kLibraryFallback, dlerror());
    library = kLibraryFallback;
    handle_ = dlopen(library, kDlopenFlags);
    if (handle_ == NULL) {
      ALOGE("Failed to dlopen %s: %s", library, dlerror());
      return false;
    }
  }

  //查找下面三个接口函数
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetDefaultJavaVMInitArgs_),"JNI_GetDefaultJavaVMInitArgs")) {
    return false;
  }
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_CreateJavaVM_),"JNI_CreateJavaVM")) {
    return false;
  }
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetCreatedJavaVMs_),"JNI_GetCreatedJavaVMs")) {
    return false;
  }
  return true;
}


const char* JniInvocation::GetLibrary(const char* library == NULL, char* buffer) {
#ifdef __ANDROID__
  const char* default_library;

  char debuggable[PROP_VALUE_MAX];
  __system_property_get(kDebuggableSystemProperty, debuggable); //	static const char* kDebuggableSystemProperty = "ro.debuggable";

  if (strcmp(debuggable, "1") != 0) {//判断是否为debuggable，
    // Not a debuggable build. 				debuggable不为1时执行，关闭
    // Do not allow arbitrary library. Ignore the library parameter. This
    // will also ignore the default library, but initialize to fallback
    // for cleanliness.
    library = kLibraryFallback;	//static const char* kLibraryFallback = "libart.so";
    default_library = kLibraryFallback;
  } else {
    // Debuggable build.
    // Accept the library parameter. For the case it is NULL, load the default
    // library from the system property.
    if (buffer != NULL) {
      //static const char* kLibrarySystemProperty = "persist.sys.dalvik.vm.lib.2";  //这里得到值为libart.so
      if (__system_property_get(kLibrarySystemProperty, buffer) > 0) {
        default_library = buffer;
      } else {
        default_library = kLibraryFallback;
      }
    } else {
      // No buffer given, just use default fallback.
      default_library = kLibraryFallback;
    }
  }
#else
  UNUSED(buffer);
  const char* default_library = kLibraryFallback;
#endif
  if (library == NULL) {
    library = default_library;
  }

  return library;// libart.so
}



