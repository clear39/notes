#define ANDROID 1
/**
 * @    frameworks/wilhelm/src/itf/IEngine.cpp
 * 
*/

static SLresult IEngine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces,const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

    if (NULL == pMix) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pMix = NULL;
        unsigned exposedMask;
        /**
         * 
         * 这里返回  COutputMix_class
        */
        const ClassTable *pCOutputMix_class = objectIDtoClass(SL_OBJECTID_OUTPUTMIX);
        assert(NULL != pCOutputMix_class);

        /**
         * 
        */
        result = checkInterfaces(pCOutputMix_class, numInterfaces,pInterfaceIds, pInterfaceRequired, &exposedMask, NULL);
        if (SL_RESULT_SUCCESS == result) {
            /**
             * 
            */
            COutputMix *thiz = (COutputMix *) construct(pCOutputMix_class, exposedMask, self);
            if (NULL == thiz) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
#ifdef ANDROID
                android_outputMix_create(thiz);
#endif
#ifdef USE_SDL
                IEngine *thisEngine = &thiz->mObject.mEngine->mEngine;
                interface_lock_exclusive(thisEngine);
                bool unpause = false;
                if (NULL == thisEngine->mOutputMix) {
                    thisEngine->mOutputMix = thiz;
                    unpause = true;
                }
                interface_unlock_exclusive(thisEngine);
#endif
                IObject_Publish(&thiz->mObject);
#ifdef USE_SDL
                if (unpause) {
                    // Enable SDL_callback to be called periodically by SDL's internal thread
                    SDL_PauseAudio(0);
                }
#endif
                // return the new output mix object
                *pMix = &thiz->mObject.mItf;
            }
        }
    }

    SL_LEAVE_INTERFACE
}


static const ClassTable COutputMix_class = {
    OutputMix_interfaces,
    INTERFACES_OutputMix,
    MPH_to_OutputMix,
    "OutputMix",
    sizeof(COutputMix),
    SL_OBJECTID_OUTPUTMIX,
    XA_OBJECTID_OUTPUTMIX,
    COutputMix_Realize,
    COutputMix_Resume,
    COutputMix_Destroy,
    COutputMix_PreDestroy
};

