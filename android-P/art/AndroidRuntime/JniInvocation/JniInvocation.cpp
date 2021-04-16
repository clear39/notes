
//	@	libnativehelper/JniInvocation.cpp


class JniInvocation {
  static JniInvocation* jni_invocation_;

  jint (*JNI_GetDefaultJavaVMInitArgs_)(void*);
  jint (*JNI_CreateJavaVM_)(JavaVM**, JNIEnv**, void*);
  jint (*JNI_GetCreatedJavaVMs_)(JavaVM**, jsize, jsize*);
}

JniInvocation::JniInvocation() :
    handle_(NULL),
    JNI_GetDefaultJavaVMInitArgs_(NULL),
    JNI_CreateJavaVM_(NULL),
    JNI_GetCreatedJavaVMs_(NULL) {

  LOG_ALWAYS_FATAL_IF(jni_invocation_ != NULL, "JniInvocation instance already initialized");
  jni_invocation_ = this; //赋值静态全局变量
}

// jni_invocation.Init(NULL);
bool JniInvocation::Init(const char* library) {
#ifdef __ANDROID__
  char buffer[PROP_VALUE_MAX];
#else
  char* buffer = NULL;
#endif
  library = GetLibrary(library, buffer);
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
    ALOGW("Falling back from %s to %s after dlopen error: %s", library, kLibraryFallback, dlerror());
    library = kLibraryFallback;
    handle_ = dlopen(library, kDlopenFlags);
    if (handle_ == NULL) {
      ALOGE("Failed to dlopen %s: %s", library, dlerror());
      return false;
    }
  }
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetDefaultJavaVMInitArgs_), "JNI_GetDefaultJavaVMInitArgs")) {
    return false;
  }
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_CreateJavaVM_), "JNI_CreateJavaVM")) {
    return false;
  }
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetCreatedJavaVMs_),"JNI_GetCreatedJavaVMs")) {
    return false;
  }
  return true;
}

// library = NULL
const char* JniInvocation::GetLibrary(const char* library, char* buffer) {
  return GetLibrary(library, buffer, &IsDebuggable, &GetLibrarySystemProperty);
}

int GetLibrarySystemProperty(char* buffer) {
#ifdef __ANDROID__
  return __system_property_get("persist.sys.dalvik.vm.lib.2", buffer);
#else
  ......
#endif
}

// static const char* kLibraryFallback = "libart.so";
const char* JniInvocation::GetLibrary(const char* library, char* buffer, bool (*is_debuggable)(), int (*get_library_system_property)(char* buffer)) {
#ifdef __ANDROID__
  const char* default_library;

  if (!is_debuggable()) {
    // Not a debuggable build.
    // Do not allow arbitrary library. Ignore the library parameter. This
    // will also ignore the default library, but initialize to fallback
    // for cleanliness.
    library = kLibraryFallback;   
    default_library = kLibraryFallback;
  } else {
    // Debuggable build.
    // Accept the library parameter. For the case it is NULL, load the default
    // library from the system property.
    if (buffer != NULL) {
      // 非user版本执行 获取 persist.sys.dalvik.vm.lib.2 属性值
      if (get_library_system_property(buffer) > 0) {
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
  ......
#endif
  if (library == NULL) {
    library = default_library;
  }

  return library;
}






JniInvocation& JniInvocation::GetJniInvocation() {
  LOG_ALWAYS_FATAL_IF(jni_invocation_ == NULL, "Failed to create JniInvocation instance before using JNI invocation API");
  return *jni_invocation_;
}


extern "C" jint JNI_GetDefaultJavaVMInitArgs(void* vm_args) {
  return JniInvocation::GetJniInvocation().JNI_GetDefaultJavaVMInitArgs(vm_args);
}

extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
  return JniInvocation::GetJniInvocation().JNI_CreateJavaVM(p_vm, p_env, vm_args);
}

extern "C" jint JNI_GetCreatedJavaVMs(JavaVM** vms, jsize size, jsize* vm_count) {
  return JniInvocation::GetJniInvocation().JNI_GetCreatedJavaVMs(vms, size, vm_count);
}
