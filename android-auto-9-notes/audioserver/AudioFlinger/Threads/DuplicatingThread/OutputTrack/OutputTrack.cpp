
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audioflinger/Tracks.cpp


AudioFlinger::PlaybackThread::OutputTrack::OutputTrack(
            PlaybackThread *playbackThread,
            DuplicatingThread *sourceThread,
            uint32_t sampleRate,
            audio_format_t format,
            audio_channel_mask_t channelMask,
            size_t frameCount,
            uid_t uid)
    :   Track(playbackThread, NULL, AUDIO_STREAM_PATCH,
              audio_attributes_t{} /* currently unused for output track */,
              sampleRate, format, channelMask, frameCount,
              nullptr /* buffer */, (size_t)0 /* bufferSize */, nullptr /* sharedBuffer */,
              AUDIO_SESSION_NONE, uid, AUDIO_OUTPUT_FLAG_NONE,
              TYPE_OUTPUT),
    mActive(false), mSourceThread(sourceThread)
{

    if (mCblk != NULL) {
        mOutBuffer.frameCount = 0;
        playbackThread->mTracks.add(this);
        ALOGV("OutputTrack constructor mCblk %p, mBuffer %p, "
                "frameCount %zu, mChannelMask 0x%08x",
                mCblk, mBuffer,
                frameCount, mChannelMask);
        // since client and server are in the same process,
        // the buffer has the same virtual address on both sides
        mClientProxy = new AudioTrackClientProxy(mCblk, mBuffer, mFrameCount, mFrameSize, true /*clientInServer*/);
        mClientProxy->setVolumeLR(GAIN_MINIFLOAT_PACKED_UNITY);
        mClientProxy->setSendLevel(0.0);
        mClientProxy->setSampleRate(sampleRate);
    } else {
        ALOGW("Error creating output track on thread %p", playbackThread);
    }
}














bool AudioFlinger::PlaybackThread::OutputTrack::write(void* data, uint32_t frames)
{
    Buffer *pInBuffer;
    Buffer inBuffer;
    bool outputBufferFull = false;
    inBuffer.frameCount = frames;
    inBuffer.raw = data;

    uint32_t waitTimeLeftMs = mSourceThread->waitTimeMs();//    mSourceThread 为 DuplicatingThread

    if (!mActive && frames != 0) {
        (void) start();
    }

    while (waitTimeLeftMs) {
        // First write pending buffers, then new data
        if (mBufferQueue.size()) {
            pInBuffer = mBufferQueue.itemAt(0);
        } else {
            pInBuffer = &inBuffer;
        }

        if (pInBuffer->frameCount == 0) {
            break;
        }

        if (mOutBuffer.frameCount == 0) {
            mOutBuffer.frameCount = pInBuffer->frameCount;
            nsecs_t startTime = systemTime();
            status_t status = obtainBuffer(&mOutBuffer, waitTimeLeftMs);
            if (status != NO_ERROR && status != NOT_ENOUGH_DATA) {
                ALOGV("OutputTrack::write() %p thread %p no more output buffers; status %d", this, mThread.unsafe_get(), status);
                outputBufferFull = true;
                break;
            }
            uint32_t waitTimeMs = (uint32_t)ns2ms(systemTime() - startTime);
            if (waitTimeLeftMs >= waitTimeMs) {
                waitTimeLeftMs -= waitTimeMs;
            } else {
                waitTimeLeftMs = 0;
            }
            if (status == NOT_ENOUGH_DATA) {
                restartIfDisabled();
                continue;
            }
        }

        uint32_t outFrames = pInBuffer->frameCount > mOutBuffer.frameCount ? mOutBuffer.frameCount :  pInBuffer->frameCount;
        memcpy(mOutBuffer.raw, pInBuffer->raw, outFrames * mFrameSize);
        Proxy::Buffer buf;
        buf.mFrameCount = outFrames;
        buf.mRaw = NULL;
        mClientProxy->releaseBuffer(&buf);
        restartIfDisabled();
        pInBuffer->frameCount -= outFrames;
        pInBuffer->raw = (int8_t *)pInBuffer->raw + outFrames * mFrameSize;
        mOutBuffer.frameCount -= outFrames;
        mOutBuffer.raw = (int8_t *)mOutBuffer.raw + outFrames * mFrameSize;

        if (pInBuffer->frameCount == 0) {
            if (mBufferQueue.size()) {
                mBufferQueue.removeAt(0);
                free(pInBuffer->mBuffer);
                if (pInBuffer != &inBuffer) {
                    delete pInBuffer;
                }
                ALOGV("OutputTrack::write() %p thread %p released overflow buffer %zu", this,mThread.unsafe_get(), mBufferQueue.size());
            } else {
                break;
            }
        }
    }

    // If we could not write all frames, allocate a buffer and queue it for next time.
    if (inBuffer.frameCount) {
        sp<ThreadBase> thread = mThread.promote();
        if (thread != 0 && !thread->standby()) {
            if (mBufferQueue.size() < kMaxOverFlowBuffers) {
                pInBuffer = new Buffer;
                pInBuffer->mBuffer = malloc(inBuffer.frameCount * mFrameSize);
                pInBuffer->frameCount = inBuffer.frameCount;
                pInBuffer->raw = pInBuffer->mBuffer;
                memcpy(pInBuffer->raw, inBuffer.raw, inBuffer.frameCount * mFrameSize);
                mBufferQueue.add(pInBuffer);
                ALOGV("OutputTrack::write() %p thread %p adding overflow buffer %zu", this,
                        mThread.unsafe_get(), mBufferQueue.size());
            } else {
                ALOGW("OutputTrack::write() %p thread %p no more overflow buffers",
                        mThread.unsafe_get(), this);
            }
        }
    }

    // Calling write() with a 0 length buffer means that no more data will be written:
    // We rely on stop() to set the appropriate flags to allow the remaining frames to play out.
    if (frames == 0 && mBufferQueue.size() == 0 && mActive) {
        stop();
    }

    return outputBufferFull;
}