

/***
 *   @   /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/wilhelm/src/itf/IObject.cpp
 * 
 * 
 * 
 * result = (*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mEngine);
 * 
 * const SLInterfaceID SL_IID_ENGINE = &SL_IID_array[MPH_ENGINE]; 
 * 
 * */
static SLresult IObject_GetInterface(SLObjectItf self, const SLInterfaceID iid, void *pInterface)
{
    SL_ENTER_INTERFACE

    if (NULL == pInterface) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        void *interface = NULL;
        if (NULL == iid) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            /**
             * 
            */
            IObject *thiz = (IObject *) self;
            /**
             * ClassTable   @  ./src/sles_allinclusive.h
             * CEngine_class    @   frameworks/wilhelm/src/classes.cpp
            */
            const ClassTable *clazz = thiz->mClass;
            int MPH, index;
            /**
             * mMPH_to_index 为 CEngine_class 第三个成员
             * 
             * mMPH_to_index 为 MPH_to_Engine @ ./src/MPH_to.c
             * 
             * iid 为 const SLInterfaceID SL_IID_ENGINE = &SL_IID_array[MPH_ENGINE];   //   #define MPH_ENGINE                     16
             * 
             * MPH 得到索引差值  16
             * 
             * index = 2
            */
            if ((0 > (MPH = IID_to_MPH(iid))) ||
                    // no need to check for an initialization hook
                    // (NULL == MPH_init_table[MPH].mInit) ||
                    (0 > (index = clazz->mMPH_to_index[MPH]))) {
                result = SL_RESULT_FEATURE_UNSUPPORTED;
            } else {
                unsigned mask = 1 << index;
                object_lock_exclusive(thiz);
                if ((SL_OBJECT_STATE_REALIZED != thiz->mState) && !(INTERFACE_PREREALIZE & clazz->mInterfaces[index].mInterface)) {
                    // Can't get interface on an unrealized object unless pre-realize is ok
                    result = SL_RESULT_PRECONDITIONS_VIOLATED;
                } else if ((MPH_MUTESOLO == MPH) && (SL_OBJECTID_AUDIOPLAYER ==  clazz->mSLObjectID) && (1 == ((CAudioPlayer *) thiz)->mNumChannels)) {
                    // Can't get the MuteSolo interface of an audio player if the channel count is
                    // mono, but _can_ get the MuteSolo interface if the channel count is unknown
                    result = SL_RESULT_FEATURE_UNSUPPORTED;
                } else {

                    /**
                     * index = 2
                     * 
                     * Engine_interfaces @    frameworks/wilhelm/src/classes.cpp
                     * 
                     * {MPH_ENGINE, INTERFACE_IMPLICIT, offsetof(CEngine, mEngine)},
                    */
                    switch (thiz->mInterfaceStates[index]) {
                    case INTERFACE_EXPOSED:
                    case INTERFACE_ADDED:
                    /**
                     * IEngine mEngine;// Engine_interface
                    */
                        interface = (char *) thiz + clazz->mInterfaces[index].mOffset;
                        // Note that interface has been gotten,
                        // for debugger and to detect incorrect use of interfaces
                        if (!(thiz->mGottenMask & mask)) {
                            thiz->mGottenMask |= mask;
                            // This trickery validates the v-table
                            ((size_t *) interface)[0] ^= ~0;
                        }
                        result = SL_RESULT_SUCCESS;
                        break;
                    // Can't get interface if uninitialized, initialized, suspended,
                    // suspending, resuming, adding, or removing
                    default:
                        result = SL_RESULT_FEATURE_UNSUPPORTED;
                        break;
                    }
                }
                object_unlock_exclusive(thiz);
            }
        }
        *(void **)pInterface = interface;
    }

    SL_LEAVE_INTERFACE
}


typedef struct Engine_interface {
    const struct SLEngineItf_ *mItf;
    IObject *mThis;
    SLboolean mLossOfControlGlobal;
#ifdef USE_SDL
    COutputMix *mOutputMix; // SDL pulls PCM from an arbitrary IOutputMixExt
#endif
    // Each engine is its own universe.
    SLuint32 mInstanceCount;
    unsigned mInstanceMask; // 1 bit per active object
    unsigned mChangedMask;  // objects which have changed since last sync
#define MAX_INSTANCE 32     // maximum active objects per engine, see mInstanceMask
    IObject *mInstances[MAX_INSTANCE];
    SLboolean mShutdown;
    SLboolean mShutdownAck;
    // SLuint32 mVersion;      // 0xXXYYZZ where XX=major, YY=minor, ZZ=step
    SLuint32 mNativeEndianness; // one of SL_BYTEORDER_LITTLEENDIAN or SL_BYTEORDER_BIGENDIAN
} IEngine;



/**
 * @    frameworks/wilhelm/src/autogen/IID_to_MPH.cpp
 * 
 *iid 为  ./src/sl_iid.cpp:41:const SLInterfaceID SL_IID_ENGINE = &SL_IID_array[MPH_ENGINE]; 
*/
int IID_to_MPH(const SLInterfaceID iid)
{
#define MAX_HASH_VALUE 180
  static const unsigned char asso_values[] =
    {
       35,  20,  79,  99, 181, 181,  74, 181, 181, 181,
      181, 181, 181, 181,  84, 181,  79,  59, 181, 181,
      181, 181,  54, 181,  69,  39, 181, 181,  29, 181,
      181,  85,   5, 181, 181, 181, 181, 181,  49, 181,
       24, 181, 181, 181,  14, 181, 117, 181, 181, 181,
      181, 181,   9, 181, 181, 181, 181, 181,  14, 100,
      181,   9,  24, 181,  50, 181, 181, 181, 181, 127,
      181, 181, 107,  85, 181, 181, 181, 181, 181, 181,
      181, 181, 181, 181, 181, 181, 127, 181,  19, 181,
      181, 181,   9, 181, 181, 181,  25, 122,  14, 107,
      181, 181, 181, 181, 181, 181, 181,   4, 181, 102,
      181, 181,   0, 181,   4,  97, 122, 181,  72,  45,
      181, 181, 105, 181, 181, 181,  87, 181,  30, 181,
      181, 181, 181, 181, 181, 107, 125, 181, 181, 181,
      181, 181,  80,  82, 181,  77, 181, 120, 181, 181,
      181, 181, 181,  97, 181, 181,  87, 181, 181,  65,
        5, 181,  57, 181, 181,  82, 115, 181, 181,  52,
      181,  72,  55, 181, 181,  62, 181,  45, 181, 181,
      181, 181,  32, 181,  32,  30, 181, 181, 181, 181,
      181, 181,   0, 181, 181, 181, 110,  47, 181, 181,
      181, 181, 181, 181, 181, 181, 120, 181, 181, 181,
      100,  47, 181, 181, 181,  37, 181, 181,   2, 181,
      181, 181, 181,  27,  10, 181, 181,  27, 181, 181,
      181, 181, 181, 181,  22, 181,  75, 181, 181, 181,
       10,  12, 181, 181,  70, 181,   7,   7, 181, 181,
      181, 105,   2, 125, 181, 181
    };

    static const signed char hash_to_MPH[] = {
        MPH_ANDROIDEFFECTSEND,
        -1,
        MPH_XADYNAMICINTERFACEMANAGEMENT,
        -1,
        MPH_XAAUDIODECODERCAPABILITIES,
        MPH_XALED,
        -1,
        MPH_BASSBOOST,
        -1,
        MPH_XAOUTPUTMIX,
        MPH_MIDITIME,
        -1,
        MPH_RECORD,
        -1,
        MPH_AUDIOIODEVICECAPABILITIES,
        MPH_MUTESOLO,
        -1,
        MPH_VOLUME,
        -1,
        MPH_ANDROIDCONFIGURATION,
        MPH_XASNAPSHOT,
        -1,
        MPH_XAIMAGECONTROLS,
        -1,
        MPH_SEEK,
        MPH_3DLOCATION,
        -1,
        MPH_XATHREADSYNC,
        -1,
        MPH_XAIMAGEDECODERCAPABILITIES,
        MPH_XACAMERACAPABILITIES,
        -1,
        MPH_XAVOLUME,
        -1,
        MPH_VIBRA,
        MPH_XAIMAGEEFFECTS,
        -1,
        MPH_XAPLAY,
        -1,
        MPH_PRESETREVERB,
        MPH_XAOBJECT,
        -1,
        MPH_XACONFIGEXTENSION,
        -1,
        MPH_EQUALIZER,
        MPH_XAVIBRA,
        -1,
        MPH_3DMACROSCOPIC,
        -1,
        MPH_PITCH,
        MPH_ENGINECAPABILITIES,
        -1,
        MPH_XAMETADATAEXTRACTION,
        -1,
        MPH_3DDOPPLER,
        MPH_XAVIDEODECODERCAPABILITIES,
        -1,
        MPH_RATEPITCH,
        -1,
        MPH_ANDROIDAUTOMATICGAINCONTROL,
        MPH_AUDIODECODERCAPABILITIES,
        -1,
        MPH_XARADIO,
        -1,
        MPH_OUTPUTMIXEXT,
        MPH_ENVIRONMENTALREVERB,
        -1,
        MPH_XAVIDEOPOSTPROCESSING,
        -1,
        MPH_3DCOMMIT,
        MPH_OUTPUTMIX,
        -1,
        MPH_METADATATRAVERSAL,
        -1,
        MPH_XASEEK,
        MPH_XASTREAMINFORMATION,
        -1,
        MPH_DEVICEVOLUME,
        -1,
        MPH_OBJECT,
        MPH_VIRTUALIZER,
        -1,
        MPH_XARDS,
        -1,
        MPH_XAVIDEOENCODER,
        MPH_PLAY,
        -1,
        MPH_XADYNAMICSOURCE,
        -1,
        MPH_3DGROUPING,
        MPH_XAVIDEOENCODERCAPABILITIES,
        -1,
        MPH_XAPREFETCHSTATUS,
        -1,
        MPH_XAMETADATATRAVERSAL,
        MPH_XAEQUALIZER,
        -1,
        MPH_BUFFERQUEUE,
        -1,
        MPH_ANDROIDBUFFERQUEUESOURCE,
        MPH_XARECORD,
        -1,
        MPH_XAMETADATAINSERTION,
        -1,
        MPH_XADEVICEVOLUME,
        MPH_ANDROIDNOISESUPPRESSION,
        -1,
        MPH_ENGINE,
        -1,
        MPH_MIDIMUTESOLO,
        MPH_METADATAEXTRACTION,
        -1,
        MPH_XACAMERA,
        -1,
        -1,
        MPH_PREFETCHSTATUS,
        -1,
        MPH_LED,
        -1,
        -1,
        MPH_XAAUDIOENCODER,
        -1,
        MPH_XAENGINE,
        -1,
        -1,
        MPH_ANDROIDEFFECTCAPABILITIES,
        -1,
        MPH_MIDIMESSAGE,
        -1,
        -1,
        MPH_VISUALIZATION,
        -1,
        MPH_3DSOURCE,
        -1,
        -1,
        MPH_PLAYBACKRATE,
        -1,
        MPH_XAAUDIOENCODERCAPABILITIES,
        -1,
        -1,
        MPH_EFFECTSEND,
        -1,
        MPH_XAAUDIOIODEVICECAPABILITIES,
        -1,
        -1,
        MPH_NULL,
        -1,
        MPH_ANDROIDACOUSTICECHOCANCELLATION,
        -1,
        -1,
        MPH_XAIMAGEENCODERCAPABILITIES,
        -1,
        MPH_ANDROIDEFFECT,
        -1,
        -1,
        MPH_MIDITEMPO,
        -1,
        MPH_XAPLAYBACKRATE,
        -1,
        -1,
        MPH_DYNAMICINTERFACEMANAGEMENT,
        -1,
        MPH_DYNAMICSOURCE,
        -1,
        -1,
        MPH_ANDROIDSIMPLEBUFFERQUEUE,
        -1,
        MPH_THREADSYNC,
        -1,
        -1,
        MPH_AUDIOENCODERCAPABILITIES,
        -1,
        -1,
        -1,
        -1,
        MPH_XAIMAGEENCODER,
        -1,
        -1,
        -1,
        -1,
        MPH_AUDIOENCODER
    };
    /**
     *  @   ./src/OpenSLES_IID.cpp
     * 
     *  iid 为 const SLInterfaceID SL_IID_ENGINE = &SL_IID_array[MPH_ENGINE]; 
     * 
     * 
     *  // SL_IID_ENGINE
     *  { 0x8d97c260, 0xddd4, 0x11db, 0x958f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
     * 
     * 
    */
    if (&SL_IID_array[0] <= iid && &SL_IID_array[MPH_MAX] > iid)
        return iid - &SL_IID_array[0];

    if (NULL != iid) {
        unsigned key = asso_values[((unsigned char *)iid)[8]] + asso_values[((unsigned char *)iid)[0]];
        if (key <= MAX_HASH_VALUE) {
            int MPH = hash_to_MPH[key];
            if (MPH >= 0) {
                assert(MPH < MPH_MAX);
                SLInterfaceID trial = &SL_IID_array[MPH];
                if (!memcmp(iid, trial, sizeof(struct SLInterfaceID_)))
                    return MPH;
            }
        }
    }
    return -1;
}




/**
 * ./include/SLES/OpenSLES.h
*/

/** Interface ID defined as a UUID */
typedef const struct SLInterfaceID_ {
    SLuint32 time_low;
    SLuint16 time_mid;
    SLuint16 time_hi_and_version;
    SLuint16 clock_seq;
    SLuint8  node[6];
} * SLInterfaceID;


const struct SLInterfaceID_ SL_IID_array[MPH_MAX] = {

// OpenSL ES 1.0.1 interfaces

    // SL_IID_3DCOMMIT
    { 0x3564ad80, 0xdd0f, 0x11db, 0x9e19, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_3DDOPPLER
    { 0xb45c9a80, 0xddd2, 0x11db, 0xb028, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_3DGROUPING
    { 0xebe844e0, 0xddd2, 0x11db, 0xb510, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_3DLOCATION
    { 0x2b878020, 0xddd3, 0x11db, 0x8a01, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_3DMACROSCOPIC
    { 0x5089aec0, 0xddd3, 0x11db, 0x9ad3, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_3DSOURCE
    { 0x70bc7b00, 0xddd3, 0x11db, 0xa873, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_AUDIODECODERCAPABILITIES
    { 0x3fe5a3a0, 0xfcc6, 0x11db, 0x94ac, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_AUDIOENCODER
    { 0xd7d5af7a, 0x351c, 0x41a6, 0x94ec, { 0x1a, 0xc9, 0x5c, 0x71, 0x82, 0x2c } },
    // SL_IID_AUDIOENCODERCAPABILITIES
    { 0x0f52a340, 0xfcd1, 0x11db, 0xa993, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_AUDIOIODEVICECAPABILITIES
    { 0xb2564dc0, 0xddd3, 0x11db, 0xbd62, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_BASSBOOST
    { 0x0634f220, 0xddd4, 0x11db, 0xa0fc, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_BUFFERQUEUE
    { 0x2bc99cc0, 0xddd4, 0x11db, 0x8d99, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_DEVICEVOLUME
    { 0xe1634760, 0xf3e2, 0x11db, 0x9ca9, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_DYNAMICINTERFACEMANAGEMENT
    { 0x63936540, 0xf775, 0x11db, 0x9cc4, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_DYNAMICSOURCE
    { 0xc55cc100, 0x038b, 0x11dc, 0xbb45, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_EFFECTSEND
    { 0x56e7d200, 0xddd4, 0x11db, 0xaefb, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ENGINE
    { 0x8d97c260, 0xddd4, 0x11db, 0x958f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ENGINECAPABILITIES
    { 0x8320d0a0, 0xddd5, 0x11db, 0xa1b1, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ENVIRONMENTALREVERB
    { 0xc2e5d5f0, 0x94bd, 0x4763, 0x9cac, { 0x4e, 0x23, 0x4d, 0x6, 0x83, 0x9e } },
    // SL_IID_EQUALIZER
    { 0x0bed4300, 0xddd6, 0x11db, 0x8f34, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_LED
    { 0x2cc1cd80, 0xddd6, 0x11db, 0x807e, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_METADATAEXTRACTION
    { 0xaa5b1f80, 0xddd6, 0x11db, 0xac8e, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_METADATATRAVERSAL
    { 0xc43662c0, 0xddd6, 0x11db, 0xa7ab, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_MIDIMESSAGE
    { 0xddf4a820, 0xddd6, 0x11db, 0xb174, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_MIDIMUTESOLO
    { 0x039eaf80, 0xddd7, 0x11db, 0x9a02, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_MIDITEMPO
    { 0x1f347400, 0xddd7, 0x11db, 0xa7ce, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_MIDITIME
    { 0x3da51de0, 0xddd7, 0x11db, 0xaf70, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_MUTESOLO
    { 0x5a28ebe0, 0xddd7, 0x11db, 0x8220, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_NULL
    { 0xec7178ec, 0xe5e1, 0x4432, 0xa3f4, { 0x46, 0x57, 0xe6, 0x79, 0x52, 0x10 } },
    // SL_IID_OBJECT
    { 0x79216360, 0xddd7, 0x11db, 0xac16, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_OUTPUTMIX
    { 0x97750f60, 0xddd7, 0x11db, 0x92b1, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_PITCH
    { 0xc7e8ee00, 0xddd7, 0x11db, 0xa42c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_PLAY
    { 0xef0bd9c0, 0xddd7, 0x11db, 0xbf49, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_PLAYBACKRATE
    { 0x2e3b2a40, 0xddda, 0x11db, 0xa349, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_PREFETCHSTATUS
    { 0x2a41ee80, 0xddd8, 0x11db, 0xa41f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_PRESETREVERB
    { 0x47382d60, 0xddd8, 0x11db, 0xbf3a, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_RATEPITCH
    { 0x61b62e60, 0xddda, 0x11db, 0x9eb8, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_RECORD
    { 0xc5657aa0, 0xdddb, 0x11db, 0x82f7, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_SEEK
    { 0xd43135a0, 0xdddc, 0x11db, 0xb458, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_THREADSYNC
    { 0xf6ac6b40, 0xdddc, 0x11db, 0xa62e, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_VIBRA
    { 0x169a8d60, 0xdddd, 0x11db, 0x923d, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_VIRTUALIZER
    { 0x37cc2c00, 0xdddd, 0x11db, 0x8577, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_VISUALIZATION
    { 0xe46b26a0, 0xdddd, 0x11db, 0x8afd, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_VOLUME
    { 0x09e8ede0, 0xddde, 0x11db, 0xb4f6, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },

// Wilhelm desktop extended interfaces

// SL_IID_OUTPUTMIXEXT
    { 0xfe5cce00, 0x57bb, 0x11df, 0x951c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },

// Android API level 9 extended interfaces
// GUID and MPH shared by SL and XA, but currently documented for SL only

    // SL_IID_ANDROIDEFFECT
    { 0xae12da60, 0x99ac, 0x11df, 0xb456, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ANDROIDEFFECTCAPABILITIES
    { 0x6a4f6d60, 0xb5e6, 0x11df, 0xbb3b, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ANDROIDEFFECTSEND
    { 0x7be462c0, 0xbc43, 0x11df, 0x8670, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ANDROIDCONFIGURATION
    { 0x89f6a7e0, 0xbeac, 0x11df, 0x8b5c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ANDROIDSIMPLEBUFERQUEUE
    { 0x198e4940, 0xc5d7, 0x11df, 0xa2a6, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },

// Android API level 12 extended interfaces
// GUID and MPH shared by SL and XA, but currently documented for XA only

    // SL_IID_ANDROIDBUFFERQUEUESOURCE
    { 0x7fc1a460, 0xeec1, 0x11df, 0xa306, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },

// OpenMAX AL 1.0.1 interfaces

    // XA_IID_AUDIODECODERCAPABILITIES
    { 0xdeac0cc0, 0x3995, 0x11dc, 0x8872, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_AUDIOENCODER
    { 0xebbab900, 0x3997, 0x11dc, 0x891f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_AUDIOENCODERCAPABILITIES
    { 0x83fbc600, 0x3998, 0x11dc, 0x8f6d, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_AUDIOIODEVICECAPABILITIES
    { 0x2b276d00, 0xf775, 0x11db, 0xa963, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_CAMERA
    { 0xc7b84d20, 0xdf00, 0x11db, 0xba87, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_CAMERACAPABILITIES
    { 0x01cab1c0, 0xe86a, 0x11db, 0xa5b9, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_CONFIGEXTENSION
    { 0x6dc22ea0, 0xdf03, 0x11db, 0xbed7, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_DEVICEVOLUME
    { 0x4bb44020, 0xf775, 0x11db, 0xad03, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_DYNAMICINTERFACEMANAGEMENT
    { 0x6e2340c0, 0xf775, 0x11db, 0x85da, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_DYNAMICSOURCE
    { 0xc88d5480, 0x3a12, 0x11dc, 0x80a2, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_ENGINE
    { 0x45c58f40, 0xdf04, 0x11db, 0x9e76, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_EQUALIZER
    { 0x7ad86d40, 0xf775, 0x11db, 0xbc77, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_IMAGECONTROLS
    { 0xf46de3e0, 0xdf03, 0x11db, 0x92f1, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_IMAGEDECODERCAPABILITIES
    { 0xc333e7a0, 0xe616, 0x11dc, 0xa93e, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_IMAGEEFFECTS
    { 0xb865bca0, 0xdf04, 0x11db, 0xbab9, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_IMAGEENCODER
    { 0xcd49f140, 0xdf04, 0x11db, 0x8888, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_IMAGEENCODERCAPABILITIES
    { 0xc19f0640, 0xe86f, 0x11db, 0xb2d2, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_LED
    { 0xa534d920, 0xf775, 0x11db, 0x8b70, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_METADATAEXTRACTION
    { 0x5df4fda0, 0xf776, 0x11db, 0xabc5, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_METADATAINSERTION
    { 0x49a14d60, 0xdf05, 0x11db, 0x9191, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_METADATATRAVERSAL
    { 0x73ffb0e0, 0xf776, 0x11db, 0xa00e, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_OBJECT
    { 0x82f5a5a0, 0xf776, 0x11db, 0x9700, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_OUTPUTMIX
    { 0xb25b6fa0, 0xf776, 0x11db, 0xb86b, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_NULL
    // shared with OpenSL ES
    // XA_IID_PLAY
    { 0xb9c293e0, 0xf776, 0x11db, 0x80df, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_PLAYBACKRATE
    { 0xc36f1440, 0xf776, 0x11db, 0xac48, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_PREFETCHSTATUS
    { 0xcceac0a0, 0xf776, 0x11db, 0xbb9c, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_RADIO
    { 0xb316ad80, 0xdf05, 0x11db, 0xb5b6, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_RDS
    { 0xb80f42c0, 0xdf05, 0x11db, 0x92a5, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_RECORD
    { 0xd7948cc0, 0xf776, 0x11db, 0x8a3b, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_SEEK
    { 0xee6a3120, 0xf776, 0x11db, 0xb518, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_SNAPSHOT
    { 0xdb1b6dc0, 0xdf05, 0x11db, 0x8c01, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_STREAMINFORMATION
    { 0x3a628fe0, 0x1238, 0x11de, 0xad9f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_THREADSYNC
    { 0xf3599ea0, 0xf776, 0x11db, 0xb3ea, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_VIBRA
    { 0xfe374c00, 0xf776, 0x11db, 0xa8f0, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_VIDEODECODERCAPABILITIES
    { 0xd18cb200, 0xe616, 0x11dc, 0xab01, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_VIDEOENCODER
    { 0x9444db60, 0xdf06, 0x11db, 0xb311, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_VIDEOENCODERCAPABILITIES
    { 0x5aef2760, 0xe872, 0x11db, 0x849f, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_VIDEOPOSTPROCESSING
    { 0x898b6820, 0x7e6e, 0x11dd, 0x8caf, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // XA_IID_VOLUME
    { 0x088ba520, 0xf777, 0x11db, 0xa5e3, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },

// Android API level 20 extended interfaces

    // SL_IID_ANDROIDACOUSTICECHOCANCELLATION
    { 0x7b491460, 0x8d4d, 0x11e0, 0xbd61, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ANDROIDAUTOMATICGAINCONTROL
    { 0x0a8abfe0, 0x654c, 0x11e0, 0xba26, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
    // SL_IID_ANDROIDNOISESUPPRESSION
    { 0x58b4b260, 0x8e06, 0x11e0, 0xaa8e, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } },
};











const signed char MPH_to_Engine[MPH_MAX] = {
#ifdef USE_DESIGNATED_INITIALIZERS
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_ENGINE] = 2,
    [MPH_ENGINECAPABILITIES] = 3,
    [MPH_THREADSYNC] = 4,
    [MPH_AUDIOIODEVICECAPABILITIES] = 5,
    [MPH_AUDIODECODERCAPABILITIES] = 6,
    [MPH_AUDIOENCODERCAPABILITIES] = 7,
    [MPH_3DCOMMIT] = 8,
    [MPH_DEVICEVOLUME] = 9,
    [MPH_XAENGINE] = 10,
#ifdef ANDROID
    [MPH_ANDROIDEFFECTCAPABILITIES] = 11,
#endif
    [MPH_XAVIDEODECODERCAPABILITIES] = 12
#else
#include "MPH_to_Engine.h"
#endif
};

//  @   frameworks/wilhelm/src/autogen/MPH_to_Engine.h
// This file is automagically generated by mphtogen, do not edit
  8, -1, -1, -1, -1, -1,  6, -1,  7,  5, -1, -1,  9,  1, -1, -1,  2,  3, -1, -1,
 -1, -1, -1, -1, -1, -1, -1, -1, -1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1,  4,
 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
 -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
 -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, -1, -1, -1
