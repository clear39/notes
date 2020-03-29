// Track constructor must be called with AudioFlinger::mLock and ThreadBase::mLock held

//    frameworks/av/services/audioflinger/PlaybackTracks.h
class Track : public TrackBase, public VolumeProvider {

}

/***
 * frameworks/av/services/audioflinger/Tracks.cpp 
 * 
 * 
 * 
 * */
AudioFlinger::PlaybackThread::Track::Track(
            PlaybackThread *thread,
            const sp<Client>& client,
            audio_stream_type_t streamType,
            const audio_attributes_t& attr,
            uint32_t sampleRate,
            audio_format_t format,
            audio_channel_mask_t channelMask,
            size_t frameCount,
            void *buffer,
            size_t bufferSize,
            const sp<IMemory>& sharedBuffer,
            audio_session_t sessionId,
            uid_t uid,
            audio_output_flags_t flags,
            track_type type,
            audio_port_handle_t portId)
    :   TrackBase(thread, client, attr, sampleRate, format, channelMask, frameCount,
                  (sharedBuffer != 0) ? sharedBuffer->pointer() : buffer,
                  (sharedBuffer != 0) ? sharedBuffer->size() : bufferSize,
                  sessionId, uid, true /*isOut*/,
                  (type == TYPE_PATCH) ? ( buffer == NULL ? ALLOC_LOCAL : ALLOC_NONE) : ALLOC_CBLK,
                  type, portId),
    mFillingUpStatus(FS_INVALID),
    // mRetryCount initialized later when needed
    mSharedBuffer(sharedBuffer),
    mStreamType(streamType),
    mName(TRACK_NAME_FAILURE),  // set to TRACK_NAME_PENDING on constructor success.
    /**
     * PlaybackThread::mSinkBuffer
     * 
     * effect_buffer_t *PlaybackThread::sinkBuffer() const { return reinterpret_cast<effect_buffer_t *>(mSinkBuffer); };
    */
    mMainBuffer(thread->sinkBuffer()),
    mAuxBuffer(NULL),
    mAuxEffectId(0), mHasVolumeController(false),
    mPresentationCompleteFrames(0),
    mFrameMap(16 /* sink-frame-to-track-frame map memory */),
    mVolumeHandler(new media::VolumeHandler(sampleRate)),
    // mSinkTimestamp
    mFastIndex(-1),
    mCachedVolume(1.0),
    /* The track might not play immediately after being active, similarly as if its volume was 0.
     * When the track starts playing, its volume will be computed. */
    mFinalVolume(0.f),
    mResumeToStopping(false),
    mFlushHwPending(false),
    mFlags(flags)
{
    // client == 0 implies sharedBuffer == 0
    ALOG_ASSERT(!(client == 0 && sharedBuffer != 0));

    ALOGV_IF(sharedBuffer != 0, "sharedBuffer: %p, size: %zu", sharedBuffer->pointer(),sharedBuffer->size());

    if (mCblk == NULL) {
        return;
    }

    if (sharedBuffer == 0) {
        /**
         * mCblk 在 TrackBase 分配
        */
        mAudioTrackServerProxy = new AudioTrackServerProxy(mCblk, mBuffer, frameCount,mFrameSize, !isExternalTrack(), sampleRate);
    } else {
        mAudioTrackServerProxy = new StaticAudioTrackServerProxy(mCblk, mBuffer, frameCount,mFrameSize);
    }
    mServerProxy = mAudioTrackServerProxy;

    if (!thread->isTrackAllowed_l(channelMask, format, sessionId, uid)) {
        ALOGE("no more tracks available");
        return;
    }
    // only allocate a fast track index if we were able to allocate a normal track name
    if (flags & AUDIO_OUTPUT_FLAG_FAST) {
        // FIXME: Not calling framesReadyIsCalledByMultipleThreads() exposes a potential
        // race with setSyncEvent(). However, if we call it, we cannot properly start
        // static fast tracks (SoundPool) immediately after stopping.
        //mAudioTrackServerProxy->framesReadyIsCalledByMultipleThreads();
        ALOG_ASSERT(thread->mFastTrackAvailMask != 0);
        int i = __builtin_ctz(thread->mFastTrackAvailMask);
        ALOG_ASSERT(0 < i && i < (int)FastMixerState::sMaxFastTracks);
        // FIXME This is too eager.  We allocate a fast track index before the
        //       fast track becomes active.  Since fast tracks are a scarce resource,
        //       this means we are potentially denying other more important fast tracks from
        //       being created.  It would be better to allocate the index dynamically.
        mFastIndex = i;
        thread->mFastTrackAvailMask &= ~(1 << i);
    }
    mName = TRACK_NAME_PENDING;
}

/**
enum track_type {
   TYPE_DEFAULT,    //  表示为 Track 在 sp<AudioFlinger::PlaybackThread::Track> AudioFlinger::PlaybackThread::createTrack_l 中创建
   TYPE_OUTPUT,     // 表示为 OutputTrack  在 void AudioFlinger::DuplicatingThread::addOutputTrack(MixerThread *thread) 中创建
   TYPE_PATCH,      //  表示为 PatchRecord 和 PatchTrack 在 status_t AudioFlinger::PatchPanel::createPatchConnections(Patch *patch,const struct audio_patch *audioPatch)
};
 *
 * status_t AudioFlinger::PatchPanel::createAudioPatch(const struct audio_patch *patch,audio_patch_handle_t *handle)
 * ---> status_t AudioFlinger::PatchPanel::createPatchConnections(Patch *patch,const struct audio_patch *audioPatch)
 * 
*/


bool TrackBase::isExternalTrack() const {
    return !isOutputTrack() && !isPatchTrack(); 
}

bool TrackBase::isOutputTrack() const { 
    return (mType == TYPE_OUTPUT);
}

bool TrackBase::isPatchTrack() const { 
    return (mType == TYPE_PATCH);
}

audio_session_t TrackBase::sessionId() const { 
    return mSessionId; 
}

/**
 * @system/media/audio/include/system/audio-base.h:360:    AUDIO_OUTPUT_FLAG_FAST             = 0x4,
*/
bool        Track::isFastTrack() const { 
     return (mFlags & AUDIO_OUTPUT_FLAG_FAST) != 0; 
}


/**
 * 
 * status_t AudioFlinger::TrackHandle::start() 
 * 
 * 
*/
status_t AudioFlinger::PlaybackThread::Track::start(AudioSystem::sync_event_t event = AudioSystem::SYNC_EVENT_NONE,audio_session_t triggerSession = AUDIO_SESSION_NONE)
{
    status_t status = NO_ERROR;
    ALOGV("start(%d), calling pid %d session %d",  mName, IPCThreadState::self()->getCallingPid(), mSessionId);

    sp<ThreadBase> thread = mThread.promote();
    if (thread != 0) {
        if (isOffloaded()) {
            Mutex::Autolock _laf(thread->mAudioFlinger->mLock);
            Mutex::Autolock _lth(thread->mLock);
            sp<EffectChain> ec = thread->getEffectChain_l(mSessionId);
            if (thread->mAudioFlinger->isNonOffloadableGlobalEffectEnabled_l() ||  (ec != 0 && ec->isNonOffloadableEnabled())) {
                invalidate();
                return PERMISSION_DENIED;
            }
        }
        Mutex::Autolock _lth(thread->mLock);
        track_state state = mState;
        // here the track could be either new, or restarted
        // in both cases "unstop" the track

        // initial state-stopping. next state-pausing.
        // What if resume is called ?

        if (state == PAUSED || state == PAUSING) {
            if (mResumeToStopping) {
                // happened we need to resume to STOPPING_1
                mState = TrackBase::STOPPING_1;
                ALOGV("PAUSED => STOPPING_1 (%d) on thread %p", mName, this);
            } else {
                mState = TrackBase::RESUMING;
                ALOGV("PAUSED => RESUMING (%d) on thread %p", mName, this);
            }
        } else {
            mState = TrackBase::ACTIVE;
            ALOGV("? => ACTIVE (%d) on thread %p", mName, this);
        }

        // states to reset position info for non-offloaded/direct tracks
        if (!isOffloaded() && !isDirect() && (state == IDLE || state == STOPPED || state == FLUSHED)) {
            mFrameMap.reset();
        }
        PlaybackThread *playbackThread = (PlaybackThread *)thread.get();
        if (isFastTrack()) {
            // refresh fast track underruns on start because that field is never cleared
            // by the fast mixer; furthermore, the same track can be recycled, i.e. start
            // after stop.
            mObservedUnderruns = playbackThread->getFastTrackUnderruns(mFastIndex);
        }
        /**
         * 在 PlaybackThread 中实现
        */
        status = playbackThread->addTrack_l(this);
        if (status == INVALID_OPERATION || status == PERMISSION_DENIED) {
            triggerEvents(AudioSystem::SYNC_EVENT_PRESENTATION_COMPLETE);
            //  restore previous state if start was rejected by policy manager
            if (status == PERMISSION_DENIED) {
                mState = state;
            }
        }

        if (status == NO_ERROR || status == ALREADY_EXISTS) {
            // for streaming tracks, remove the buffer read stop limit.
            /**
             * 
             * 
            */
            mAudioTrackServerProxy->start();
        }

        // track was already in the active list, not a problem
        if (status == ALREADY_EXISTS) {
            status = NO_ERROR;
        } else {
            // Acknowledge any pending flush(), so that subsequent new data isn't discarded.
            // It is usually unsafe to access the server proxy from a binder thread.
            // But in this case we know the mixer thread (whether normal mixer or fast mixer)
            // isn't looking at this track yet:  we still hold the normal mixer thread lock,
            // and for fast tracks the track is not yet in the fast mixer thread's active set.
            // For static tracks, this is used to acknowledge change in position or loop.
            ServerProxy::Buffer buffer;
            buffer.mFrameCount = 1;
            (void) mAudioTrackServerProxy->obtainBuffer(&buffer, true /*ackFlush*/);
        }
    } else {
        status = BAD_VALUE;
    }
    return status;
}











//////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
*/
// AudioBufferProvider interface
status_t AudioFlinger::PlaybackThread::Track::getNextBuffer(AudioBufferProvider::Buffer* buffer)
{
    ServerProxy::Buffer buf;
    size_t desiredFrames = buffer->frameCount;
    buf.mFrameCount = desiredFrames;
    status_t status = mServerProxy->obtainBuffer(&buf);
    buffer->frameCount = buf.mFrameCount;
    buffer->raw = buf.mRaw;
    if (buf.mFrameCount == 0 && !isStopping() && !isStopped() && !isPaused()) {
        ALOGV("underrun,  framesReady(%zu) < framesDesired(%zd), state: %d",buf.mFrameCount, desiredFrames, mState);
        mAudioTrackServerProxy->tallyUnderrunFrames(desiredFrames);
    } else {
        mAudioTrackServerProxy->tallyUnderrunFrames(0);
    }

    return status;
}