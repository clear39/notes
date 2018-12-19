//	frameworks/wilhelm/include/SLES/OpenSLES.h:105:typedef const struct SLObjectItf_ * const * SLObjectItf;
//	static SLObjectItf engineObject = NULL;		
//  result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL); // create engine

//	@	frameworks/wilhelm/src/sl_entry.cpp
SL_API SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_GLOBAL	//	@frameworks/wilhelm/src/trace.h:35:#define SL_ENTER_GLOBAL SLresult result;


    //	@frameworks/wilhelm/include/SLES/OpenSLES.h:109:#define SL_OBJECTID_ENGINE			((SLuint32) 0x00001001)
    result = liCreateEngine(pEngine, numOptions, pEngineOptions, numInterfaces, pInterfaceIds,pInterfaceRequired, objectIDtoClass(SL_OBJECTID_ENGINE));

    SL_LEAVE_GLOBAL	//frameworks/wilhelm/src/trace.h:36:#define SL_LEAVE_GLOBAL return result;
}


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
    const ClassTable *pCEngine_class)
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
        for (i = 0; i < numOptions; ++i, ++option) {
            switch (option->feature) {
            case SL_ENGINEOPTION_THREADSAFE:
                threadSafe = SL_BOOLEAN_FALSE != (SLboolean) option->data; // normalize
                break;
            case SL_ENGINEOPTION_LOSSOFCONTROL:
                lossOfControlGlobal = SL_BOOLEAN_FALSE != (SLboolean) option->data; // normalize
                break;
            default:
                SL_LOGE("unknown engine option: feature=%u data=%u", option->feature, option->data);
                result = SL_RESULT_PARAMETER_INVALID;
                break;
            }
        }
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        unsigned exposedMask;
        assert(NULL != pCEngine_class);


        //	@frameworks/wilhelm/src/sles.cpp:151
        result = checkInterfaces(pCEngine_class, numInterfaces,pInterfaceIds, pInterfaceRequired, &exposedMask, NULL);
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        // if an engine already exists, then increment its ref count
        CEngine *thiz = theOneTrueEngine;
        if (NULL != thiz) {
            assert(0 < theOneTrueRefCount);
            ++theOneTrueRefCount;

            // In order to update the engine object, we need to lock it,
            // but that would violate the lock order and potentially deadlock.
            // So we unlock now and note that it should not be unlocked later.
            ok = pthread_mutex_unlock(&theOneTrueMutex);
            assert(0 == ok);
            needToUnlockTheOneTrueMutex = false;
            object_lock_exclusive(&thiz->mObject);

            // now expose additional interfaces not requested by the earlier engine create
            const struct iid_vtable *x = pCEngine_class->mInterfaces;
            SLuint8 *interfaceStateP = thiz->mObject.mInterfaceStates;
            SLuint32 index;
            for (index = 0; index < pCEngine_class->mInterfaceCount; ++index, ++x,exposedMask >>= 1, ++interfaceStateP) {
                switch (*interfaceStateP) {
                case INTERFACE_EXPOSED:         // previously exposed
                    break;
                case INTERFACE_INITIALIZED:     // not exposed during the earlier create
                    if (exposedMask & 1) {
                        const struct MPH_init *mi = &MPH_init_table[x->mMPH];
                        BoolHook expose = mi->mExpose;
                        if ((NULL == expose) || (*expose)((char *) thiz + x->mOffset)) {
                            *interfaceStateP = INTERFACE_EXPOSED;
                        }
                        // FIXME log or report to application that expose hook failed
                    }
                    break;
                case INTERFACE_UNINITIALIZED:   // no init hook
                    break;
                default:                        // impossible
                    assert(false);
                    break;
                }
            }
            object_unlock_exclusive(&thiz->mObject);
            // return the shared engine object
            *pEngine = &thiz->mObject.mItf;
            break;
        }

        // here when creating the first engine reference
        assert(0 == theOneTrueRefCount);

#ifdef ANDROID
        android::ProcessState::self()->startThreadPool();
#endif

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