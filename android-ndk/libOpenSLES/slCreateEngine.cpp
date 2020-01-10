

//  @   frameworks/wilhelm/src/sl_entry.cpp
/***
 * 第一步
 * result = slCreateEngine(&mEngineObject, 0, NULL, 0, NULL, NULL);
 * 
 * // mEngineObject 接口封装类
 * mEngineObject    得到    thiz->mItf = &IObject_Itf;  //  @   frameworks/wilhelm/src/itf/IObject.cpp
 * 
 * /
/** \brief slCreateEngine Function */
SL_API SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_GLOBAL

    /**
     *  frameworks/wilhelm/include/SLES/OpenSLES.h:109: #define SL_OBJECTID_ENGINE			((SLuint32) 0x00001001)
     * 
     * objectIDtoClass(SL_OBJECTID_ENGINE) 返回 CEngine_class
    */
    result = liCreateEngine(pEngine, numOptions, pEngineOptions, numInterfaces, pInterfaceIds,pInterfaceRequired, objectIDtoClass(SL_OBJECTID_ENGINE));

    SL_LEAVE_GLOBAL
}

/**
 * @    frameworks/wilhelm/src/sles_allinclusive.h
*/
// Per-class const data shared by all instances of the same class

typedef struct {
    const struct iid_vtable *mInterfaces;   // maps interface index to info about that interface
    SLuint32 mInterfaceCount;  // number of possible interfaces
    const signed char *mMPH_to_index;
    const char * const mName;
    size_t mSize;
    // OpenSL ES and OpenMAX AL object IDs come from different ranges, and some objects such as
    // Engine, Output Mix, LED, and Vibra belong to both APIs, so we keep both object IDs
    SLuint16 mSLObjectID;   // OpenSL ES object ID
    XAuint16 mXAObjectID;   // OpenMAX AL object ID
    // hooks
    AsyncHook mRealize;     // called with mutex locked; can temporarily unlock mutex (for async)
    AsyncHook mResume;      // called with mutex locked; can temporarily unlock mutex (for async)
    VoidHook mDestroy;      // called with mutex locked and must keep mutex locked throughout
    PreDestroyHook mPreDestroy; // called with mutex locked; can temporarily unlock mutex (for wait)
} ClassTable;



/**
 * @    frameworks/wilhelm/src/classes.cpp
 * 
 *  \brief Map SL_OBJECTID to class or NULL if object ID not supported 
 * */
//  ClassTable  @   frameworks/wilhelm/src/sles_allinclusive.h
static const ClassTable CEngine_class = {
    Engine_interfaces,  //  @   frameworks/wilhelm/src/classes.cpp
    INTERFACES_Engine,  //  #define INTERFACES_Engine 13
    MPH_to_Engine,
    "Engine",
    sizeof(CEngine),
    SL_OBJECTID_ENGINE,
    XA_OBJECTID_ENGINE,
    CEngine_Realize,
    CEngine_Resume,
    CEngine_Destroy,
    CEngine_PreDestroy
};

static const ClassTable * const slClasses[] = {
    // Do not change order of these entries; they are in numerical order
    &CEngine_class,
#if USE_PROFILES & USE_PROFILES_OPTIONAL
    &CLEDDevice_class,
    &CVibraDevice_class,
#else
    NULL,
    NULL,
#endif
    &CAudioPlayer_class,
#if (USE_PROFILES & USE_PROFILES_OPTIONAL) || defined(ANDROID)
    &CAudioRecorder_class,
#else
    NULL,
#endif
#if USE_PROFILES & (USE_PROFILES_GAME | USE_PROFILES_PHONE)
    &CMidiPlayer_class,
#else
    NULL,
#endif
#if USE_PROFILES & USE_PROFILES_GAME
    &CListener_class,
    &C3DGroup_class,
#else
    NULL,
    NULL,
#endif
    &COutputMix_class,
#if USE_PROFILES & (USE_PROFILES_GAME | USE_PROFILES_MUSIC)
    &CMetadataExtractor_class
#else
    NULL
#endif
};

/**
 * 
*/
LI_API const ClassTable *objectIDtoClass(SLuint32 objectID)
{
    // object ID is the engine and always present
    assert(NULL != slClasses[0]);
    SLuint32 slObjectID0 = slClasses[0]->mSLObjectID; // SL_OBJECTID_ENGINE
    if ((slObjectID0 <= objectID) && ((slObjectID0 + sizeof(slClasses)/sizeof(slClasses[0])) > objectID)) {
        return slClasses[objectID - slObjectID0]; // SL_OBJECTID_ENGINE - SL_OBJECTID_ENGINE  = 0
    }


    assert(NULL != xaClasses[0]);
    SLuint32 xaObjectID0 = xaClasses[0]->mXAObjectID;
    if ((xaObjectID0 <= objectID) && ((xaObjectID0 + sizeof(xaClasses)/sizeof(xaClasses[0])) >  objectID)) {
        return xaClasses[objectID - xaObjectID0];
    }
    return NULL;
}




/***
 *   @   frameworks/wilhelm/src/entry.cpp
 * 
 *  liCreateEngine( &mEngineObject, 0, NULL, 0, NULL, NULL, CEngine_class[0] );
 * 
 * */
LI_API SLresult liCreateEngine(SLObjectItf *pEngine,
    SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, 
    SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds,
     const SLboolean *pInterfaceRequired,
    const ClassTable *pCEngine_class) //    pCEngine_class = CEngine_class
{
    SLresult result;

    int ok;
    /**
     * 
    */
    ok = pthread_mutex_lock(&theOneTrueMutex);
    assert(0 == ok);
    bool needToUnlockTheOneTrueMutex = true;

    do {

        if (NULL == pEngine) {
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }
        *pEngine = NULL;

        if ((0 < numOptions) && (NULL == pEngineOptions)) { // pEngineOptions = NULL,numOptions=0
            SL_LOGE("numOptions=%u and pEngineOptions=NULL", numOptions);
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }

        // default values
        SLboolean threadSafe = SL_BOOLEAN_TRUE;
        SLboolean lossOfControlGlobal = SL_BOOLEAN_FALSE;

        // process engine options
        SLuint32 i;
        const SLEngineOption *option = pEngineOptions;// NULL
        result = SL_RESULT_SUCCESS;
        /**
         * numOptions 为 0
        */
        for (i = 0; i < numOptions; ++i, ++option) {
            //。。。。。。
        }
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        unsigned exposedMask;
        assert(NULL != pCEngine_class);
        /**
         * pCEngine_class = CEngine_class,numInterfaces = 0,pInterfaceIds=NULL,pInterfaceRequired=NULL,
         * 
         * exposedMask  =   0x407 
        */
        result = checkInterfaces(pCEngine_class, numInterfaces,pInterfaceIds, pInterfaceRequired, &exposedMask, NULL);
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        // if an engine already exists, then increment its ref count
        CEngine *thiz = theOneTrueEngine;
        if (NULL != thiz) {
         ......
        }

        // here when creating the first engine reference
        assert(0 == theOneTrueRefCount);

#ifdef ANDROID
        android::ProcessState::self()->startThreadPool();
#endif

        /**
         * 
        */
        thiz = (CEngine *) construct(pCEngine_class, exposedMask, NULL);
        if (NULL == thiz) {
            result = SL_RESULT_MEMORY_FAILURE;
            break;
        }

        // initialize fields not associated with an interface
        // mThreadPool is initialized in CEngine_Realize
        memset(&thiz->mThreadPool, 0, sizeof(ThreadPool));
        memset(&thiz->mSyncThread, 0, sizeof(pthread_t));
#if defined(ANDROID)
        thiz->mEqNumPresets = 0;
        thiz->mEqPresetNames = NULL;
#endif
        // initialize fields related to an interface
        thiz->mObject.mLossOfControlMask = lossOfControlGlobal ? ~0 : 0;
        thiz->mEngine.mLossOfControlGlobal = lossOfControlGlobal;
        thiz->mEngineCapabilities.mThreadSafe = threadSafe;
        IObject_Publish(&thiz->mObject);  //    ./src/itf/IObject.cpp
        theOneTrueEngine = thiz;
        theOneTrueRefCount = 1;
        /**
         * thiz 为  #define IObject struct Object_interface  // @   frameworks/wilhelm/src/itfstruct.h
         * 
         * 分配空间为sizeof(CEngine)
         *  frameworks/wilhelm/src/sles_allinclusive.h:63:  typedef struct CEngine_struct CEngine;
         * 
         * 这里注意由于 CEngine_struct 第一个成员为 IObject
         * 
        */
        // return the new engine object
        *pEngine = &thiz->mObject.mItf;

    } while(0);

    if (needToUnlockTheOneTrueMutex) {
        ok = pthread_mutex_unlock(&theOneTrueMutex);
        assert(0 == ok);
    }

    return result;
}


/** 
 * @    frameworks/wilhelm/src/sles.cpp
 * \brief Check the interface IDs passed into a Create operation 
 * 
 *  exposedMask  =   0x407 
 *  result = checkInterfaces(CEngine_class[0], 0,NULL, NULL, &exposedMask, NULL);
 * 
 * */
SLresult checkInterfaces(const ClassTable *clazz, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired,
    unsigned *pExposedMask, unsigned *pRequiredMask)
{
    assert(NULL != clazz && NULL != pExposedMask);
    // Initially no interfaces are exposed
    unsigned exposedMask = 0;
    unsigned requiredMask = 0;
    /**
     * @    frameworks/wilhelm/src/classes.cpp
     * mInterfaces 为 ClassTable 第一个成员
     * mInterfaceCount 为第二个成员
    */
    const struct iid_vtable *interfaces = clazz->mInterfaces; // Engine_interfaces 
    SLuint32 interfaceCount = clazz->mInterfaceCount; // frameworks/wilhelm/src/classes.h:174:#define INTERFACES_Engine 13
    SLuint32 i;

    /**
     * 
    */
    // Expose all implicit interfaces
    for (i = 0; i < interfaceCount; ++i) {
        /**
         * 
        */
        switch (interfaces[i].mInterface) {
        case INTERFACE_IMPLICIT:
        case INTERFACE_IMPLICIT_PREREALIZE:
            // there must be an initialization hook present
            if (NULL != MPH_init_table[interfaces[i].mMPH].mInit) {
                exposedMask |= 1 << i;
            }
            break;
        case INTERFACE_EXPLICIT:
        case INTERFACE_DYNAMIC:
        case INTERFACE_UNAVAILABLE:
        case INTERFACE_EXPLICIT_PREREALIZE:
            break;
        default:
            assert(false);
            break;
        }
    }

    /**
     * numInterfaces = 0
    */
    if (0 < numInterfaces) {
        if (NULL == pInterfaceIds || NULL == pInterfaceRequired) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        bool anyRequiredButUnsupported = false;
        // Loop for each requested interface
        for (i = 0; i < numInterfaces; ++i) {
            SLInterfaceID iid = pInterfaceIds[i];
            if (NULL == iid) {
                return SL_RESULT_PARAMETER_INVALID;
            }
            SLboolean isRequired = pInterfaceRequired[i];
            int MPH, index;
            if ((0 > (MPH = IID_to_MPH(iid))) ||
                    // there must be an initialization hook present
                    (NULL == MPH_init_table[MPH].mInit) ||
                    (0 > (index = clazz->mMPH_to_index[MPH])) ||
                    (INTERFACE_UNAVAILABLE == interfaces[index].mInterface)) {
                // Here if interface was not found, or is not available for this object type
                if (isRequired) {
                    // Application said it required the interface, so give up
                    SL_LOGE("class %s interface %u required but unavailable MPH=%d", clazz->mName, i, MPH);
                    anyRequiredButUnsupported = true;
                }
                // Application said it didn't really need the interface, so ignore with warning
                SL_LOGW("class %s interface %u requested but unavailable MPH=%d", clazz->mName, i, MPH);
                continue;
            }
            if (isRequired) {
                requiredMask |= (1 << index);
            }
            // The requested interface was both found and available, so expose it
            exposedMask |= (1 << index);
            // Note that we ignore duplicate requests, including equal and aliased IDs
        }
        if (anyRequiredButUnsupported) {
            return SL_RESULT_FEATURE_UNSUPPORTED;
        }
    }
    *pExposedMask = exposedMask;
    if (NULL != pRequiredMask) {
        *pRequiredMask = requiredMask;
    }
    return SL_RESULT_SUCCESS;
}


/**
 *   @  frameworks/wilhelm/src/entry.cpp
 * 
 * engine = NULL,clazz = CEngine_class
 * 
 * exposedMask  =   0x407 
*/
IObject *construct(const ClassTable *clazz, unsigned exposedMask, SLEngineItf engine)
{
    IObject *thiz;      //  frameworks/wilhelm/src/handlers.h:20:   #define IObject struct Object_interface  // @   frameworks/wilhelm/src/itfstruct.h
    // Do not change this to malloc; we depend on the object being memset to zero
    /**
     *  clazz->mSize = sizeof(CEngine) //  @   frameworks/wilhelm/src/sles_allinclusive.h:63:  typedef struct CEngine_struct CEngine;
     * 
     *  这里注意由于 CEngine_struct(CEngine) 第一个成员为 IObject
     * 
    */
    thiz = (IObject *) calloc(1, clazz->mSize);   

    if (NULL != thiz) {

        SL_LOGV("construct %s at %p", clazz->mName, thiz);

        unsigned lossOfControlMask = 0;
        // a NULL engine means we are constructing the engine
        IEngine *thisEngine = (IEngine *) engine;//  NULL

        if (NULL == thisEngine) {
            thiz->mEngine = (CEngine *) thiz;       //  @   frameworks/wilhelm/src/sles_allinclusive.h:63:  typedef struct CEngine_struct CEngine;
        } else {
            ......
        }

        thiz->mLossOfControlMask = lossOfControlMask;
        thiz->mClass = clazz;  // clazz = CEngine_class

        /**
         * iid_vtable   @  frameworks/wilhelm/src/sles_allinclusive.h
         * 
         *  clazz->mInterfaces = Engine_interfaces @    frameworks/wilhelm/src/classes.cpp
        */
        const struct iid_vtable *x = clazz->mInterfaces; 

        SLuint8 *interfaceStateP = thiz->mInterfaceStates;
        SLuint32 index;
        /**
         * clazz->mInterfaceCount = INTERFACES_Engine //  #define INTERFACES_Engine 13
         * 
         * 注意这里有多个初始化
        */
        for (index = 0; index < clazz->mInterfaceCount; ++index, ++x, exposedMask >>= 1) {
            SLuint8 state;
            /**
             *MPH_init_table  @    frameworks/wilhelm/src/sles.cpp
            */
            // initialize all interfaces with init hooks, even if not exposed


              //{ /* MPH_OBJECT, */ IObject_init, NULL, IObject_deinit, NULL, NULL },
            /**
             *  @ frameworks/wilhelm/src/sles.cpp
            */
            const struct MPH_init *mi = &MPH_init_table[x->mMPH];// MPH_OBJECT  @    frameworks/wilhelm/src/MPH.h:55:#define MPH_OBJECT                     29
            /**
             *MPH_init @  frameworks/wilhelm/src/sles_allinclusive.h:285:struct MPH_init {
            */
            VoidHook init = mi->mInit;// IObject_init
            if (NULL != init) {
                void *self = (char *) thiz + x->mOffset;// offsetof(CEngine, mObject)    // 0
                // IObject does not have an mThis, so [1] is not always defined
                if (index) {
                    ((IObject **) self)[1] = thiz;
                }
                // call the initialization hook
                (*init)(self);// IObject_init(self)     //  ./src/itf/IObject.cpp
                // IObject does not require a call to GetInterface
                if (index) {
                    // This trickery invalidates the v-table until GetInterface
                    ((size_t *) self)[0] ^= ~0;
                }
                /**
                 * 
                 * mi->mExpose = NULL
                 * 
                */
                // if interface is exposed, also call the optional expose hook
                BoolHook expose;
                state = (exposedMask & 1) && ((NULL == (expose = mi->mExpose)) || (*expose)(self)) ? INTERFACE_EXPOSED : INTERFACE_INITIALIZED;
                // FIXME log or report to application if an expose hook on a
                // required explicit interface fails at creation time
            } else {
                state = INTERFACE_UNINITIALIZED;
            }
            *interfaceStateP++ = state;
        }
        // note that the new object is not yet published; creator must call IObject_Publish
    }
    return thiz;
}




void IObject_init(void *self)
{
    IObject *thiz = (IObject *) self;
    thiz->mItf = &IObject_Itf;  //  @   frameworks/wilhelm/src/itf/IObject.cpp
    // initialized in construct:
    // mClass
    // mInstanceID
    // mLossOfControlMask
    // mEngine
    // mInterfaceStates
    thiz->mState = SL_OBJECT_STATE_UNREALIZED;
    thiz->mGottenMask = 1;  // IObject
    thiz->mAttributesMask = 0;
    thiz->mCallback = NULL;
    thiz->mContext = NULL;
#if USE_PROFILES & USE_PROFILES_BASE
    thiz->mPriority = SL_PRIORITY_NORMAL;
    thiz->mPreemptable = SL_BOOLEAN_FALSE;
#endif
    thiz->mStrongRefCount = 0;
    int ok;
    ok = pthread_mutex_init(&thiz->mMutex, (const pthread_mutexattr_t *) NULL);
    assert(0 == ok);
#ifdef USE_DEBUG
    memset(&thiz->mOwner, 0, sizeof(pthread_t));
    thiz->mFile = NULL;
    thiz->mLine = 0;
    thiz->mGeneration = 0;
#endif
    ok = pthread_cond_init(&thiz->mCond, (const pthread_condattr_t *) NULL);
    assert(0 == ok);
}

/***
 * SLObjectItf_     @   frameworks/wilhelm/include/SLES/OpenSLES.h
 *   @   frameworks/wilhelm/src/itf/IObject.cpp
 * */
static const struct SLObjectItf_ IObject_Itf = {
    IObject_Realize,   // Realize(实现)
    IObject_Resume,
    IObject_GetState,
    IObject_GetInterface,
    IObject_RegisterCallback,
    IObject_AbortAsyncOperation,
    IObject_Destroy,
    IObject_SetPriority,
    IObject_GetPriority,
    IObject_SetLossOfControlInterfaces
};

//  @   frameworks/wilhelm/include/SLES/OpenSLES.h
struct SLObjectItf_ {
        SLresult (*Realize) (
                SLObjectItf self,
                SLboolean async
        );
        SLresult (*Resume) (
                SLObjectItf self,
                SLboolean async
        );
        SLresult (*GetState) (
                SLObjectItf self,
                SLuint32 * pState
        );
        SLresult (*GetInterface) (
                SLObjectItf self,
                const SLInterfaceID iid,
                void * pInterface
        );
        SLresult (*RegisterCallback) (
                SLObjectItf self,
                slObjectCallback callback,
                void * pContext
        );
        void (*AbortAsyncOperation) (
                SLObjectItf self
        );
        void (*Destroy) (
                SLObjectItf self
        );
        SLresult (*SetPriority) (
                SLObjectItf self,
                SLint32 priority,
                SLboolean preemptable
        );
        SLresult (*GetPriority) (
                SLObjectItf self,
                SLint32 *pPriority,
                SLboolean *pPreemptable
        );
        SLresult (*SetLossOfControlInterfaces) (
                SLObjectItf self,
                SLint16 numInterfaces,
                SLInterfaceID * pInterfaceIDs,
                SLboolean enabled
        );
};


static const struct SLEngineItf_ IEngine_Itf = {
    IEngine_CreateLEDDevice,
    IEngine_CreateVibraDevice,
    IEngine_CreateAudioPlayer,
    IEngine_CreateAudioRecorder,
    IEngine_CreateMidiPlayer,
    IEngine_CreateListener,
    IEngine_Create3DGroup,
    IEngine_CreateOutputMix,
    IEngine_CreateMetadataExtractor,
    IEngine_CreateExtensionObject,
    IEngine_QueryNumSupportedInterfaces,
    IEngine_QuerySupportedInterfaces,
    IEngine_QueryNumSupportedExtensions,
    IEngine_QuerySupportedExtension,
    IEngine_IsExtensionSupported
};

void IEngine_init(void *self)
{
    SL_LOGV("%s",__func__);

    IEngine *thiz = (IEngine *) self;
    thiz->mItf = &IEngine_Itf;
    // mLossOfControlGlobal is initialized in slCreateEngine
#ifdef USE_SDL
    thiz->mOutputMix = NULL;
#endif
    thiz->mInstanceCount = 1; // ourself
    thiz->mInstanceMask = 0;
    thiz->mChangedMask = 0;
    unsigned i;
    for (i = 0; i < MAX_INSTANCE; ++i) {
        thiz->mInstances[i] = NULL;
    }
    thiz->mShutdown = SL_BOOLEAN_FALSE;
    thiz->mShutdownAck = SL_BOOLEAN_FALSE;
#if _BYTE_ORDER == _BIG_ENDIAN
    thiz->mNativeEndianness = SL_BYTEORDER_BIGENDIAN;
#else
    thiz->mNativeEndianness = SL_BYTEORDER_LITTLEENDIAN;
#endif
}