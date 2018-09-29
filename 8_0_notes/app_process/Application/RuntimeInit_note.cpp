//	普通应用程序APK入口类
//	frameworks/base/core/java/com/android/internal/os/RuntimeInit.java

/**
 * Main entry point for runtime initialization.  Not for
 * public consumption.
 * @hide
 */
public class RuntimeInit {


    public static final void main(String[] argv) {

	    enableDdms();

	    if (argv.length == 2 && argv[1].equals("application")) {
	        if (DEBUG) Slog.d(TAG, "RuntimeInit: Starting application");
	        redirectLogStreams(); //重定向输出
	    } else {
	        if (DEBUG) Slog.d(TAG, "RuntimeInit: Starting tool");
	    }

	    commonInit();

	    /*
	     * Now that we're running in interpreted code, call back into native code
	     * to run the system.
	     */
	    //native方法	frameworks/base/core/jni/AndroidRuntime.cpp
	    nativeFinishInit();

	    if (DEBUG) Slog.d(TAG, "Leaving RuntimeInit!");
    }

    /**
     * Enable DDMS.
     */
    static final void enableDdms() {
        // Register handlers for DDM messages.
        android.ddm.DdmRegister.registerHandlers();
    }



     /**
     * Redirect System.out and System.err to the Android log.
     */
    public static void redirectLogStreams() {
        System.out.close();
        System.setOut(new AndroidPrintStream(Log.INFO, "System.out"));
        System.err.close();
        System.setErr(new AndroidPrintStream(Log.WARN, "System.err"));
    }



    rotected static final void commonInit() {
        if (DEBUG) Slog.d(TAG, "Entered RuntimeInit!");

        /*
         * set handlers; these apply to all threads in the VM. Apps can replace
         * the default handler, but not the pre handler.
         */
        Thread.setUncaughtExceptionPreHandler(new LoggingHandler());
        Thread.setDefaultUncaughtExceptionHandler(new KillApplicationHandler());

        /*
         * Install a TimezoneGetter subclass for ZoneInfo.db
         */
        TimezoneGetter.setInstance(new TimezoneGetter() {
            @Override
            public String getId() {
                return SystemProperties.get("persist.sys.timezone");
            }
        });
        TimeZone.setDefault(null);

        /*
         * Sets handler for java.util.logging to use Android log facilities.
         * The odd "new instance-and-then-throw-away" is a mirror of how
         * the "java.util.logging.config.class" system property works. We
         * can't use the system property here since the logger has almost
         * certainly already been initialized.
         */
        LogManager.getLogManager().reset();
        new AndroidConfig();

        /*
         * Sets the default HTTP User-Agent used by HttpURLConnection.
         */
        String userAgent = getDefaultUserAgent();
        System.setProperty("http.agent", userAgent);

        /*
         * Wire socket tagging to traffic stats.
         */
        NetworkManagementSocketTagger.install();

        /*
         * If we're running in an emulator launched with "-trace", put the
         * VM into emulator trace profiling mode so that the user can hit
         * F9/F10 at any time to capture traces.  This has performance
         * consequences, so it's not something you want to do always.
         */
        String trace = SystemProperties.get("ro.kernel.android.tracing");
        if (trace.equals("1")) {
            Slog.i(TAG, "NOTE: emulator trace profiling enabled");
            Debug.enableEmulatorTraceOutput();
        }

        initialized = true;
    }




}



/*
 * Code written in the Java Programming Language calls here from main().
 */
//	frameworks/base/core/jni/AndroidRuntime.cpp
static void com_android_internal_os_RuntimeInit_nativeFinishInit(JNIEnv* env, jobject clazz)
{
    gCurRuntime->onStarted();
}


virtual void AppRuntime::onStarted()
{
	//启动binder通讯机制
    sp<ProcessState> proc = ProcessState::self();
    ALOGV("App process: starting thread pool.\n");
    proc->startThreadPool();

    AndroidRuntime* ar = AndroidRuntime::getRuntime();// 这里 ar == gCurRuntime
    ar->callMain(mClassName, mClass, mArgs); //mClassName == android.app.ActivityThread

    IPCThreadState::self()->stopProcess();
    //??? 这里没有进行初始化 为什么要stopProcess
    hardware::IPCThreadState::self()->stopProcess();
}



// 注意这里className是由setClassNameAndArgs传入   android.app.ActivityThread
//clazz 是在AppRuntime : :onVmCreated初始化的
//这里注意args为前面通过setClassNameAndArgs设置的 
status_t AndroidRuntime::callMain(const String8& className, jclass clazz,const Vector<String8>& args)
{
    JNIEnv* env;
    jmethodID methodId;

    ALOGD("Calling main entry %s", className.string());

    env = getJNIEnv();
    if (clazz == NULL || env == NULL) {
        return UNKNOWN_ERROR;
    }

    methodId = env->GetStaticMethodID(clazz, "main", "([Ljava/lang/String;)V");
    if (methodId == NULL) {
        ALOGE("ERROR: could not find method %s.main(String[])\n", className.string());
        return UNKNOWN_ERROR;
    }

    /*
     * We want to call main() with a String array with our arguments in it.
     * Create an array and populate it.
     */
    jclass stringClass;
    jobjectArray strArray;

    const size_t numArgs = args.size();
    stringClass = env->FindClass("java/lang/String");
    strArray = env->NewObjectArray(numArgs, stringClass, NULL);

    for (size_t i = 0; i < numArgs; i++) {
        jstring argStr = env->NewStringUTF(args[i].string());
        env->SetObjectArrayElement(strArray, i, argStr);
    }

    env->CallStaticVoidMethod(clazz, methodId, strArray); //这里开始进入android.app.ActivityThread.main()函数中执行
    return NO_ERROR;
}
