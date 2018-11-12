package java.lang;



/**
 * Every Java application has a single instance of class
 * <code>Runtime</code> that allows the application to interface with
 * the environment in which the application is running. The current
 * runtime can be obtained from the <code>getRuntime</code> method.
 * <p>
 * An application cannot create its own instance of this class.
 *
 * @author  unascribed
 * @see     java.lang.Runtime#getRuntime()
 * @since   JDK1.0
 */


//本地接口  libcore/ojluni/src/main/native/Runtime.c
public class Runtime {
    private static Runtime currentRuntime = new Runtime();

    public static Runtime getRuntime() {
        return currentRuntime;
    }

    public native void gc();

    public void runFinalization() {
        VMRuntime.runFinalization(0);
    }

}



//art/runtime/openjdkjvm/OpenjdkJvm.cc
JNIEXPORT jlong JNICALL Runtime_freeMemory(JNIEnv *env, jobject this)
{
    return JVM_FreeMemory();
}

JNIEXPORT jlong JNICALL  Runtime_totalMemory(JNIEnv *env, jobject this)
{
    return JVM_TotalMemory();
}

JNIEXPORT jlong JNICALL Runtime_maxMemory(JNIEnv *env, jobject this)
{
    return JVM_MaxMemory();
}

JNIEXPORT void JNICALL Runtime_gc(JNIEnv *env, jobject this)
{
    JVM_GC();
}

JNIEXPORT void JNICALL Runtime_nativeExit(JNIEnv *env, jclass this, jint status)
{
    JVM_Exit(status);
}

JNIEXPORT jstring JNICALL Runtime_nativeLoad(JNIEnv* env, jclass ignored, jstring javaFilename,jobject javaLoader, jstring javaLibrarySearchPath)
{
    return JVM_NativeLoad(env, javaFilename, javaLoader, javaLibrarySearchPath);
}

//  art/runtime/runtime.cc
JNIEXPORT jlong JVM_FreeMemory(void) {
  return art::Runtime::Current()->GetHeap()->GetFreeMemory();
}

JNIEXPORT jlong JVM_TotalMemory(void) {
  return art::Runtime::Current()->GetHeap()->GetTotalMemory();
}

JNIEXPORT jlong JVM_MaxMemory(void) {
  return art::Runtime::Current()->GetHeap()->GetMaxMemory();
}

JNIEXPORT void JVM_GC(void) {
  if (art::Runtime::Current()->IsExplicitGcDisabled()) {
      LOG(INFO) << "Explicit GC skipped.";
      return;
  }
  art::Runtime::Current()->GetHeap()->CollectGarbage(false);
}

JNIEXPORT __attribute__((noreturn)) void JVM_Exit(jint status) {
  LOG(INFO) << "System.exit called, status: " << status;
  art::Runtime::Current()->CallExitHook(status);
  exit(status);
}


JNIEXPORT jstring JVM_NativeLoad(JNIEnv* env, jstring javaFilename, jobject javaLoader, jstring javaLibrarySearchPath) {
  ScopedUtfChars filename(env, javaFilename);
  if (filename.c_str() == NULL) {
    return NULL;
  }

  std::string error_msg;
  {
    art::JavaVMExt* vm = art::Runtime::Current()->GetJavaVM();
    bool success = vm->LoadNativeLibrary(env, filename.c_str(), javaLoader, javaLibrarySearchPath, &error_msg);
    if (success) {
      return nullptr;
    }
  }

  // Don't let a pending exception from JNI_OnLoad cause a CheckJNI issue with NewStringUTF.
  env->ExceptionClear();
  return env->NewStringUTF(error_msg.c_str());
}