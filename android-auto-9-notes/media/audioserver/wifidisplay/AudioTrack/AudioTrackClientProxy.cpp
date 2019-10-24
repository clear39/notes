
//  @   frameworks/av/include/private/media/AudioTrackShared.h



class AudioTrackClientProxy : public ClientProxy {}




Proxy::Proxy(audio_track_cblk_t* cblk, void *buffers, size_t frameCount, size_t frameSize,bool isOut, bool clientInServer)
    : mCblk(cblk), mBuffers(buffers), mFrameCount(frameCount), mFrameSize(frameSize),
      mFrameCountP2(roundup(frameCount)), mIsOut(isOut), mClientInServer(clientInServer),
      mIsShutdown(false), mUnreleased(0)
{
}

ClientProxy::ClientProxy(audio_track_cblk_t* cblk, void *buffers, size_t frameCount,size_t frameSize, bool isOut, bool clientInServer)
    : Proxy(cblk, buffers, frameCount, frameSize, isOut, clientInServer)
    , mEpoch(0)
    , mTimestampObserver(&cblk->mExtendedTimestampQueue)
{
    setBufferSizeInFrames(frameCount);
}

//  @   frameworks/av/media/libaudioclient/AudioTrackShared.cpp
AudioTrackClientProxy::AudioTrackClientProxy(audio_track_cblk_t* cblk, void *buffers, size_t frameCount,size_t frameSize, bool clientInServer = false)
        : ClientProxy(cblk, buffers, frameCount, frameSize, true /*isOut*/,clientInServer),
          mPlaybackRateMutator(&cblk->mPlaybackRateQueue) {
}


/***
 * 注意这里的目的是查看哪里 会返回 DEAD_OBJECT
 * */
__attribute__((no_sanitize("integer")))
status_t ClientProxy::obtainBuffer(Buffer* buffer, const struct timespec *requested,struct timespec *elapsed)
{
    LOG_ALWAYS_FATAL_IF(buffer == NULL || buffer->mFrameCount == 0, "%s: null or zero frame buffer, buffer:%p", __func__, buffer);
    struct timespec total;          // total elapsed time spent waiting
    total.tv_sec = 0;
    total.tv_nsec = 0;
    bool measure = elapsed != NULL; // whether to measure total elapsed time spent waiting

    status_t status;
    enum {
        TIMEOUT_ZERO,       // requested == NULL || *requested == 0
        TIMEOUT_INFINITE,   // *requested == infinity
        TIMEOUT_FINITE,     // 0 < *requested < infinity
        TIMEOUT_CONTINUE,   // additional chances after TIMEOUT_FINITE
    } timeout;
    if (requested == NULL) {
        timeout = TIMEOUT_ZERO;
    } else if (requested->tv_sec == 0 && requested->tv_nsec == 0) {
        timeout = TIMEOUT_ZERO;
    } else if (requested->tv_sec == INT_MAX) {
        timeout = TIMEOUT_INFINITE;
    } else {
        timeout = TIMEOUT_FINITE;
        if (requested->tv_sec > 0 || requested->tv_nsec >= MEASURE_NS) {
            measure = true;
        }
    }
    struct timespec before;
    bool beforeIsValid = false;
    audio_track_cblk_t* cblk = mCblk;
    bool ignoreInitialPendingInterrupt = true;
    // check for shared memory corruption
    if (mIsShutdown) {
        status = NO_INIT;
        goto end;
    }
    for (;;) {
        /***
         * android_atomic_and 原子与操作
         * frameworks/av/include/private/media/AudioTrackShared.h:50:#define CBLK_INTERRUPT 0x200 
        */
        int32_t flags = android_atomic_and(~CBLK_INTERRUPT, &cblk->mFlags);
        // check for track invalidation by server, or server death detection
        /**
         * 
         * frameworks/av/include/private/media/AudioTrackShared.h:41:
         * #define CBLK_INVALID    0x04 // track buffer invalidated by AudioFlinger, need to re-create
        */
        if (flags & CBLK_INVALID) {
            ALOGV("Track invalidated");
            status = DEAD_OBJECT;
            goto end;
        }
        if (flags & CBLK_DISABLED) {
            ALOGV("Track disabled");
            status = NOT_ENOUGH_DATA;
            goto end;
        }
        // check for obtainBuffer interrupted by client
        if (!ignoreInitialPendingInterrupt && (flags & CBLK_INTERRUPT)) {
            ALOGV("obtainBuffer() interrupted by client");
            status = -EINTR;
            goto end;
        }
        ignoreInitialPendingInterrupt = false;
        // compute number of frames available to write (AudioTrack) or read (AudioRecord)
        int32_t front;
        int32_t rear;
        if (mIsOut) {
            // The barrier following the read of mFront is probably redundant.
            // We're about to perform a conditional branch based on 'filled',
            // which will force the processor to observe the read of mFront
            // prior to allowing data writes starting at mRaw.
            // However, the processor may support speculative execution,
            // and be unable to undo speculative writes into shared memory.
            // The barrier will prevent such speculative execution.
            front = android_atomic_acquire_load(&cblk->u.mStreaming.mFront);
            rear = cblk->u.mStreaming.mRear;
        } else {
            // On the other hand, this barrier is required.
            rear = android_atomic_acquire_load(&cblk->u.mStreaming.mRear);
            front = cblk->u.mStreaming.mFront;
        }
        // write to rear, read from front
        ssize_t filled = rear - front;
        // pipe should not be overfull
        if (!(0 <= filled && (size_t) filled <= mFrameCount)) {
            if (mIsOut) {
                ALOGE("Shared memory control block is corrupt (filled=%zd, mFrameCount=%zu); "
                        "shutting down", filled, mFrameCount);
                mIsShutdown = true;
                status = NO_INIT;
                goto end;
            }
            // for input, sync up on overrun
            filled = 0;
            cblk->u.mStreaming.mFront = rear;
            (void) android_atomic_or(CBLK_OVERRUN, &cblk->mFlags);
        }
        // Don't allow filling pipe beyond the user settable size.
        // The calculation for avail can go negative if the buffer size
        // is suddenly dropped below the amount already in the buffer.
        // So use a signed calculation to prevent a numeric overflow abort.
        ssize_t adjustableSize = (ssize_t) getBufferSizeInFrames();
        ssize_t avail =  (mIsOut) ? adjustableSize - filled : filled;
        if (avail < 0) {
            avail = 0;
        } else if (avail > 0) {
            // 'avail' may be non-contiguous, so return only the first contiguous chunk
            size_t part1;
            if (mIsOut) {
                rear &= mFrameCountP2 - 1;
                part1 = mFrameCountP2 - rear;
            } else {
                front &= mFrameCountP2 - 1;
                part1 = mFrameCountP2 - front;
            }
            if (part1 > (size_t)avail) {
                part1 = avail;
            }
            if (part1 > buffer->mFrameCount) {
                part1 = buffer->mFrameCount;
            }
            buffer->mFrameCount = part1;
            buffer->mRaw = part1 > 0 ?
                    &((char *) mBuffers)[(mIsOut ? rear : front) * mFrameSize] : NULL;
            buffer->mNonContig = avail - part1;
            mUnreleased = part1;
            status = NO_ERROR;
            break;
        }
        struct timespec remaining;
        const struct timespec *ts;
        switch (timeout) {
        case TIMEOUT_ZERO:
            status = WOULD_BLOCK;
            goto end;
        case TIMEOUT_INFINITE:
            ts = NULL;
            break;
        case TIMEOUT_FINITE:
            timeout = TIMEOUT_CONTINUE;
            if (MAX_SEC == 0) {
                ts = requested;
                break;
            }
            // fall through
        case TIMEOUT_CONTINUE:
            // FIXME we do not retry if requested < 10ms? needs documentation on this state machine
            if (!measure || requested->tv_sec < total.tv_sec ||
                    (requested->tv_sec == total.tv_sec && requested->tv_nsec <= total.tv_nsec)) {
                status = TIMED_OUT;
                goto end;
            }
            remaining.tv_sec = requested->tv_sec - total.tv_sec;
            if ((remaining.tv_nsec = requested->tv_nsec - total.tv_nsec) < 0) {
                remaining.tv_nsec += 1000000000;
                remaining.tv_sec++;
            }
            if (0 < MAX_SEC && MAX_SEC < remaining.tv_sec) {
                remaining.tv_sec = MAX_SEC;
                remaining.tv_nsec = 0;
            }
            ts = &remaining;
            break;
        default:
            LOG_ALWAYS_FATAL("obtainBuffer() timeout=%d", timeout);
            ts = NULL;
            break;
        }
        int32_t old = android_atomic_and(~CBLK_FUTEX_WAKE, &cblk->mFutex);
        if (!(old & CBLK_FUTEX_WAKE)) {
            if (measure && !beforeIsValid) {
                clock_gettime(CLOCK_MONOTONIC, &before);
                beforeIsValid = true;
            }
            errno = 0;
            (void) syscall(__NR_futex, &cblk->mFutex,
                    mClientInServer ? FUTEX_WAIT_PRIVATE : FUTEX_WAIT, old & ~CBLK_FUTEX_WAKE, ts);
            status_t error = errno; // clock_gettime can affect errno
            // update total elapsed time spent waiting
            if (measure) {
                struct timespec after;
                clock_gettime(CLOCK_MONOTONIC, &after);
                total.tv_sec += after.tv_sec - before.tv_sec;
                long deltaNs = after.tv_nsec - before.tv_nsec;
                if (deltaNs < 0) {
                    deltaNs += 1000000000;
                    total.tv_sec--;
                }
                if ((total.tv_nsec += deltaNs) >= 1000000000) {
                    total.tv_nsec -= 1000000000;
                    total.tv_sec++;
                }
                before = after;
                beforeIsValid = true;
            }
            switch (error) {
            case 0:            // normal wakeup by server, or by binderDied()
            case EWOULDBLOCK:  // benign race condition with server
            case EINTR:        // wait was interrupted by signal or other spurious wakeup
            case ETIMEDOUT:    // time-out expired
                // FIXME these error/non-0 status are being dropped
                break;
            default:
                status = error;
                ALOGE("%s unexpected error %s", __func__, strerror(status));
                goto end;
            }
        }
    }

end:
    if (status != NO_ERROR) {
        buffer->mFrameCount = 0;
        buffer->mRaw = NULL;
        buffer->mNonContig = 0;
        mUnreleased = 0;
    }
    if (elapsed != NULL) {
        *elapsed = total;
    }
    if (requested == NULL) {
        requested = &kNonBlocking;
    }
    if (measure) {
        ALOGV("requested %ld.%03ld elapsed %ld.%03ld",
              requested->tv_sec, requested->tv_nsec / 1000000,
              total.tv_sec, total.tv_nsec / 1000000);
    }
    return status;
}