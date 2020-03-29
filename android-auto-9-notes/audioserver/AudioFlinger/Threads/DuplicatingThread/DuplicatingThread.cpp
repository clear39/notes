

AudioFlinger::DuplicatingThread::DuplicatingThread(const sp<AudioFlinger>& audioFlinger,AudioFlinger::MixerThread* mainThread, audio_io_handle_t id, bool systemReady)
    :   MixerThread(audioFlinger, mainThread->getOutput(), id, mainThread->outDevice(),systemReady, DUPLICATING),mWaitTimeMs(UINT_MAX)
{
    addOutputTrack(mainThread);
}


void AudioFlinger::DuplicatingThread::addOutputTrack(MixerThread *thread)
{
    Mutex::Autolock _l(mLock);
    // The downstream MixerThread consumes thread->frameCount() amount of frames per mix pass.
    // Adjust for thread->sampleRate() to determine minimum buffer frame count.
    // Then triple buffer because Threads do not run synchronously and may not be clock locked.
    const size_t frameCount =   3 * sourceFramesNeeded(mSampleRate, thread->frameCount(), thread->sampleRate());
    // TODO: Consider asynchronous sample rate conversion to handle clock disparity
    // from different OutputTracks and their associated MixerThreads (e.g. one may
    // nearly empty and the other may be dropping data).

    sp<OutputTrack> outputTrack = new OutputTrack(thread,
                                            this,
                                            mSampleRate,
                                            mFormat,
                                            mChannelMask,
                                            frameCount, IPCThreadState::self()->getCallingUid());
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


void AudioFlinger::DuplicatingThread::removeOutputTrack(MixerThread *thread)
{
    Mutex::Autolock _l(mLock);
    for (size_t i = 0; i < mOutputTracks.size(); i++) {
        if (mOutputTracks[i]->thread() == thread) {
            mOutputTracks[i]->destroy();
            mOutputTracks.removeAt(i);
            updateWaitTime_l();
            if (thread->getOutput() == mOutput) {
                mOutput = NULL;
            }
            return;
        }
    }
    ALOGV("removeOutputTrack(): unknown thread: %p", thread);
}

// caller must hold mLock
void AudioFlinger::DuplicatingThread::updateWaitTime_l()
{
    mWaitTimeMs = UINT_MAX;
    for (size_t i = 0; i < mOutputTracks.size(); i++) {
        sp<ThreadBase> strong = mOutputTracks[i]->thread().promote();
        if (strong != 0) {
            uint32_t waitTimeMs = (strong->frameCount() * 2 * 1000) / strong->sampleRate();
            if (waitTimeMs < mWaitTimeMs) {
                mWaitTimeMs = waitTimeMs;
            }
        }
    }
}



void AudioFlinger::DuplicatingThread::saveOutputTracks()
{
    outputTracks = mOutputTracks;
}

void AudioFlinger::DuplicatingThread::clearOutputTracks()
{
    outputTracks.clear();
}

ssize_t AudioFlinger::DuplicatingThread::threadLoop_write()
{
    for (size_t i = 0; i < outputTracks.size(); i++) {
        outputTracks[i]->write(mSinkBuffer, writeFrames);
    }
    mStandby = false;
    return (ssize_t)mSinkBufferSize;
}