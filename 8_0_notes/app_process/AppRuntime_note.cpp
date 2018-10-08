//  frameworks/base/cmds/app_process/app_main.cpp
AppRuntime::AppRuntime(char* argBlockStart, const size_t argBlockLength)
    : AndroidRuntime(argBlockStart, argBlockLength)
    , mClass(NULL)
{
}

//	frameworks/base/core/jni/AndroidRuntime.cpp

AndroidRuntime::AndroidRuntime(char* argBlockStart, const size_t argBlockLength) :
        mExitWithoutCleanup(false),
        mArgBlockStart(argBlockStart),	//	char* const mArgBlockStart;
        mArgBlockLength(argBlockLength)	//const size_t mArgBlockLength;
{
    SkGraphics::Init(); //注意初始胡Skia库

    // Pre-allocate enough space to hold a fair number of options.
    mOptions.setCapacity(20); //Vector<JavaVMOption> mOptions;

    assert(gCurRuntime == NULL);        // one per process

    //static AndroidRuntime* gCurRuntime = NULL; 	//	frameworks/base/core/jni/AndroidRuntime.cpp

    //static AndroidRuntime* getRuntime(); 这个获取gCurRuntime
    gCurRuntime = this; //保存AndroidRuntime到全局静态gCurRuntime变量中
}

//如果为APK程序则className = android.app.ActivityThread
void AppRuntime::setClassNameAndArgs(const String8& className, int argc, char * const *argv) {
    mClassName = className;
    for (int i = 0; i < argc; ++i) {
         mArgs.add(String8(argv[i]));
    }
}


int AndroidRuntime::startVm(JavaVM** pJavaVM, JNIEnv** pEnv, bool zygote)
{
    
    //忽略参数解析已经重新打包
    .......


    /*
     * Initialize the VM.
     *
     * The JavaVM* is essentially per-process, and the JNIEnv* is per-thread.
     * If this call succeeds, the VM is ready, and we can start issuing
     * JNI calls.
     */
    if (JNI_CreateJavaVM(pJavaVM, pEnv, &initArgs) < 0) {
        ALOGE("JNI_CreateJavaVM failed\n");
        return -1;
    }

    return 0;
}



/*
 * Start the Android runtime.  This involves starting the virtual machine
 * and calling the "static void main(String[] args)" method in the class
 * named by "className".
 *
 * Passes the main function two arguments, the class name and the specified
 * options string.
 */
void AndroidRuntime::start(const char* className = "com.android.internal.os.RuntimeInit  ", const Vector<String8>& options, bool zygote)
{
    ALOGD(">>>>>> START %s uid %d <<<<<<\n", className != NULL ? className : "(unknown)", getuid());

    static const String8 startSystemServer("start-system-server");

    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {//如果启动为systemserver打印Event log
           /* track our progress through the boot sequence */
           const int LOG_BOOT_PROGRESS_START = 3000;
           LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,  ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
        }
    }

    const char* rootDir = getenv("ANDROID_ROOT");//	/system
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            LOG_FATAL("No root directory specified, and /android does not exist.");
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }

    //const char* kernelHack = getenv("LD_ASSUME_KERNEL");
    //ALOGD("Found LD_ASSUME_KERNEL='%s'\n", kernelHack);

    /* start the virtual machine */
    //虚拟机初始化，这里先不做讨论，日后分析art再分析
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);//加载对应虚拟机库



    JNIEnv* env;
    if (startVm(&mJavaVM, &env, zygote) != 0) {
        return;
    }


    onVmCreated(env);

    /*
     * Register android functions.
     */
    //系统函数本地注册，老掉牙的东西，不做分析
    if (startReg(env) < 0) {
        ALOGE("Unable to register all android natives\n");
        return;
    }

    /*
     * We want to call main() with a String array with arguments in it.
     * At present we have two arguments, the class name and an option string.
     * Create an array to hold them.
     */
    jclass stringClass;
    jobjectArray strArray;
    jstring classNameStr;

    //这里注意，在虚拟机起来时，默认会加载由环境变量BOOTCLASSPATH的所有系统jar包，其中当然包括"java/lang/String"
    stringClass = env->FindClass("java/lang/String");
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(options.size() + 1, stringClass, NULL);//创建用于保存启动参数的String数组
    assert(strArray != NULL);
    classNameStr = env->NewStringUTF(className); //创建用于保存启动类的String
    assert(classNameStr != NULL);
    env->SetObjectArrayElement(strArray, 0, classNameStr);
    //进行赋值
    for (size_t i = 0; i < options.size(); ++i) {
        jstring optionsStr = env->NewStringUTF(options.itemAt(i).string());
        assert(optionsStr != NULL);
        env->SetObjectArrayElement(strArray, i + 1, optionsStr);
    }

    /*
     * Start VM.  This thread becomes the main thread of the VM, and will
     * not return until the VM exits.
     */
    // 这里 toSlashClassName 主要讲字符窜中"."转化成"/",如：com.android.internal.os.RuntimeInit  转为
    char* slashClassName = toSlashClassName(className);
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);
        /* keep going */
    } else {
    	//调用启动类中main函数
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main","([Ljava/lang/String;)V");
        if (startMeth == NULL) {
            ALOGE("JavaVM unable to find main() in '%s'\n", className);
            /* keep going */
        } else {
            env->CallStaticVoidMethod(startClass, startMeth, strArray);

#if 0
            if (env->ExceptionCheck())
                threadExitUncaughtException(env);
#endif
        }
    }
    free(slashClassName);

    ALOGD("Shutting down VM\n");
    if (mJavaVM->DetachCurrentThread() != JNI_OK)
        ALOGW("Warning: unable to detach main thread\n");
    if (mJavaVM->DestroyJavaVM() != 0)
        ALOGW("Warning: VM did not shut down cleanly\n");
}

char* AndroidRuntime::toSlashClassName(const char* className)
{
    char* result = strdup(className);
    for (char* cp = result; *cp != '\0'; cp++) {
        if (*cp == '.') {
            *cp = '/';
        }
    }
    return result;
}


virtual void AppRuntime : :onVmCreated(JNIEnv* env)
{
    if (mClassName.isEmpty()) {
        return; // Zygote. Nothing to do here.  如果为Zygote程序，则什么也不做
    }

    /*
     * This is a little awkward because the JNI FindClass call uses the
     * class loader associated with the native method we're executing in.
     * If called in onStarted (from RuntimeInit.finishInit because we're
     * launching "am", for example), FindClass would see that we're calling
     * from a boot class' native method, and so wouldn't look for the class
     * we're trying to look up in CLASSPATH. Unfortunately it needs to,
     * because the "am" classes are not boot classes.
     *
     * The easiest fix is to call FindClass here, early on before we start
     * executing boot class Java code and thereby deny ourselves access to
     * non-boot classes.
     */
    //将android.app.ActivityThread转化为android/app/ActivityThread
    char* slashClassName = toSlashClassName(mClassName.string()); 
    mClass = env->FindClass(slashClassName);
    if (mClass == NULL) {
        ALOGE("ERROR: could not find class '%s'\n", mClassName.string());
    }
    free(slashClassName);

    mClass = reinterpret_cast<jclass>(env->NewGlobalRef(mClass));
}