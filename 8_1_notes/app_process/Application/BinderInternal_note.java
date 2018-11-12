//	本地接口	frameworks/base/core/jni/android_util_Binder.cpp:962
public class BinderInternal {
	static WeakReference<GcWatcher> sGcWatcher = new WeakReference<GcWatcher>(new GcWatcher());
    static ArrayList<Runnable> sGcWatchers = new ArrayList<>();
    static Runnable[] sTmpWatchers = new Runnable[1];
    static long sLastGcTime;

    static final class GcWatcher {
        @Override
        protected void finalize() throws Throwable {
            handleGc();
            sLastGcTime = SystemClock.uptimeMillis();
            synchronized (sGcWatchers) {
                sTmpWatchers = sGcWatchers.toArray(sTmpWatchers);
            }
            for (int i=0; i<sTmpWatchers.length; i++) {
                if (sTmpWatchers[i] != null) {
                    sTmpWatchers[i].run();
                }
            }
            sGcWatcher = new WeakReference<GcWatcher>(new GcWatcher());
        }
    }

    public static void addGcWatcher(Runnable watcher) {
        synchronized (sGcWatchers) {
            sGcWatchers.add(watcher);
        }
    }

    /**
     * Add the calling thread to the IPC thread pool.  This function does
     * not return until the current process is exiting.
     */
    public static final native void joinThreadPool();
    
    /**
     * Return the system time (as reported by {@link SystemClock#uptimeMillis
     * SystemClock.uptimeMillis()}) that the last garbage collection occurred
     * in this process.  This is not for general application use, and the
     * meaning of "when a garbage collection occurred" will change as the
     * garbage collector evolves.
     * 
     * @return Returns the time as per {@link SystemClock#uptimeMillis
     * SystemClock.uptimeMillis()} of the last garbage collection.
     */
    public static long getLastGcTime() {
        return sLastGcTime;
    }

    /**
     * Return the global "context object" of the system.  This is usually
     * an implementation of IServiceManager, which you can use to find
     * other services.
     */
    public static final native IBinder getContextObject();
    
    /**
     * Special for system process to not allow incoming calls to run at
     * background scheduling priority.
     * @hide
     */
    public static final native void disableBackgroundScheduling(boolean disable); //这个设置目前不知道其作用？？？？？？？？

    public static final native void setMaxThreads(int numThreads);
    
    //这里是执行gc之后，调用handleGc重置binder的gc标记，
    static native final void handleGc();
    
    //java binder创建一个会累加，当大于200时会激发虚拟机gc动作
    public static void forceGc(String reason) {
        EventLog.writeEvent(2741, reason);
        VMRuntime.getRuntime().requestConcurrentGC();
    }
    
    //jni调用forceBinderGc
    static void forceBinderGc() {
        forceGc("Binder");
    }
}




//本地接口

static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    return javaObjectForIBinder(env, b);
}

static void android_os_BinderInternal_joinThreadPool(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    android::IPCThreadState::self()->joinThreadPool();
}

static void android_os_BinderInternal_disableBackgroundScheduling(JNIEnv* env,jobject clazz, jboolean disable)
{
    IPCThreadState::disableBackgroundScheduling(disable ? true : false);
}

static void android_os_BinderInternal_setMaxThreads(JNIEnv* env,jobject clazz, jint maxThreads)
{
    ProcessState::self()->setThreadPoolMaxThreadCount(maxThreads);
}

static void android_os_BinderInternal_handleGc(JNIEnv* env, jobject clazz)
{
    ALOGV("Gc has executed, clearing binder ops");
    android_atomic_and(0, &gNumRefsCreated);
}