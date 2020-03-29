
// for audio_track_cblk_t::mFlags
#define CBLK_UNDERRUN   0x01 // set by server immediately on output underrun, cleared by client
#define CBLK_FORCEREADY 0x02 // set: track is considered ready immediately by AudioFlinger, clear: track is ready when buffer full
#define CBLK_INVALID    0x04 // track buffer invalidated by AudioFlinger, need to re-create   //通过 restoreTrack_l 恢复 和 AudioFlinger上的连接
#define CBLK_DISABLED   0x08 // output track disabled by AudioFlinger due to underrun,  need to re-start.  Unlike CBLK_UNDERRUN, this is not set  immediately, but only after a long string of underruns.
// 0x10 unused
#define CBLK_LOOP_CYCLE 0x20 // set by server each time a loop cycle other than final one completes
#define CBLK_LOOP_FINAL 0x40 // set by server when the final loop cycle completes
#define CBLK_BUFFER_END 0x80 // set by server when the position reaches end of buffer if not looping
#define CBLK_OVERRUN   0x100 // set by server immediately on input overrun, cleared by client
#define CBLK_INTERRUPT 0x200 // set by client on interrupt(), cleared by client in obtainBuffer()
#define CBLK_STREAM_END_DONE 0x400 // set by server on render completion, cleared by client
 
//EL_FIXME 20 seconds may not be enough and must be reconciled with new obtainBuffer implementation
#define MAX_RUN_OFFLOADED_TIMEOUT_MS 20000 // assuming up to a maximum of 20 seconds of offloaded


/**
 * @    /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/include/private/media/AudioTrackShared.h
*/
// Important: do not add any virtual methods, including ~
struct audio_track_cblk_t
{
                // Since the control block is always located in shared memory, this constructor
                // is only used for placement new().  It is never used for regular new() or stack.
                            audio_track_cblk_t();
                /*virtual*/ ~audio_track_cblk_t() { }

                friend class Proxy;
                friend class ClientProxy;
                friend class AudioTrackClientProxy;
                friend class AudioRecordClientProxy;
                friend class ServerProxy;
                friend class AudioTrackServerProxy;
                friend class AudioRecordServerProxy;

    // The data members are grouped so that members accessed frequently and in the same context
    // are in the same line of data cache.

                uint32_t    mServer;    // Number of filled frames consumed by server (mIsOut),
                                        // or filled frames provided by server (!mIsOut).
                                        // It is updated asynchronously by server without a barrier.
                                        // The value should be used
                                        // "for entertainment purposes only",
                                        // which means don't make important decisions based on it.

                uint32_t    mPad1;      // unused

    volatile    int32_t     mFutex;     // event flag: down (P) by client,
                                        // up (V) by server or binderDied() or interrupt()
#define CBLK_FUTEX_WAKE 1               // if event flag bit is set, then a deferred wake is pending

private:

                // This field should be a size_t, but since it is located in shared memory we
                // force to 32-bit.  The client and server may have different typedefs for size_t.
                uint32_t    mMinimum;       // server wakes up client if available >= mMinimum

                // Stereo gains for AudioTrack only, not used by AudioRecord.
                gain_minifloat_packed_t mVolumeLR;

                uint32_t    mSampleRate;    // AudioTrack only: client's requested sample rate in Hz
                                            // or 0 == default. Write-only client, read-only server.

                PlaybackRateQueue::Shared mPlaybackRateQueue;

                // client write-only, server read-only
                uint16_t    mSendLevel;      // Fixed point U4.12 so 0x1000 means 1.0

                uint16_t    mPad2 __attribute__((__unused__)); // unused

                // server write-only, client read
                ExtendedTimestampQueue::Shared mExtendedTimestampQueue;

                // This is set by AudioTrack.setBufferSizeInFrames().
                // A write will not fill the buffer above this limit.
                volatile    uint32_t   mBufferSizeInFrames;  // effective size of the buffer

public:

                 volatile    int32_t     mFlags;         // combinations of CBLK_*

public:
                union {
                    AudioTrackSharedStreaming   mStreaming;
                    AudioTrackSharedStatic      mStatic;
                    int                         mAlign[8];
                } u;

                // Cache line boundary (32 bytes)
};

struct AudioTrackSharedStreaming {
    // similar to NBAIO MonoPipe
    // in continuously incrementing frame units, take modulo buffer size, which must be a power of 2
    volatile int32_t mFront;    // read by consumer (output: server, input: client)
    volatile int32_t mRear;     // written by producer (output: client, input: server)
    volatile int32_t mFlush;    // incremented by client to indicate a request to flush;
                                // server notices and discards all data between mFront and mRear
    volatile int32_t mStop;     // set by client to indicate a stop frame position; server
                                // will not read beyond this position until start is called.
    volatile uint32_t mUnderrunFrames; // server increments for each unavailable but desired frame
    volatile uint32_t mUnderrunCount;  // server increments for each underrun occurrence
};

struct AudioTrackSharedStatic {
    // client requests to the server for loop or position changes.
    StaticAudioTrackSingleStateQueue::Shared mSingleStateQueue;
    // position info updated asynchronously by server and read by client,
    // "for entertainment purposes only"
    StaticAudioTrackPosLoopQueue::Shared mPosLoopQueue;
};