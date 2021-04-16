class AndroidRuntime{

    enum StartMode {
        Zygote,
        SystemServer,
        Application,
        Tool,
    };


}



//	@	frameworks/base/core/jni/AndroidRuntime.cpp


AndroidRuntime::AndroidRuntime(char* argBlockStart, const size_t argBlockLength) :
        mExitWithoutCleanup(false),
        mArgBlockStart(argBlockStart),
        mArgBlockLength(argBlockLength)
{
    SkGraphics::Init();

    // Pre-allocate enough space to hold a fair number of options.
    mOptions.setCapacity(20);

    assert(gCurRuntime == NULL);        // one per process
    gCurRuntime = this;
}

void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
    ALOGD(">>>>>> START %s uid %d <<<<<<\n", className != NULL ? className : "(unknown)", getuid());

    static const String8 startSystemServer("start-system-server");

    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {
           /* track our progress through the boot sequence */
           const int LOG_BOOT_PROGRESS_START = 3000;
           LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,  ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
        }
    }

    const char* rootDir = getenv("ANDROID_ROOT");
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            LOG_FATAL("No root directory specified, and /android does not exist.");
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }

    /* start the virtual machine */
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);


    JNIEnv* env;
    // startVm 实现在 AndroidRuntime
    if (startVm(&mJavaVM, &env, zygote) != 0) {
        return;
    }

    // onVmCreated 实现在 AppRuntime
    onVmCreated(env);

    /*
     * Register android functions.
     */
    // 实现在 AndroidRuntime
    // 
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

    stringClass = env->FindClass("java/lang/String");
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(options.size() + 1, stringClass, NULL);
    assert(strArray != NULL);
    classNameStr = env->NewStringUTF(className);
    assert(classNameStr != NULL);
    env->SetObjectArrayElement(strArray, 0, classNameStr);

    for (size_t i = 0; i < options.size(); ++i) {
        jstring optionsStr = env->NewStringUTF(options.itemAt(i).string());
        assert(optionsStr != NULL);
        env->SetObjectArrayElement(strArray, i + 1, optionsStr);
    }

    /*
     * Start VM.  This thread becomes the main thread of the VM, and will
     * not return until the VM exits.
     */
    char* slashClassName = toSlashClassName(className != NULL ? className : "");
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);
        /* keep going */
    } else {
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




int AndroidRuntime::startVm(JavaVM** pJavaVM, JNIEnv** pEnv, bool zygote)
{
    JavaVMInitArgs initArgs;
    char propBuf[PROPERTY_VALUE_MAX];
    char stackTraceFileBuf[sizeof("-Xstacktracefile:")-1 + PROPERTY_VALUE_MAX];
    char jniOptsBuf[sizeof("-Xjniopts:")-1 + PROPERTY_VALUE_MAX];
    char heapstartsizeOptsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char heapsizeOptsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char heapgrowthlimitOptsBuf[sizeof("-XX:HeapGrowthLimit=")-1 + PROPERTY_VALUE_MAX];
    char heapminfreeOptsBuf[sizeof("-XX:HeapMinFree=")-1 + PROPERTY_VALUE_MAX];
    char heapmaxfreeOptsBuf[sizeof("-XX:HeapMaxFree=")-1 + PROPERTY_VALUE_MAX];
    char usejitOptsBuf[sizeof("-Xusejit:")-1 + PROPERTY_VALUE_MAX];
    char jitmaxsizeOptsBuf[sizeof("-Xjitmaxsize:")-1 + PROPERTY_VALUE_MAX];
    char jitinitialsizeOptsBuf[sizeof("-Xjitinitialsize:")-1 + PROPERTY_VALUE_MAX];
    char jitthresholdOptsBuf[sizeof("-Xjitthreshold:")-1 + PROPERTY_VALUE_MAX];
    char useJitProfilesOptsBuf[sizeof("-Xjitsaveprofilinginfo:")-1 + PROPERTY_VALUE_MAX];
    char jitprithreadweightOptBuf[sizeof("-Xjitprithreadweight:")-1 + PROPERTY_VALUE_MAX];
    char jittransitionweightOptBuf[sizeof("-Xjittransitionweight:")-1 + PROPERTY_VALUE_MAX];
    char hotstartupsamplesOptsBuf[sizeof("-Xps-hot-startup-method-samples:")-1 + PROPERTY_VALUE_MAX];
    char madviseRandomOptsBuf[sizeof("-XX:MadviseRandomAccess:")-1 + PROPERTY_VALUE_MAX];
    char gctypeOptsBuf[sizeof("-Xgc:")-1 + PROPERTY_VALUE_MAX];
    char backgroundgcOptsBuf[sizeof("-XX:BackgroundGC=")-1 + PROPERTY_VALUE_MAX];
    char heaptargetutilizationOptsBuf[sizeof("-XX:HeapTargetUtilization=")-1 + PROPERTY_VALUE_MAX];
    char foregroundHeapGrowthMultiplierOptsBuf[
            sizeof("-XX:ForegroundHeapGrowthMultiplier=")-1 + PROPERTY_VALUE_MAX];
    char cachePruneBuf[sizeof("-Xzygote-max-boot-retry=")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmsImageFlagsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmxImageFlagsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmsFlagsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmxFlagsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char dex2oatCompilerFilterBuf[sizeof("--compiler-filter=")-1 + PROPERTY_VALUE_MAX];
    char dex2oatImageCompilerFilterBuf[sizeof("--compiler-filter=")-1 + PROPERTY_VALUE_MAX];
    char dex2oatThreadsBuf[sizeof("-j")-1 + PROPERTY_VALUE_MAX];
    char dex2oatThreadsImageBuf[sizeof("-j")-1 + PROPERTY_VALUE_MAX];
    char dex2oat_isa_variant_key[PROPERTY_KEY_MAX];
    char dex2oat_isa_variant[sizeof("--instruction-set-variant=") -1 + PROPERTY_VALUE_MAX];
    char dex2oat_isa_features_key[PROPERTY_KEY_MAX];
    char dex2oat_isa_features[sizeof("--instruction-set-features=") -1 + PROPERTY_VALUE_MAX];
    char dex2oatFlagsBuf[PROPERTY_VALUE_MAX];
    char dex2oatImageFlagsBuf[PROPERTY_VALUE_MAX];
    char extraOptsBuf[PROPERTY_VALUE_MAX];
    char voldDecryptBuf[PROPERTY_VALUE_MAX];
    enum {
      kEMDefault,
      kEMIntPortable,
      kEMIntFast,
      kEMJitCompiler,
    } executionMode = kEMDefault;
    char localeOption[sizeof("-Duser.locale=") + PROPERTY_VALUE_MAX];
    char lockProfThresholdBuf[sizeof("-Xlockprofthreshold:")-1 + PROPERTY_VALUE_MAX];
    char nativeBridgeLibrary[sizeof("-XX:NativeBridge=") + PROPERTY_VALUE_MAX];
    char cpuAbiListBuf[sizeof("--cpu-abilist=") + PROPERTY_VALUE_MAX];
    char methodTraceFileBuf[sizeof("-Xmethod-trace-file:") + PROPERTY_VALUE_MAX];
    char methodTraceFileSizeBuf[sizeof("-Xmethod-trace-file-size:") + PROPERTY_VALUE_MAX];
    std::string fingerprintBuf;
    char jdwpProviderBuf[sizeof("-XjdwpProvider:") - 1 + PROPERTY_VALUE_MAX];

    bool checkJni = false;
    property_get("dalvik.vm.checkjni", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        checkJni = true;
    } else if (strcmp(propBuf, "false") != 0) {
        /* property is neither true nor false; fall back on kernel parameter */
        property_get("ro.kernel.android.checkjni", propBuf, "");
        if (propBuf[0] == '1') {
            checkJni = true;
        }
    }
    ALOGV("CheckJNI is %s\n", checkJni ? "ON" : "OFF");
    if (checkJni) {
        /* extended JNI checking */
        addOption("-Xcheck:jni");

        /* with -Xcheck:jni, this provides a JNI function call trace */
        //addOption("-verbose:jni");
    }

    property_get("dalvik.vm.execution-mode", propBuf, "");
    if (strcmp(propBuf, "int:portable") == 0) {
        executionMode = kEMIntPortable;
    } else if (strcmp(propBuf, "int:fast") == 0) {
        executionMode = kEMIntFast;
    } else if (strcmp(propBuf, "int:jit") == 0) {
        executionMode = kEMJitCompiler;
    }

    // If dalvik.vm.stack-trace-dir is set, it enables the "new" stack trace
    // dump scheme and a new file is created for each stack dump. If it isn't set,
    // the old scheme is enabled.
    property_get("dalvik.vm.stack-trace-dir", propBuf, "");
    if (strlen(propBuf) > 0) {
        addOption("-Xusetombstonedtraces");
    } else {
        parseRuntimeOption("dalvik.vm.stack-trace-file", stackTraceFileBuf, "-Xstacktracefile:");
    }

    strcpy(jniOptsBuf, "-Xjniopts:");
    if (parseRuntimeOption("dalvik.vm.jniopts", jniOptsBuf, "-Xjniopts:")) {
        ALOGI("JNI options: '%s'\n", jniOptsBuf);
    }

    /* route exit() to our handler */
    addOption("exit", (void*) runtime_exit);

    /* route fprintf() to our handler */
    addOption("vfprintf", (void*) runtime_vfprintf);

    /* register the framework-specific "is sensitive thread" hook */
    addOption("sensitiveThread", (void*) runtime_isSensitiveThread);

    /* enable verbose; standard options are { jni, gc, class } */
    //addOption("-verbose:jni");
    addOption("-verbose:gc");
    //addOption("-verbose:class");

    /*
     * The default starting and maximum size of the heap.  Larger
     * values should be specified in a product property override.
     */
    parseRuntimeOption("dalvik.vm.heapstartsize", heapstartsizeOptsBuf, "-Xms", "4m");
    parseRuntimeOption("dalvik.vm.heapsize", heapsizeOptsBuf, "-Xmx", "16m");

    parseRuntimeOption("dalvik.vm.heapgrowthlimit", heapgrowthlimitOptsBuf, "-XX:HeapGrowthLimit=");
    parseRuntimeOption("dalvik.vm.heapminfree", heapminfreeOptsBuf, "-XX:HeapMinFree=");
    parseRuntimeOption("dalvik.vm.heapmaxfree", heapmaxfreeOptsBuf, "-XX:HeapMaxFree=");
    parseRuntimeOption("dalvik.vm.heaptargetutilization",
                       heaptargetutilizationOptsBuf,
                       "-XX:HeapTargetUtilization=");

    /* Foreground heap growth multiplier option */
    parseRuntimeOption("dalvik.vm.foreground-heap-growth-multiplier",
                       foregroundHeapGrowthMultiplierOptsBuf,
                       "-XX:ForegroundHeapGrowthMultiplier=");

    /*
     * JIT related options.
     */
    parseRuntimeOption("dalvik.vm.usejit", usejitOptsBuf, "-Xusejit:");
    parseRuntimeOption("dalvik.vm.jitmaxsize", jitmaxsizeOptsBuf, "-Xjitmaxsize:");
    parseRuntimeOption("dalvik.vm.jitinitialsize", jitinitialsizeOptsBuf, "-Xjitinitialsize:");
    parseRuntimeOption("dalvik.vm.jitthreshold", jitthresholdOptsBuf, "-Xjitthreshold:");
    property_get("dalvik.vm.usejitprofiles", useJitProfilesOptsBuf, "");
    if (strcmp(useJitProfilesOptsBuf, "true") == 0) {
        addOption("-Xjitsaveprofilinginfo");
    }

    parseRuntimeOption("dalvik.vm.jitprithreadweight",
                       jitprithreadweightOptBuf,
                       "-Xjitprithreadweight:");

    parseRuntimeOption("dalvik.vm.jittransitionweight",
                       jittransitionweightOptBuf,
                       "-Xjittransitionweight:");

    property_get("dalvik.vm.profilebootimage", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        addOption("-Xps-profile-boot-class-path");
        addOption("-Xps-profile-aot-code");
    }

    /*
     * Madvise related options.
     */
    parseRuntimeOption("dalvik.vm.madvise-random", madviseRandomOptsBuf, "-XX:MadviseRandomAccess:");

    /*
     * Profile related options.
     */
    parseRuntimeOption("dalvik.vm.hot-startup-method-samples", hotstartupsamplesOptsBuf,
            "-Xps-hot-startup-method-samples:");

    property_get("ro.config.low_ram", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
      addOption("-XX:LowMemoryMode");
    }

    parseRuntimeOption("dalvik.vm.gctype", gctypeOptsBuf, "-Xgc:");
    parseRuntimeOption("dalvik.vm.backgroundgctype", backgroundgcOptsBuf, "-XX:BackgroundGC=");

    /*
     * Enable debugging only for apps forked from zygote.
     */
    if (zygote) {
      // Set the JDWP provider and required arguments. By default let the runtime choose how JDWP is
      // implemented. When this is not set the runtime defaults to not allowing JDWP.
      addOption("-XjdwpOptions:suspend=n,server=y");
      parseRuntimeOption("dalvik.vm.jdwp-provider",
                         jdwpProviderBuf,
                         "-XjdwpProvider:",
                         "default");
    }

    parseRuntimeOption("dalvik.vm.lockprof.threshold",
                       lockProfThresholdBuf,
                       "-Xlockprofthreshold:");

    if (executionMode == kEMIntPortable) {
        addOption("-Xint:portable");
    } else if (executionMode == kEMIntFast) {
        addOption("-Xint:fast");
    } else if (executionMode == kEMJitCompiler) {
        addOption("-Xint:jit");
    }

    // If we are booting without the real /data, don't spend time compiling.
    property_get("vold.decrypt", voldDecryptBuf, "");
    bool skip_compilation = ((strcmp(voldDecryptBuf, "trigger_restart_min_framework") == 0) ||
                             (strcmp(voldDecryptBuf, "1") == 0));

    // Extra options for boot.art/boot.oat image generation.
    parseCompilerRuntimeOption("dalvik.vm.image-dex2oat-Xms", dex2oatXmsImageFlagsBuf,
                               "-Xms", "-Ximage-compiler-option");
    parseCompilerRuntimeOption("dalvik.vm.image-dex2oat-Xmx", dex2oatXmxImageFlagsBuf,
                               "-Xmx", "-Ximage-compiler-option");
    if (skip_compilation) {
        addOption("-Ximage-compiler-option");
        addOption("--compiler-filter=assume-verified");
    } else {
        parseCompilerOption("dalvik.vm.image-dex2oat-filter", dex2oatImageCompilerFilterBuf,
                            "--compiler-filter=", "-Ximage-compiler-option");
    }

    // If there is a boot profile, it takes precedence over the image and preloaded classes.
    if (hasFile("/system/etc/boot-image.prof")) {
        addOption("-Ximage-compiler-option");
        addOption("--profile-file=/system/etc/boot-image.prof");
        addOption("-Ximage-compiler-option");
        addOption("--compiler-filter=speed-profile");
    } else {
        // Make sure there is a preloaded-classes file.
        if (!hasFile("/system/etc/preloaded-classes")) {
            ALOGE("Missing preloaded-classes file, /system/etc/preloaded-classes not found: %s\n",
                  strerror(errno));
            return -1;
        }
        addOption("-Ximage-compiler-option");
        addOption("--image-classes=/system/etc/preloaded-classes");

        // If there is a compiled-classes file, push it.
        if (hasFile("/system/etc/compiled-classes")) {
            addOption("-Ximage-compiler-option");
            addOption("--compiled-classes=/system/etc/compiled-classes");
        }

        // If there is a dirty-image-objects file, push it.
        if (hasFile("/system/etc/dirty-image-objects")) {
            addOption("-Ximage-compiler-option");
            addOption("--dirty-image-objects=/system/etc/dirty-image-objects");
        }
    }

    property_get("dalvik.vm.image-dex2oat-flags", dex2oatImageFlagsBuf, "");
    parseExtraOpts(dex2oatImageFlagsBuf, "-Ximage-compiler-option");

    // Extra options for DexClassLoader.
    parseCompilerRuntimeOption("dalvik.vm.dex2oat-Xms", dex2oatXmsFlagsBuf,
                               "-Xms", "-Xcompiler-option");
    parseCompilerRuntimeOption("dalvik.vm.dex2oat-Xmx", dex2oatXmxFlagsBuf,
                               "-Xmx", "-Xcompiler-option");
    if (skip_compilation) {
        addOption("-Xcompiler-option");
        addOption("--compiler-filter=assume-verified");

        // We skip compilation when a minimal runtime is brought up for decryption. In that case
        // /data is temporarily backed by a tmpfs, which is usually small.
        // If the system image contains prebuilts, they will be relocated into the tmpfs. In this
        // specific situation it is acceptable to *not* relocate and run out of the prebuilts
        // directly instead.
        addOption("--runtime-arg");
        addOption("-Xnorelocate");
    } else {
        parseCompilerOption("dalvik.vm.dex2oat-filter", dex2oatCompilerFilterBuf,
                            "--compiler-filter=", "-Xcompiler-option");
    }
    parseCompilerOption("dalvik.vm.dex2oat-threads", dex2oatThreadsBuf, "-j", "-Xcompiler-option");
    parseCompilerOption("dalvik.vm.image-dex2oat-threads", dex2oatThreadsImageBuf, "-j",
                        "-Ximage-compiler-option");

    // The runtime will compile a boot image, when necessary, not using installd. Thus, we need to
    // pass the instruction-set-features/variant as an image-compiler-option.
    // TODO: Find a better way for the instruction-set.
#if defined(__arm__)
    constexpr const char* instruction_set = "arm";
#elif defined(__aarch64__)
    constexpr const char* instruction_set = "arm64";
#elif defined(__mips__) && !defined(__LP64__)
    constexpr const char* instruction_set = "mips";
#elif defined(__mips__) && defined(__LP64__)
    constexpr const char* instruction_set = "mips64";
#elif defined(__i386__)
    constexpr const char* instruction_set = "x86";
#elif defined(__x86_64__)
    constexpr const char* instruction_set = "x86_64";
#else
    constexpr const char* instruction_set = "unknown";
#endif
    // Note: it is OK to reuse the buffer, as the values are exactly the same between
    //       * compiler-option, used for runtime compilation (DexClassLoader)
    //       * image-compiler-option, used for boot-image compilation on device

    // Copy the variant.
    sprintf(dex2oat_isa_variant_key, "dalvik.vm.isa.%s.variant", instruction_set);
    parseCompilerOption(dex2oat_isa_variant_key, dex2oat_isa_variant,
                        "--instruction-set-variant=", "-Ximage-compiler-option");
    parseCompilerOption(dex2oat_isa_variant_key, dex2oat_isa_variant,
                        "--instruction-set-variant=", "-Xcompiler-option");
    // Copy the features.
    sprintf(dex2oat_isa_features_key, "dalvik.vm.isa.%s.features", instruction_set);
    parseCompilerOption(dex2oat_isa_features_key, dex2oat_isa_features,
                        "--instruction-set-features=", "-Ximage-compiler-option");
    parseCompilerOption(dex2oat_isa_features_key, dex2oat_isa_features,
                        "--instruction-set-features=", "-Xcompiler-option");


    property_get("dalvik.vm.dex2oat-flags", dex2oatFlagsBuf, "");
    parseExtraOpts(dex2oatFlagsBuf, "-Xcompiler-option");

    /* extra options; parse this late so it overrides others */
    property_get("dalvik.vm.extra-opts", extraOptsBuf, "");
    parseExtraOpts(extraOptsBuf, NULL);

    /* Set the properties for locale */
    {
        strcpy(localeOption, "-Duser.locale=");
        const std::string locale = readLocale();
        strncat(localeOption, locale.c_str(), PROPERTY_VALUE_MAX);
        addOption(localeOption);
    }

    // Trace files are stored in /data/misc/trace which is writable only in debug mode.
    property_get("ro.debuggable", propBuf, "0");
    if (strcmp(propBuf, "1") == 0) {
        property_get("dalvik.vm.method-trace", propBuf, "false");
        if (strcmp(propBuf, "true") == 0) {
            addOption("-Xmethod-trace");
            parseRuntimeOption("dalvik.vm.method-trace-file",
                               methodTraceFileBuf,
                               "-Xmethod-trace-file:");
            parseRuntimeOption("dalvik.vm.method-trace-file-siz",
                               methodTraceFileSizeBuf,
                               "-Xmethod-trace-file-size:");
            property_get("dalvik.vm.method-trace-stream", propBuf, "false");
            if (strcmp(propBuf, "true") == 0) {
                addOption("-Xmethod-trace-stream");
            }
        }
    }

    // Native bridge library. "0" means that native bridge is disabled.
    property_get("ro.dalvik.vm.native.bridge", propBuf, "");
    if (propBuf[0] == '\0') {
        ALOGW("ro.dalvik.vm.native.bridge is not expected to be empty");
    } else if (strcmp(propBuf, "0") != 0) {
        snprintf(nativeBridgeLibrary, sizeof("-XX:NativeBridge=") + PROPERTY_VALUE_MAX,
                 "-XX:NativeBridge=%s", propBuf);
        addOption(nativeBridgeLibrary);
    }

#if defined(__LP64__)
    const char* cpu_abilist_property_name = "ro.product.cpu.abilist64";
#else
    const char* cpu_abilist_property_name = "ro.product.cpu.abilist32";
#endif  // defined(__LP64__)
    property_get(cpu_abilist_property_name, propBuf, "");
    if (propBuf[0] == '\0') {
        ALOGE("%s is not expected to be empty", cpu_abilist_property_name);
        return -1;
    }
    snprintf(cpuAbiListBuf, sizeof(cpuAbiListBuf), "--cpu-abilist=%s", propBuf);
    addOption(cpuAbiListBuf);

    // Dalvik-cache pruning counter.
    parseRuntimeOption("dalvik.vm.zygote.max-boot-retry", cachePruneBuf,
                       "-Xzygote-max-boot-retry=");

    /*
     * When running with debug.generate-debug-info, add --generate-debug-info to
     * the compiler options so that the boot image, if it is compiled on device,
     * will include native debugging information.
     */
    property_get("debug.generate-debug-info", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        addOption("-Xcompiler-option");
        addOption("--generate-debug-info");
        addOption("-Ximage-compiler-option");
        addOption("--generate-debug-info");
    }

    // The mini-debug-info makes it possible to backtrace through JIT code.
    if (property_get_bool("dalvik.vm.minidebuginfo", 0)) {
        addOption("-Xcompiler-option");
        addOption("--generate-mini-debug-info");
    }

    /*
     * Retrieve the build fingerprint and provide it to the runtime. That way, ANR dumps will
     * contain the fingerprint and can be parsed.
     * Fingerprints are potentially longer than PROPERTY_VALUE_MAX, so parseRuntimeOption() cannot
     * be used here.
     * Do not ever re-assign fingerprintBuf as its c_str() value is stored in mOptions.
     */
    std::string fingerprint = GetProperty("ro.build.fingerprint", "");
    if (!fingerprint.empty()) {
        fingerprintBuf = "-Xfingerprint:" + fingerprint;
        addOption(fingerprintBuf.c_str());
    }

    initArgs.version = JNI_VERSION_1_4;
    initArgs.options = mOptions.editArray();
    initArgs.nOptions = mOptions.size();
    initArgs.ignoreUnrecognized = JNI_FALSE;

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
 * Register android native functions with the VM.
 */
/*static*/ int AndroidRuntime::startReg(JNIEnv* env)
{
    ATRACE_NAME("RegisterAndroidNatives");
    /*
     * This hook causes all future threads created in this process to be
     * attached to the JavaVM.  (This needs to go away in favor of JNI
     * Attach calls.)
     */
    // @ system/core/libutils/Threads.cpp
    // 将javaCreateThreadEtc 赋值给全局变量 gCreateThreadFn
    // 再通过 androidCreateThreadEtc 进行调用
    androidSetCreateThreadFunc((android_create_thread_fn) javaCreateThreadEtc);

    ALOGV("--- registering native functions ---\n");

    /*
     * Every "register" function calls one or more things that return
     * a local reference (e.g. FindClass).  Because we haven't really
     * started the VM yet, they're all getting stored in the base frame
     * and never released.  Use Push/Pop to manage the storage.
     */
    env->PushLocalFrame(200);

    if (register_jni_procs(gRegJNI, NELEM(gRegJNI), env) < 0) {
        env->PopLocalFrame(NULL);
        return -1;
    }
    env->PopLocalFrame(NULL);

    //createJavaThread("fubar", quickTest, (void*) "hello");

    return 0;
}


/*
 * This is invoked from androidCreateThreadEtc() via the callback
 * set with androidSetCreateThreadFunc().
 *
 * We need to create the new thread in such a way that it gets hooked
 * into the VM before it really starts executing.
 */
/*static*/ int AndroidRuntime::javaCreateThreadEtc(
                                android_thread_func_t entryFunction,
                                void* userData,
                                const char* threadName,
                                int32_t threadPriority,
                                size_t threadStackSize,
                                android_thread_id_t* threadId)
{
    void** args = (void**) malloc(3 * sizeof(void*));   // javaThreadShell must free
    int result;

    LOG_ALWAYS_FATAL_IF(threadName == nullptr, "threadName not provided to javaCreateThreadEtc");

    args[0] = (void*) entryFunction;
    args[1] = userData;
    args[2] = (void*) strdup(threadName);   // javaThreadShell must free

    // androidCreateRawThreadEtc 定义在 system/core/libutils/Threads.cpp
    // 内部实现就是调用 pthread_create(linux api) 创建线程 
    result = androidCreateRawThreadEtc(AndroidRuntime::javaThreadShell, args,
        threadName, threadPriority, threadStackSize, threadId);
    return result;
}