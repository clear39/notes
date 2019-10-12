//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/services/audioflinger/Threads.cpp
/**
 * **/
AudioFlinger::DuplicatingThread::DuplicatingThread(const sp<AudioFlinger>& audioFlinger,AudioFlinger::MixerThread* mainThread, audio_io_handle_t id, bool systemReady)
    :   MixerThread(audioFlinger, mainThread->getOutput(), id, mainThread->outDevice(),systemReady, DUPLICATING),
        mWaitTimeMs(UINT_MAX)
{
    ALOGV("addOutputTrack() mainThread %p", mainThread);
    addOutputTrack(mainThread);
}


void AudioFlinger::DuplicatingThread::addOutputTrack(MixerThread *thread)
{
    Mutex::Autolock _l(mLock);
    // The downstream MixerThread consumes thread->frameCount() amount of frames per mix pass.
    // Adjust for thread->sampleRate() to determine minimum buffer frame count.
    // Then triple buffer because Threads do not run synchronously and may not be clock locked.
    const size_t frameCount = 3 * sourceFramesNeeded(mSampleRate, thread->frameCount(), thread->sampleRate());
    // TODO: Consider asynchronous sample rate conversion to handle clock disparity
    // from different OutputTracks and their associated MixerThreads (e.g. one may
    // nearly empty and the other may be dropping data).

    sp<OutputTrack> outputTrack = new OutputTrack(thread,
                                            this,
                                            mSampleRate,
                                            mFormat,
                                            mChannelMask,
                                            frameCount,
                                            IPCThreadState::self()->getCallingUid());

    status_t status = outputTrack != 0 ? outputTrack->initCheck() : (status_t) NO_MEMORY;
    if (status != NO_ERROR) {
        ALOGE("addOutputTrack() initCheck failed %d", status);
        return;
    }
    thread->setStreamVolume(AUDIO_STREAM_PATCH, 1.0f);

    mOutputTracks.add(outputTrack);

    ALOGV("addOutputTrack() track %p, on thread %p", outputTrack.get(), thread);

    updateWaitTime_l();
}

void AudioFlinger::PlaybackThread::setStreamVolume(audio_stream_type_t stream, float value)
{
    Mutex::Autolock _l(mLock);
    mStreamTypes[stream].volume = value;
    broadcast_l();
}

void AudioFlinger::ThreadBase::broadcast_l()
{
    // Thread could be blocked waiting for async
    // so signal it to handle state changes immediately
    // If threadLoop is currently unlocked a signal of mWaitWorkCV will
    // be lost so we also flag to prevent it blocking on mWaitWorkCV
    mSignalPending = true;
    mWaitWorkCV.broadcast();
}





void AudioFlinger::PlaybackThread::ioConfigChanged(audio_io_config_event event, pid_t pid) {
    sp<AudioIoDescriptor> desc = new AudioIoDescriptor();
    ALOGV("PlaybackThread::ioConfigChanged, thread %p, event %d", this, event);

    desc->mIoHandle = mId;

    switch (event) {
    case AUDIO_OUTPUT_OPENED:      //  这里
    case AUDIO_OUTPUT_REGISTERED:
    case AUDIO_OUTPUT_CONFIG_CHANGED:
        desc->mPatch = mPatch;
        desc->mChannelMask = mChannelMask;
        desc->mSamplingRate = mSampleRate;
        desc->mFormat = mFormat;
        desc->mFrameCount = mNormalFrameCount; // FIXME see  AudioFlinger::frameCount(audio_io_handle_t)
        desc->mFrameCountHAL = mFrameCount;
        desc->mLatency = latency_l();
        break;

    case AUDIO_OUTPUT_CLOSED:
    default:
        break;
    }
    mAudioFlinger->ioConfigChanged(event, desc, pid);
}















ssize_t AudioFlinger::DuplicatingThread::threadLoop_write()
{
    ALOGV("DuplicatingThread::threadLoop_write()");
    for (size_t i = 0; i < outputTracks.size(); i++) {
        outputTracks[i]->write(mSinkBuffer, writeFrames);
    }
    mStandby = false;
    return (ssize_t)mSinkBufferSize;
}
