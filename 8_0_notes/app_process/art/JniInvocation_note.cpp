
//初始化虚拟机库，这里就是dalvik切到art的核心
class JniInvocation {
	static JniInvocation* jni_invocation_; //私有静态	
	static JniInvocation& GetJniInvocation();
}




bool JniInvocation::Init(const char* library == NULL) {
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
      if (__system_property_get(kLibrarySystemProperty, buffer) > 0) {//static const char* kLibrarySystemProperty = "persist.sys.dalvik.vm.lib.2";  //这里得到值为libart.so
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

  return library;
}