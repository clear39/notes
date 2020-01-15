

/**
 * @    frameworks/av/include/private/media/AudioTrackShared.h
 * @    frameworks/av/media/libaudioclient/AudioTrackShared.cpp
*/


/**
 * @    frameworks/av/include/private/media/AudioTrackShared.h:Q
 * Track构造函数中创建  @   frameworks/av/services/audioflinger/Tracks.cpp 
*/
AudioTrackServerProxy::AudioTrackServerProxy(audio_track_cblk_t* cblk, void *buffers, size_t frameCount,
            size_t frameSize, bool clientInServer = false, uint32_t sampleRate = 0)
        : ServerProxy(cblk, buffers, frameCount, frameSize, true /*isOut*/, clientInServer),
        mPlaybackRateObserver(&cblk->mPlaybackRateQueue),
        mUnderrunCount(0), mUnderrunning(false), mDrained(true) {
    mCblk->mSampleRate = sampleRate;
    mPlaybackRate = AUDIO_PLAYBACK_RATE_DEFAULT;
}

/**
 * status_t AudioFlinger::TrackHandle::start() 
 * -->status_t AudioFlinger::PlaybackThread::Track::start()
 * 
*/
void AudioTrackServerProxy::start()
{
    mStopLast = android_atomic_acquire_load(&mCblk->u.mStreaming.mStop);
}



__attribute__((no_sanitize("integer")))
status_t ServerProxy::obtainBuffer(Buffer* buffer, bool ackFlush)
{
    LOG_ALWAYS_FATAL_IF(buffer == NULL || buffer->mFrameCount == 0,
            "%s: null or zero frame buffer, buffer:%p", __func__, buffer);
    if (mIsShutdown) {
        goto no_init;
    }
    {
    audio_track_cblk_t* cblk = mCblk;
    // compute number of frames available to write (AudioTrack) or read (AudioRecord),
    // or use previous cached value from framesReady(), with added barrier if it omits.
    int32_t front;
    int32_t rear;
    // See notes on barriers at ClientProxy::obtainBuffer()
    if (mIsOut) {
        flushBufferIfNeeded(); // might modify mFront
        rear = getRear();
        front = cblk->u.mStreaming.mFront;
    } else {
        front = android_atomic_acquire_load(&cblk->u.mStreaming.mFront);
        rear = cblk->u.mStreaming.mRear;
    }
    ssize_t filled = rear - front;
    // pipe should not already be overfull
    if (!(0 <= filled && (size_t) filled <= mFrameCount)) {
        ALOGE("Shared memory control block is corrupt (filled=%zd, mFrameCount=%zu); shutting down",
                filled, mFrameCount);
        mIsShutdown = true;
    }
    if (mIsShutdown) {
        goto no_init;
    }
    // don't allow filling pipe beyond the nominal size
    size_t availToServer;
    if (mIsOut) {
        availToServer = filled;
        mAvailToClient = mFrameCount - filled;
    } else {
        availToServer = mFrameCount - filled;
        mAvailToClient = filled;
    }
    // 'availToServer' may be non-contiguous, so return only the first contiguous chunk
    size_t part1;
    if (mIsOut) {
        front &= mFrameCountP2 - 1;
        part1 = mFrameCountP2 - front;
    } else {
        rear &= mFrameCountP2 - 1;
        part1 = mFrameCountP2 - rear;
    }
    if (part1 > availToServer) {
        part1 = availToServer;
    }
    size_t ask = buffer->mFrameCount;
    if (part1 > ask) {
        part1 = ask;
    }
    // is assignment redundant in some cases?
    buffer->mFrameCount = part1;
    buffer->mRaw = part1 > 0 ? &((char *) mBuffers)[(mIsOut ? front : rear) * mFrameSize] : NULL;
    buffer->mNonContig = availToServer - part1;
    // After flush(), allow releaseBuffer() on a previously obtained buffer;
    // see "Acknowledge any pending flush()" in audioflinger/Tracks.cpp.
    if (!ackFlush) {
        mUnreleased = part1;
    }
    return part1 > 0 ? NO_ERROR : WOULD_BLOCK;
    }
no_init:
    buffer->mFrameCount = 0;
    buffer->mRaw = NULL;
    buffer->mNonContig = 0;
    mUnreleased = 0;
    return NO_INIT;
}