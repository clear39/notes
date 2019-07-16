public class NativeActivity extends Activity implements SurfaceHolder.Callback2,InputQueue.Callback, OnGlobalLayoutListener {



    @Override
    protected void onCreate(Bundle savedInstanceState) {
        String libname = "main";
        String funcname = "ANativeActivity_onCreate";
        ActivityInfo ai;

        mIMM = getSystemService(InputMethodManager.class);

        getWindow().takeSurface(this); //   SurfaceHolder.Callback2
        getWindow().takeInputQueue(this);   //  InputQueue.Callback
        getWindow().setFormat(PixelFormat.RGB_565);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        mNativeContentView = new NativeContentView(this);
        mNativeContentView.mActivity = this;
        setContentView(mNativeContentView);
        mNativeContentView.requestFocus();
        mNativeContentView.getViewTreeObserver().addOnGlobalLayoutListener(this); //OnGlobalLayoutListener
        
        try {
            ai = getPackageManager().getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
            if (ai.metaData != null) {
                // 获取 jni so 库名
                String ln = ai.metaData.getString(META_DATA_LIB_NAME); // 
                if (ln != null) libname = ln;
                // 获取 函数入口名,默认 为 ANativeActivity_onCreate
                ln = ai.metaData.getString(META_DATA_FUNC_NAME);   // 
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

        //  @   frameworks/base/core/jni/android_app_NativeActivity.cpp
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


//  @   frameworks/base/core/jni/android_app_NativeActivity.cpp
static jlong loadNativeCode_native(JNIEnv* env, jobject clazz, jstring path, jstring funcName,
        jobject messageQueue, jstring internalDataDir, jstring obbDir,
        jstring externalDataDir, jint sdkVersion, jobject jAssetMgr,
        jbyteArray savedState, jobject classLoader, jstring libraryPath) {
    if (kLogTrace) {
        ALOGD("loadNativeCode_native");
    }

    ScopedUtfChars pathStr(env, path);
    std::unique_ptr<NativeCode> code;
    bool needs_native_bridge = false;

    //  @   system/core/libnativeloader/native_loader.cpp
    void* handle = OpenNativeLibrary(env,
                                     sdkVersion,
                                     pathStr.c_str(),
                                     classLoader,
                                     libraryPath,
                                     &needs_native_bridge,
                                     &g_error_msg);

    if (handle == nullptr) {
        ALOGW("NativeActivity LoadNativeLibrary(\"%s\") failed: %s",
              pathStr.c_str(),
              g_error_msg.c_str());
        return 0;
    }

    // 根据字符串查找字符串
    void* funcPtr = NULL;
    const char* funcStr = env->GetStringUTFChars(funcName, NULL);
    if (needs_native_bridge) {
        funcPtr = NativeBridgeGetTrampoline(handle, funcStr, NULL, 0);
    } else {
        funcPtr = dlsym(handle, funcStr);
    }
    /***
     * NativeCode 继承 ANativeActivity
     */
    code.reset(new NativeCode(handle, (ANativeActivity_createFunc*)funcPtr));
    env->ReleaseStringUTFChars(funcName, funcStr);

    if (code->createActivityFunc == NULL) {
        g_error_msg = needs_native_bridge ? NativeBridgeGetError() : dlerror();
        ALOGW("ANativeActivity_onCreate not found: %s", g_error_msg.c_str());
        return 0;
    }

    // 获取java层传入的对应c层的消息队列
    code->messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueue);
    if (code->messageQueue == NULL) {
        g_error_msg = "Unable to retrieve native MessageQueue";
        ALOGW("%s", g_error_msg.c_str());
        return 0;
    }

    /**
     * 创建一对管道
     * 并且将加入主消息队列中，用于处理java层消息
     */
    int msgpipe[2];
    if (pipe(msgpipe)) {
        g_error_msg = android::base::StringPrintf("could not create pipe: %s", strerror(errno));
        ALOGW("%s", g_error_msg.c_str());
        return 0;
    }
    code->mainWorkRead = msgpipe[0];
    code->mainWorkWrite = msgpipe[1];
    int result = fcntl(code->mainWorkRead, F_SETFL, O_NONBLOCK);
    SLOGW_IF(result != 0, "Could not make main work read pipe ""non-blocking: %s", strerror(errno));
    result = fcntl(code->mainWorkWrite, F_SETFL, O_NONBLOCK);
    SLOGW_IF(result != 0, "Could not make main work write pipe ""non-blocking: %s", strerror(errno));
    code->messageQueue->getLooper()->addFd(code->mainWorkRead, 0, ALOOPER_EVENT_INPUT, mainWorkCallback, code.get());

    /***
     * ANativeActivity和NativeCode中各有一个callbacks成员，但是ANativeActivity::callbacks 为指针
     * callbacks 是在入口函数中进行初始化的，可以参考
     * /home/xqli/tools/android-sdk/ndk-bundle/sources/android/native_app_glue/android_native_app_glue.c
     * 中的ANativeActivity_onCreate入口函数
     * 
     */
    code->ANativeActivity::callbacks = &code->callbacks;
    if (env->GetJavaVM(&code->vm) < 0) {
        g_error_msg = "NativeActivity GetJavaVM failed";
        ALOGW("%s", g_error_msg.c_str());
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
    code->assetManager = NdkAssetManagerForJavaObject(env, jAssetMgr);

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

    //调用对应的入口函数
    code->createActivityFunc(code.get(), rawSavedState, rawSavedSize);

    if (rawSavedState != NULL) {
        env->ReleaseByteArrayElements(savedState, rawSavedState, 0);
    }

    return (jlong)code.release();
}

static void write_work(int fd, int32_t cmd, int32_t arg1=0, int32_t arg2=0) {
    ActivityWork work;
    work.cmd = cmd;
    work.arg1 = arg1;
    work.arg2 = arg2;

    if (kLogTrace) {
        ALOGD("write_work: cmd=%d", cmd);
    }

restart:
    int res = write(fd, &work, sizeof(work));
    if (res < 0 && errno == EINTR) {
        goto restart;
    }
    
    if (res == sizeof(work)) return;
    
    if (res < 0) ALOGW("Failed writing to work fd: %s", strerror(errno));
    else ALOGW("Truncated writing to work fd: %d", res);
}

static bool read_work(int fd, ActivityWork* outWork) {
    int res = read(fd, outWork, sizeof(ActivityWork));
    // no need to worry about EINTR, poll loop will just come back again.
    if (res == sizeof(ActivityWork)) return true;
    
    if (res < 0) ALOGW("Failed reading work fd: %s", strerror(errno));
    else ALOGW("Truncated reading work fd: %d", res);
    return false;
}


/*
 * Callback for handling native events on the application's main thread.
 */
static int mainWorkCallback(int fd, int events, void* data) {
    NativeCode* code = (NativeCode*)data;
    if ((events & POLLIN) == 0) {
        return 1;
    }
    /***
    struct ActivityWork {
        int32_t cmd;
        int32_t arg1;
        int32_t arg2;
    };
     */
    ActivityWork work;
    /**
     * 从 code->mainWorkRead 管道中读取 ActivityWork大小，并且进行教研
     */
    if (!read_work(code->mainWorkRead, &work)) {
        return 1;
    }

    if (kLogTrace) {
        ALOGD("mainWorkCallback: cmd=%d", work.cmd);
    }

    switch (work.cmd) {
        case CMD_FINISH: {
            code->env->CallVoidMethod(code->clazz, gNativeActivityClassInfo.finish);
            code->messageQueue->raiseAndClearException(code->env, "finish");
        } break;
        case CMD_SET_WINDOW_FORMAT: {
            code->env->CallVoidMethod(code->clazz,gNativeActivityClassInfo.setWindowFormat, work.arg1);
            code->messageQueue->raiseAndClearException(code->env, "setWindowFormat");
        } break;
        case CMD_SET_WINDOW_FLAGS: {
            code->env->CallVoidMethod(code->clazz,gNativeActivityClassInfo.setWindowFlags, work.arg1, work.arg2);
            code->messageQueue->raiseAndClearException(code->env, "setWindowFlags");
        } break;
        case CMD_SHOW_SOFT_INPUT: {
            code->env->CallVoidMethod(code->clazz,gNativeActivityClassInfo.showIme, work.arg1);
            code->messageQueue->raiseAndClearException(code->env, "showIme");
        } break;
        case CMD_HIDE_SOFT_INPUT: {
            code->env->CallVoidMethod(code->clazz,gNativeActivityClassInfo.hideIme, work.arg1);
            code->messageQueue->raiseAndClearException(code->env, "hideIme");
        } break;
        default:
            ALOGW("Unknown work command: %d", work.cmd);
            break;
    }
    
    return 1;
}



//  @   /home/xqli/tools/android-sdk/ndk-bundle/sources/android/native_app_glue/android_native_app_glue.c
JNIEXPORT
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState,size_t savedStateSize) {
    LOGV("Creating: %p\n", activity);
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onStart = onStart;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onStop;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    activity->callbacks->onLowMemory = onLowMemory;
    activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

    /**
     * 由于 instance 为 void* 类型，可以任意转换 
     */
    activity->instance = android_app_create(activity, savedState, savedStateSize);
}

//  @   /home/xqli/tools/android-sdk/ndk-bundle/sources/android/native_app_glue/android_native_app_glue.c
static struct android_app* android_app_create(ANativeActivity* activity,void* savedState, size_t savedStateSize) {
    struct android_app* android_app = (struct android_app*)malloc(sizeof(struct android_app));
    memset(android_app, 0, sizeof(struct android_app));
    android_app->activity = activity;

    pthread_mutex_init(&android_app->mutex, NULL);
    pthread_cond_init(&android_app->cond, NULL);

    if (savedState != NULL) {
        android_app->savedState = malloc(savedStateSize);
        android_app->savedStateSize = savedStateSize;
        memcpy(android_app->savedState, savedState, savedStateSize);
    }

    /**
     * 创建双管道
     */
    int msgpipe[2];
    if (pipe(msgpipe)) {
        LOGE("could not create pipe: %s", strerror(errno));
        return NULL;
    }

    /**
     * 由于后面创建了子线程，所以这里创建的管道是用于和子线程通信控制
     */
    android_app->msgread = msgpipe[0];
    android_app->msgwrite = msgpipe[1];

    pthread_attr_t attr; 
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    /**
     * android_app_entry 为新创建线程函数入口
     */
    pthread_create(&android_app->thread, &attr, android_app_entry, android_app);

    // Wait for thread to start.
    pthread_mutex_lock(&android_app->mutex);
    while (!android_app->running) {
        pthread_cond_wait(&android_app->cond, &android_app->mutex);
    }
    pthread_mutex_unlock(&android_app->mutex);

    return android_app;
}

//  @   /home/xqli/tools/android-sdk/ndk-bundle/sources/android/native_app_glue/android_native_app_glue.c
static void* android_app_entry(void* param) {
    struct android_app* android_app = (struct android_app*)param;

    android_app->config = AConfiguration_new();
    AConfiguration_fromAssetManager(android_app->config, android_app->activity->assetManager);

    print_cur_config(android_app);

    android_app->cmdPollSource.id = LOOPER_ID_MAIN;
    android_app->cmdPollSource.app = android_app;
    android_app->cmdPollSource.process = process_cmd;
    android_app->inputPollSource.id = LOOPER_ID_INPUT;
    android_app->inputPollSource.app = android_app;
    android_app->inputPollSource.process = process_input;

    //  @   frameworks/base/native/android/looper.cpp
    /**
     * ALOOPER_PREPARE_ALLOW_NON_CALLBACKS 表示 允许 回调接口为空
     *  @   frameworks/native/include/android/looper.h:64:    ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1<<0
     */
    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

    /***
     * @   frameworks/base/native/android/looper.cpp
     * 这里回调为NULL
     *  */  
    ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL,&android_app->cmdPollSource);
    android_app->looper = looper;

    pthread_mutex_lock(&android_app->mutex);
    android_app->running = 1;
    pthread_cond_broadcast(&android_app->cond);
    pthread_mutex_unlock(&android_app->mutex);

    /**
     * 子线程中的main函数以及while循环
     */
    android_main(android_app);

    android_app_destroy(android_app);
    return NULL;
}


static void process_cmd(struct android_app* app, struct android_poll_source* source) {
    int8_t cmd = android_app_read_cmd(app);
    android_app_pre_exec_cmd(app, cmd);
    if (app->onAppCmd != NULL) app->onAppCmd(app, cmd);
    android_app_post_exec_cmd(app, cmd);
}


static void process_input(struct android_app* app, struct android_poll_source* source) {
    AInputEvent* event = NULL;
    while (AInputQueue_getEvent(app->inputQueue, &event) >= 0) {
        LOGV("New input event: type=%d\n", AInputEvent_getType(event));
        if (AInputQueue_preDispatchEvent(app->inputQueue, event)) {
            continue;
        }
        int32_t handled = 0;
        if (app->onInputEvent != NULL) handled = app->onInputEvent(app, event);
        AInputQueue_finishEvent(app->inputQueue, event, handled);
    }
}



/**
 * ALooper 和 Looper 相当于是同一个对象
 */
ALooper* ALooper_prepare(int opts) {
    return Looper_to_ALooper(Looper::prepare(opts).get());
}


/**
 * native_activity demo 的 android_main 函数
 */
void android_main(struct android_app* state) {
    struct engine engine;

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    engine.app = state;

    // Prepare to monitor accelerometer
    engine.sensorManager = AcquireASensorManagerInstance(state);
    engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,ASENSOR_TYPE_ACCELEROMETER);
    engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,state->looper, LOOPER_ID_USER,NULL, NULL);

    if (state->savedState != NULL) {
        // We are starting with a previous saved state; restore from it.
        engine.state = *(struct saved_state*)state->savedState;
    }

    // loop waiting for stuff to do.

    while (1) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events,(void**)&source)) >= 0) {

            /**
             * 如果 source 不为空
             */
            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // If a sensor has data, process it now.
            if (ident == LOOPER_ID_USER) {
                if (engine.accelerometerSensor != NULL) {
                    ASensorEvent event;
                    while (ASensorEventQueue_getEvents(engine.sensorEventQueue,&event, 1) > 0) {
                        LOGI("accelerometer: x=%f y=%f z=%f",event.acceleration.x, event.acceleration.y,event.acceleration.z);
                    }
                }
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                engine_term_display(&engine);
                return;
            }
        }

        if (engine.animating) {
            // Done with events; draw next animation frame.
            engine.state.angle += .01f;
            if (engine.state.angle > 1) {
                engine.state.angle = 0;
            }

            // Drawing is throttled to the screen update rate, so there
            // is no need to do timing here.
            engine_draw_frame(&engine);
        }
    }
}


static void onResume_native(JNIEnv* env, jobject clazz, jlong handle)
{
    if (kLogTrace) {
        ALOGD("onResume_native");
    }
    if (handle != 0) {
        NativeCode* code = (NativeCode*)handle;
        if (code->callbacks.onResume != NULL) {
            code->callbacks.onResume(code);
        }
    }
}
