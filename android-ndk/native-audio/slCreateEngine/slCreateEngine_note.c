
// create the engine and output mix objects
void Java_com_example_nativeaudio_NativeAudio_createEngine(JNIEnv* env, jclass clazz)
{
    SLresult result;
    ALOGD("%s %d %p",__func__,&engineObject,engineObject);
    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,&outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(outputMixEnvironmentalReverb, &reverbSettings);
        (void)result;
    }
    // ignore unsuccessful result codes for environmental reverb, as it is optional for this example

}





//	frameworks/wilhelm/include/SLES/OpenSLES.h:105:typedef const struct SLObjectItf_ * const * SLObjectItf;
//	static SLObjectItf engineObject = NULL;		
//  result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL); // create engine

//	@	frameworks/wilhelm/src/sl_entry.cpp    // SLObjectItf为一个接口定义结构体
SL_API SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_GLOBAL	//	@frameworks/wilhelm/src/trace.h:35:#define SL_ENTER_GLOBAL SLresult result;


    //	@frameworks/wilhelm/include/SLES/OpenSLES.h:109:#define SL_OBJECTID_ENGINE			((SLuint32) 0x00001001)
    // 由objectIDtoClass（SL_OBJECTID_ENGINE） 得到 ClassTable CEngine_class
    result = liCreateEngine(pEngine, numOptions, pEngineOptions, numInterfaces, pInterfaceIds,pInterfaceRequired, objectIDtoClass(SL_OBJECTID_ENGINE));

    SL_LEAVE_GLOBAL	//frameworks/wilhelm/src/trace.h:36:#define SL_LEAVE_GLOBAL return result;
}





static const struct iid_vtable Engine_interfaces[INTERFACES_Engine] = {                                                                                                                                       
    {MPH_OBJECT, INTERFACE_IMPLICIT_PREREALIZE, offsetof(CEngine, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,offsetof(CEngine, mDynamicInterfaceManagement)},
    {MPH_ENGINE, INTERFACE_IMPLICIT, offsetof(CEngine, mEngine)},
    {MPH_ENGINECAPABILITIES, INTERFACE_IMPLICIT_BASE, offsetof(CEngine, mEngineCapabilities)},
    {MPH_THREADSYNC, INTERFACE_IMPLICIT_BASE, offsetof(CEngine, mThreadSync)},
    {MPH_AUDIOIODEVICECAPABILITIES, INTERFACE_IMPLICIT_BASE,offsetof(CEngine, mAudioIODeviceCapabilities)},
    {MPH_AUDIODECODERCAPABILITIES, INTERFACE_EXPLICIT_BASE,offsetof(CEngine, mAudioDecoderCapabilities)},
    {MPH_AUDIOENCODERCAPABILITIES, INTERFACE_EXPLICIT_BASE,offsetof(CEngine, mAudioEncoderCapabilities)},
    {MPH_3DCOMMIT, INTERFACE_EXPLICIT_GAME, offsetof(CEngine, m3DCommit)},
    {MPH_DEVICEVOLUME, INTERFACE_OPTIONAL, offsetof(CEngine, mDeviceVolume)},
    {MPH_XAENGINE, INTERFACE_IMPLICIT, offsetof(CEngine, mXAEngine)},
#ifdef ANDROID
    {MPH_ANDROIDEFFECTCAPABILITIES, INTERFACE_EXPLICIT,offsetof(CEngine, mAndroidEffectCapabilities)},
#endif
    {MPH_XAVIDEODECODERCAPABILITIES, INTERFACE_EXPLICIT,offsetof(CEngine, mVideoDecoderCapabilities)},
};



//  @frameworks/wilhelm/src/classes.cpp
static const ClassTable CEngine_class = {                                                                                                                                                                     
    Engine_interfaces,   
    INTERFACES_Engine,   
    MPH_to_Engine,       
    "Engine",            
    sizeof(CEngine),     
    SL_OBJECTID_ENGINE,     //  mSLObjectID 
    XA_OBJECTID_ENGINE,     // mXAObjectID
    CEngine_Realize,     
    CEngine_Resume,      
    CEngine_Destroy,     
    CEngine_PreDestroy   
};


static const ClassTable * const slClasses[] = { 
    // Do not change order of these entries; they are in numerical order
    &CEngine_class,                             //必须
#if USE_PROFILES & USE_PROFILES_OPTIONAL
    &CLEDDevice_class,
    &CVibraDevice_class,
#else
    NULL,
    NULL,
#endif
    &CAudioPlayer_class,                        //必须
#if (USE_PROFILES & USE_PROFILES_OPTIONAL) || defined(ANDROID)
    &CAudioRecorder_class,                      //必须
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
    &COutputMix_class,                          //必须
#if USE_PROFILES & (USE_PROFILES_GAME | USE_PROFILES_MUSIC)
    &CMetadataExtractor_class                                                                                                                                                                                 
#else
    NULL
#endif
};

LI_API const ClassTable *objectIDtoClass(SLuint32 objectID)
{
    // object ID is the engine and always present
    assert(NULL != slClasses[0]);
    SLuint32 slObjectID0 = slClasses[0]->mSLObjectID;
    if ((slObjectID0 <= objectID) && ((slObjectID0 + sizeof(slClasses)/sizeof(slClasses[0])) > objectID)) {
        return slClasses[objectID - slObjectID0];
    }
    assert(NULL != xaClasses[0]);

    SLuint32 xaObjectID0 = xaClasses[0]->mXAObjectID;
    if ((xaObjectID0 <= objectID) && ((xaObjectID0 + sizeof(xaClasses)/sizeof(xaClasses[0])) > objectID)) {
        return xaClasses[objectID - xaObjectID0];
    }
    return NULL;
}



//	@frameworks/wilhelm/src/entry.cpp
LI_API SLresult liCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired,
    const ClassTable *pCEngine_class) //    pCEngine_class = CEngine_class
{
    SLresult result;

    int ok;
    ok = pthread_mutex_lock(&theOneTrueMutex);
    assert(0 == ok);
    bool needToUnlockTheOneTrueMutex = true;

    do {

        if (NULL == pEngine) {
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }
        *pEngine = NULL;

        //numOptions 为0；pEngineOptions为NULL
        if ((0 < numOptions) && (NULL == pEngineOptions)) {
            SL_LOGE("numOptions=%u and pEngineOptions=NULL", numOptions);
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }

        // default values
        SLboolean threadSafe = SL_BOOLEAN_TRUE;
        SLboolean lossOfControlGlobal = SL_BOOLEAN_FALSE;

        // process engine options
        SLuint32 i;
        const SLEngineOption *option = pEngineOptions;
        result = SL_RESULT_SUCCESS;
        for (i = 0; i < numOptions; ++i, ++option) { // numOptions == 0
            ......
        }
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        unsigned exposedMask;
        assert(NULL != pCEngine_class);


        //	@frameworks/wilhelm/src/sles.cpp:151
        // 这里其实就是同过pCEngine_class 计算的到 exposedMask 值
        result = checkInterfaces(pCEngine_class, numInterfaces,pInterfaceIds, pInterfaceRequired, &exposedMask, NULL);
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        // if an engine already exists, then increment its ref count
        // 如果theOneTrueEngine已经存在，则增加计数
        CEngine *thiz = theOneTrueEngine;
        if (NULL != thiz) {
            ......
        }

        // here when creating the first engine reference
        assert(0 == theOneTrueRefCount);

#ifdef ANDROID
        android::ProcessState::self()->startThreadPool();
#endif
        //构造CEngine
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
        IObject_Publish(&thiz->mObject);
        theOneTrueEngine = thiz;
        theOneTrueRefCount = 1;
        // return the new engine object
        *pEngine = &thiz->mObject.mItf;

    } while(0);

    if (needToUnlockTheOneTrueMutex) {
        ok = pthread_mutex_unlock(&theOneTrueMutex);
        assert(0 == ok);
    }

    return result;
}


// 这个函数的作用 通过 clazz 计算得到 pExposedMask 值，作用是方便 后面 construct 函数中 ，初始化
SLresult checkInterfaces(const ClassTable *clazz, SLuint32 numInterfaces,const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired,
    unsigned *pExposedMask, unsigned *pRequiredMask)
{
    assert(NULL != clazz && NULL != pExposedMask);
    // Initially no interfaces are exposed
    unsigned exposedMask = 0;
    unsigned requiredMask = 0;
    //这里赋值
    const struct iid_vtable *interfaces = clazz->mInterfaces;
    SLuint32 interfaceCount = clazz->mInterfaceCount;

    SLuint32 i;
    // Expose all implicit interfaces
    for (i = 0; i < interfaceCount; ++i) {
    ┊   switch (interfaces[i].mInterface) {
    ┊   case INTERFACE_IMPLICIT:
    ┊   case INTERFACE_IMPLICIT_PREREALIZE:
    ┊   ┊   // there must be an initialization hook present
    ┊   ┊   if (NULL != MPH_init_table[interfaces[i].mMPH].mInit) {
    ┊   ┊   ┊   exposedMask |= 1 << i;
    ┊   ┊   }
    ┊   ┊   break;
    ┊   case INTERFACE_EXPLICIT:
    ┊   case INTERFACE_DYNAMIC:
    ┊   case INTERFACE_UNAVAILABLE:
    ┊   case INTERFACE_EXPLICIT_PREREALIZE:                                                                                                                                                                   
    ┊   ┊   break;
    ┊   default:
    ┊   ┊   assert(false);
    ┊   ┊   break;
    ┊   }
    }
    if (0 < numInterfaces) {// numInterfaces = 0;
    ┊   ......
    }
    *pExposedMask = exposedMask;
    if (NULL != pRequiredMask) { // NULL
    ┊   *pRequiredMask = requiredMask;
    }
    return SL_RESULT_SUCCESS;
}



IObject *construct(const ClassTable *clazz, unsigned exposedMask, SLEngineItf engine)
{
    IObject *thiz;                                                                                                                                                                                            
    // Do not change this to malloc; we depend on the object being memset to zero
    thiz = (IObject *) calloc(1, clazz->mSize);   // sizeof()
    if (NULL != thiz) {
    ┊   SL_LOGV("construct %s at %p", clazz->mName, thiz);
    ┊   unsigned lossOfControlMask = 0;
    ┊   // a NULL engine means we are constructing the engine
    ┊   IEngine *thisEngine = (IEngine *) engine;
    ┊   if (NULL == thisEngine) { // 这里为engine = NULL
    ┊   ┊   // thisEngine = &((CEngine *) thiz)->mEngine;
    ┊   ┊   thiz->mEngine = (CEngine *) thiz;
    ┊   } else {
    ┊   ┊   thiz->mEngine = (CEngine *) thisEngine->mThis;
    ┊   ┊   interface_lock_exclusive(thisEngine);
    ┊   ┊   if (MAX_INSTANCE <= thisEngine->mInstanceCount) {
    ┊   ┊   ┊   SL_LOGE("Too many objects");
    ┊   ┊   ┊   interface_unlock_exclusive(thisEngine);
    ┊   ┊   ┊   free(thiz);
    ┊   ┊   ┊   return NULL;
    ┊   ┊   }
    ┊   ┊   // pre-allocate a pending slot, but don't assign bit from mInstanceMask yet
    ┊   ┊   ++thisEngine->mInstanceCount;
    ┊   ┊   assert(((unsigned) ~0) != thisEngine->mInstanceMask);
    ┊   ┊   interface_unlock_exclusive(thisEngine);
    ┊   ┊   // const, no lock needed
    ┊   ┊   if (thisEngine->mLossOfControlGlobal) {
    ┊   ┊   ┊   lossOfControlMask = ~0;
    ┊   ┊   }
    ┊   }


    ┊   thiz->mLossOfControlMask = lossOfControlMask;
    ┊   thiz->mClass = clazz;


    ┊   const struct iid_vtable *x = clazz->mInterfaces;
    ┊   SLuint8 *interfaceStateP = thiz->mInterfaceStates;
    ┊   SLuint32 index;
    ┊   for (index = 0; index < clazz->mInterfaceCount; ++index, ++x, exposedMask >>= 1) { //   n>>=1等价于n=n>>1
    ┊   ┊   SLuint8 state;
    ┊   ┊   // initialize all interfaces with init hooks, even if not exposed
    ┊   ┊   const struct MPH_init *mi = &MPH_init_table[x->mMPH];
    ┊   ┊   VoidHook init = mi->mInit;
    ┊   ┊   if (NULL != init) {
    ┊   ┊   ┊   void *self = (char *) thiz + x->mOffset;
    ┊   ┊   ┊   // IObject does not have an mThis, so [1] is not always defined
    ┊   ┊   ┊   if (index) {
    ┊   ┊   ┊   ┊   ((IObject **) self)[1] = thiz;
    ┊   ┊   ┊   }
    ┊   ┊   ┊   // call the initialization hook
    ┊   ┊   ┊   (*init)(self);  //进行初始化
    ┊   ┊   ┊   // IObject does not require a call to GetInterface
    ┊   ┊   ┊   if (index) {
    ┊   ┊   ┊   ┊   // This trickery invalidates the v-table until GetInterface
    ┊   ┊   ┊   ┊   ((size_t *) self)[0] ^= ~0;
    ┊   ┊   ┊   }
    ┊   ┊   ┊   // if interface is exposed, also call the optional expose hook
    ┊   ┊   ┊   BoolHook expose;
    ┊   ┊   ┊   state = (exposedMask & 1) && ((NULL == (expose = mi->mExpose)) || (*expose)(self)) ? INTERFACE_EXPOSED : INTERFACE_INITIALIZED;
    ┊   ┊   ┊   // FIXME log or report to application if an expose hook on a
    ┊   ┊   ┊   // required explicit interface fails at creation time
    ┊   ┊   } else {
    ┊   ┊   ┊   state = INTERFACE_UNINITIALIZED;
    ┊   ┊   }
    ┊   ┊   *interfaceStateP++ = state;
    ┊   }
    ┊   // note that the new object is not yet published; creator must call IObject_Publish
    }
    return thiz;
}      




void IObject_Publish(IObject *thiz)
{
    IEngine *thisEngine = &thiz->mEngine->mEngine;
    interface_lock_exclusive(thisEngine);
    // construct earlier reserved a pending slot, but did not choose the actual slot number
    unsigned availMask = ~thisEngine->mInstanceMask;
    assert(availMask);
    unsigned i = ctz(availMask);
    assert(MAX_INSTANCE > i);
    assert(NULL == thisEngine->mInstances[i]);
    thisEngine->mInstances[i] = thiz;
    thisEngine->mInstanceMask |= 1 << i;
    // avoid zero as a valid instance ID
    thiz->mInstanceID = i + 1;
    interface_unlock_exclusive(thisEngine);
} 