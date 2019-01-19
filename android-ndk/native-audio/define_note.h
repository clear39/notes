// 返回值定义

//	@frameworks/wilhelm/include/SLES/OpenSLES.h:85:	typedef SLuint32					SLresult;

//	@frameworks/wilhelm/include/SLES/OpenSLES.h:
#define SL_RESULT_SUCCESS               ((SLuint32) 0x00000000)                                                                                                                                          
#define SL_RESULT_PRECONDITIONS_VIOLATED    ((SLuint32) 0x00000001)
#define SL_RESULT_PARAMETER_INVALID     ((SLuint32) 0x00000002)
#define SL_RESULT_MEMORY_FAILURE            ((SLuint32) 0x00000003)
#define SL_RESULT_RESOURCE_ERROR            ((SLuint32) 0x00000004)
#define SL_RESULT_RESOURCE_LOST         ((SLuint32) 0x00000005)
#define SL_RESULT_IO_ERROR              ((SLuint32) 0x00000006)
#define SL_RESULT_BUFFER_INSUFFICIENT       ((SLuint32) 0x00000007)
#define SL_RESULT_CONTENT_CORRUPTED     ((SLuint32) 0x00000008)
#define SL_RESULT_CONTENT_UNSUPPORTED       ((SLuint32) 0x00000009)
#define SL_RESULT_CONTENT_NOT_FOUND     ((SLuint32) 0x0000000A)
#define SL_RESULT_PERMISSION_DENIED     ((SLuint32) 0x0000000B)
#define SL_RESULT_FEATURE_UNSUPPORTED       ((SLuint32) 0x0000000C)
#define SL_RESULT_INTERNAL_ERROR            ((SLuint32) 0x0000000D)
#define SL_RESULT_UNKNOWN_ERROR         ((SLuint32) 0x0000000E)
#define SL_RESULT_OPERATION_ABORTED     ((SLuint32) 0x0000000F)
#define SL_RESULT_CONTROL_LOST          ((SLuint32) 0x00000010)

//	@frameworks/wilhelm/src/ut/slesutResult.c
static const char * const slesutResultStrings[SLESUT_RESULT_MAX] = {
	"SL_RESULT_SUCCESS",                                                                                                                                                                                  
	"SL_RESULT_PRECONDITIONS_VIOLATED",
	"SL_RESULT_PARAMETER_INVALID",
	"SL_RESULT_MEMORY_FAILURE",
	"SL_RESULT_RESOURCE_ERROR",
	"SL_RESULT_RESOURCE_LOST",
	"SL_RESULT_IO_ERROR",
	"SL_RESULT_BUFFER_INSUFFICIENT",
	"SL_RESULT_CONTENT_CORRUPTED",
	"SL_RESULT_CONTENT_UNSUPPORTED",
	"SL_RESULT_CONTENT_NOT_FOUND",
	"SL_RESULT_PERMISSION_DENIED",
	"SL_RESULT_FEATURE_UNSUPPORTED",
	"SL_RESULT_INTERNAL_ERROR",
	"SL_RESULT_UNKNOWN_ERROR",
	"SL_RESULT_OPERATION_ABORTED",
	"SL_RESULT_CONTROL_LOST"
}; 






//	SLObjectItf为一个接口定义结构体
//	@frameworks/wilhelm/include/SLES/OpenSLES.h:105:typedef const struct SLObjectItf_ * const * SLObjectItf;

//	@frameworks/wilhelm/include/SLES/OpenSLES.h
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




//	ClassTable 定义
//	@frameworks/wilhelm/src/sles_allinclusive.h
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



struct iid_vtable {                                                                                                                                                                                           
    unsigned char mMPH;         // primary MPH for this interface, does not include any aliases
    unsigned char mInterface;   // relationship of interface to this class
    /*size_t*/ unsigned short mOffset;
};

typedef struct CEngine_struct CEngine



//	@frameworks/wilhelm/src/classes.h
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























