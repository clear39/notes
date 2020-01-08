

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