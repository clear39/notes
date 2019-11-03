//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audioflinger/Tracks.cpp



void AudioFlinger::PlaybackThread::Track::invalidate()
{
    TrackBase::invalidate();
    signalClientFlag(CBLK_INVALID);
}

void AudioFlinger::PlaybackThread::Track::signalClientFlag(int32_t flag)
{
    // FIXME should use proxy, and needs work
    audio_track_cblk_t* cblk = mCblk;
    android_atomic_or(flag, &cblk->mFlags);
    android_atomic_release_store(0x40000000, &cblk->mFutex);
    // client is not in server, so FUTEX_WAKE is needed instead of FUTEX_WAKE_PRIVATE
    (void) syscall(__NR_futex, &cblk->mFutex, FUTEX_WAKE, INT_MAX);
}