//  @   frameworks/av/services/audioflinger/RecordTracks.h

//  @   frameworks/av/services/audioflinger/Tracks.cpp


AudioFlinger::RecordThread::RecordTrack::RecordTrack(
            RecordThread *thread,
            const sp<Client>& client,
            const audio_attributes_t& attr,
            uint32_t sampleRate,
            audio_format_t format,
            audio_channel_mask_t channelMask,
            size_t frameCount,
            void *buffer,
            size_t bufferSize,
            audio_session_t sessionId,
            uid_t uid,
            audio_input_flags_t flags,
            track_type type,
            audio_port_handle_t portId)
    :   TrackBase(thread, client, attr, sampleRate, format,
                  channelMask, frameCount, buffer, bufferSize, sessionId, uid, false /*isOut*/,
                  (type == TYPE_DEFAULT) ? ((flags & AUDIO_INPUT_FLAG_FAST) ? ALLOC_PIPE : ALLOC_CBLK) : ((buffer == NULL) ? ALLOC_LOCAL : ALLOC_NONE),
                  type, portId),
        mOverflow(false),
        mFramesToDrop(0),
        mResamplerBufferProvider(NULL), // initialize in case of early constructor exit
        mRecordBufferConverter(NULL),
        mFlags(flags),
        mSilenced(false)
{
    if (mCblk == NULL) {
        return;
    }

    mRecordBufferConverter = new RecordBufferConverter(thread->mChannelMask, thread->mFormat, thread->mSampleRate,channelMask, format, sampleRate);
    // Check if the RecordBufferConverter construction was successful.
    // If not, don't continue with construction.
    //
    // NOTE: It would be extremely rare that the record track cannot be created
    // for the current device, but a pending or future device change would make
    // the record track configuration valid.
    if (mRecordBufferConverter->initCheck() != NO_ERROR) {
        ALOGE("RecordTrack unable to create record buffer converter");
        return;
    }

    mServerProxy = new AudioRecordServerProxy(mCblk, mBuffer, frameCount,mFrameSize, !isExternalTrack());

    mResamplerBufferProvider = new ResamplerBufferProvider(this);

    if (flags & AUDIO_INPUT_FLAG_FAST) {
        ALOG_ASSERT(thread->mFastTrackAvail);
        thread->mFastTrackAvail = false;
    }
}



status_t AudioFlinger::RecordThread::RecordTrack::start(AudioSystem::sync_event_t event,audio_session_t triggerSession)
{
    sp<ThreadBase> thread = mThread.promote();
    if (thread != 0) {
        RecordThread *recordThread = (RecordThread *)thread.get();
        return recordThread->start(this, event, triggerSession);
    } else {
        return BAD_VALUE;
    }
}

bool        RecordTrack::isOutputTrack() const { return (mType == TYPE_OUTPUT); }

bool        RecordTrack::isPatchTrack() const { return (mType == TYPE_PATCH); }

bool        RecordTrack::isExternalTrack() const { return !isOutputTrack() && !isPatchTrack(); }