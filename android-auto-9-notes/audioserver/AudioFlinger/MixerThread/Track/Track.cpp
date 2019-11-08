// Track constructor must be called with AudioFlinger::mLock and ThreadBase::mLock held
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
