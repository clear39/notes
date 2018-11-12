/**
 * Provides an interface to VM-global, Dalvik-specific features.
 * An application cannot create its own Runtime instance, and must obtain
 * one from the getRuntime method.
 *
 * @hide
 */

//单示例工作模式，通过这个类可以获取虚拟机相关信息	
//本地接口  /art/runtime/native/dalvik_system_VMRuntime.cc
//其中本地方法 
public final class VMRuntime {
	    /**
     * Holds the VMRuntime singleton.
     */
    private static final VMRuntime THE_ONE = new VMRuntime(); 

       /**
     * Returns the object that represents the VM instance's Dalvik-specific
     * runtime environment.
     *
     * @return the runtime object
     */
    public static VMRuntime getRuntime() {
        return THE_ONE;
    }

    /**
     * Returns whether the VM is running in 64-bit mode.
     */
    @FastNative
    public native boolean is64Bit();

    public native void requestConcurrentGC();
    public native void concurrentGC();
    public native void requestHeapTrim();
    public native void trimHeap();
    public native void startHeapTaskProcessor();
    public native void stopHeapTaskProcessor();
    public native void runHeapTasks();


       /**
     * Wait for objects to be finalized(完成).
     *
     * If finalization takes longer than timeout, then the function returns before all objects are  finalized.
     *
     * @param timeout
     *            timeout in nanoseconds of the maximum time to wait until all pending finalizers
     *            are run. If timeout is 0, then there is no timeout. Note that the timeout does
     *            not stop the finalization process, it merely stops the wait.
     *
     * @see #Runtime.runFinalization()
     * @see #wait(long,int)
     */
    public static void runFinalization(long timeout) {
        try {
            FinalizerReference.finalizeAllEnqueued(timeout);
        } catch (InterruptedException e) {
            // Interrupt the current thread without actually throwing the InterruptionException
            // for the caller.
            Thread.currentThread().interrupt();
        }
    }

 


    。。。。。。

}


//最终调用到art/runtime/runtime.cc中Runtime
static void VMRuntime_requestConcurrentGC(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->RequestConcurrentGC(ThreadForEnv(env),gc::kGcCauseBackground,true);
}

static void VMRuntime_trimHeap(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->Trim(ThreadForEnv(env));
}

static void VMRuntime_concurrentGC(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->ConcurrentGC(ThreadForEnv(env), gc::kGcCauseBackground, true);
}

static void VMRuntime_requestHeapTrim(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->RequestTrim(ThreadForEnv(env));
}

static void VMRuntime_trimHeap(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->Trim(ThreadForEnv(env));
}

static void VMRuntime_startHeapTaskProcessor(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->GetTaskProcessor()->Start(ThreadForEnv(env));
}

static void VMRuntime_stopHeapTaskProcessor(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->GetTaskProcessor()->Stop(ThreadForEnv(env));
}

static void VMRuntime_runHeapTasks(JNIEnv* env, jobject) {
  Runtime::Current()->GetHeap()->GetTaskProcessor()->RunAllTasks(ThreadForEnv(env));
}


static jboolean VMRuntime_is64Bit(JNIEnv*, jobject) {
  bool is64BitMode = (sizeof(void*) == sizeof(uint64_t));
  return is64BitMode ? JNI_TRUE : JNI_FALSE;
}

