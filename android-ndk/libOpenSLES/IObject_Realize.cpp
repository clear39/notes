#define ANDROID 1
/**
 * 第二步
 * result = (*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE);
 * 
 * 由 第一步 slCreateEngine 得到 mEngineObject 为  IObject_Itf;  //  @   frameworks/wilhelm/src/itf/IObject.cpp
*/

static SLresult IObject_Realize(SLObjectItf self, SLboolean async)
{
    /**
     * 用于trace追踪
    */
    SL_ENTER_INTERFACE

    /**
     *  #define IObject struct Object_interface  // @   frameworks/wilhelm/src/itfstruct.h
     * 
     * 由于 mEngineObject (IObject_Itf)为 Object_interface 第一个成员，所以可以强制转换
     * 
    */
    IObject *thiz = (IObject *) self;
    SLuint8 state;
    /**
     * CEngine_class    @   frameworks/wilhelm/src/classes.cpp
    */
    const ClassTable *clazz = thiz->mClass;
    bool isSharedEngine = false;
    object_lock_exclusive(thiz);
    // note that SL_OBJECTID_ENGINE and XA_OBJECTID_ENGINE map to same class
    if (clazz == objectIDtoClass(SL_OBJECTID_ENGINE)) {
        // important: the lock order is engine followed by theOneTrueMutex
        int ok = pthread_mutex_lock(&theOneTrueMutex);
        assert(0 == ok);
        isSharedEngine = 1 < theOneTrueRefCount;
        ok = pthread_mutex_unlock(&theOneTrueMutex);
        assert(0 == ok);
    }
    /**
     * 在 void IObject_init(void *self) 中赋值 SL_OBJECT_STATE_UNREALIZED
    */
    state = thiz->mState;
    // Reject redundant calls to Realize, except on a shared engine
    if (SL_OBJECT_STATE_UNREALIZED != state) {
        object_unlock_exclusive(thiz);
        // redundant realize on the shared engine is permitted
        if (isSharedEngine && (SL_OBJECT_STATE_REALIZED == state)) {
            result = SL_RESULT_SUCCESS;
        } else {
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
        }
    } else {
        /**
         * async = SL_BOOLEAN_FALSE
        */
        // Asynchronous: mark operation pending and cancellable
        if (async && (SL_OBJECTID_ENGINE != clazz->mSLObjectID)) {
            state = SL_OBJECT_STATE_REALIZING_1;
        // Synchronous: mark operation pending and non-cancellable
        } else {
            state = SL_OBJECT_STATE_REALIZING_2; // 执行这里
        }
        thiz->mState = state;
        switch (state) {
        case SL_OBJECT_STATE_REALIZING_1: // asynchronous on non-Engine
            .....
            break;
        case SL_OBJECT_STATE_REALIZING_2: // synchronous, or asynchronous on Engine  // 执行这里
            {
                /**
                 *  CEngine_Realize //  @   
                */
                AsyncHook realize = clazz->mRealize;  
                // Note that the mutex is locked on entry to and exit from the realize hook,
                // but the hook is permitted to temporarily unlock the mutex (e.g. for async).
                result = (NULL != realize) ? (*realize)(thiz, async /*=false*/) : SL_RESULT_SUCCESS;
                assert(SL_OBJECT_STATE_REALIZING_2 == thiz->mState);
                state = (SL_RESULT_SUCCESS == result) ? SL_OBJECT_STATE_REALIZED : SL_OBJECT_STATE_UNREALIZED;
                thiz->mState = state;
                /**
                 * 
                */
                slObjectCallback callback = thiz->mCallback;
                void *context = thiz->mContext;
                object_unlock_exclusive(thiz);
                // asynchronous Realize on an Engine is actually done synchronously, but still has
                // callback because there is no thread pool yet to do it asynchronously.
                if (async && (NULL != callback)) { // async = SL_BOOLEAN_FALSE
                    (*callback)(&thiz->mItf, context, SL_OBJECT_EVENT_ASYNC_TERMINATION, result, state,NULL);
                }
            }
            break;
        default:                          // impossible
            object_unlock_exclusive(thiz);
            assert(SL_BOOLEAN_FALSE);
            break;
        }
    }

    SL_LEAVE_INTERFACE
}




/** \brief Hook called by Object::Realize when an engine is realized */

SLresult CEngine_Realize(void *self, SLboolean async)
{
    /**
     * 这里同样是指针第一个成员强行转换
     * 
     * typedef struct CEngine_struct CEngine;  // @frameworks/wilhelm/src/sles_allinclusive.h
     * 
     * CEngine_struct   @   frameworks/wilhelm/src/classes.h
    */
    CEngine *thiz = (CEngine *) self;
    SLresult result;
#ifndef ANDROID
    ......
#endif
    // initialize the thread pool for asynchronous operations
    result = ThreadPool_init(&thiz->mThreadPool, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        thiz->mEngine.mShutdown = SL_BOOLEAN_TRUE;
        (void) pthread_join(thiz->mSyncThread, (void **) NULL);
        return result;
    }
#ifdef ANDROID
    // use checkService() to avoid blocking if audio service is not up yet
    android::sp<android::IBinder> binder =  android::defaultServiceManager()->checkService(android::String16("audio"));
    if (binder == 0) {
        SL_LOGE("CEngine_Realize: binding to audio service failed, service up?");
    } else {
        /**
         * 这里主要初始化了这里
        */
        thiz->mAudioManager = android::interface_cast<android::IAudioManager>(binder);
    }
#endif
#ifdef USE_SDL
    SDL_open(&thiz->mEngine);
#endif
    return SL_RESULT_SUCCESS;
}




/*typedef*/ struct CEngine_struct {
    // mandated implicit interfaces
    IObject mObject;
#ifdef ANDROID
#define INTERFACES_Engine 13 // see MPH_to_Engine in MPH_to.c for list of interfaces
#else
#define INTERFACES_Engine 12 // see MPH_to_Engine in MPH_to.c for list of interfaces
#endif
    SLuint8 mInterfaceStates2[INTERFACES_Engine - INTERFACES_Default];
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IEngine mEngine;
    IEngineCapabilities mEngineCapabilities;
    IThreadSync mThreadSync;
    // mandated explicit interfaces
    IAudioIODeviceCapabilities mAudioIODeviceCapabilities;
    IAudioDecoderCapabilities mAudioDecoderCapabilities;
    IAudioEncoderCapabilities mAudioEncoderCapabilities;
    I3DCommit m3DCommit;
    // optional interfaces
    IDeviceVolume mDeviceVolume;
    // OpenMAX AL mandated implicit interfaces
    IXAEngine mXAEngine;
#ifdef ANDROID
    IAndroidEffectCapabilities mAndroidEffectCapabilities;
#endif
    // OpenMAX AL explicit interfaces
    IVideoDecoderCapabilities mVideoDecoderCapabilities;
    // remaining are per-instance private fields not associated with an interface
    ThreadPool mThreadPool; // for asynchronous operations
    pthread_t mSyncThread;
#if defined(ANDROID)
    // FIXME number of presets will only be saved in IEqualizer, preset names will not be stored
    SLuint32 mEqNumPresets;
    char** mEqPresetNames;
    android::sp<android::IAudioManager> mAudioManager;
#endif
} /*CEngine*/;

