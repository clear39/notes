

//  @  /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libaudioclient/AudioTrack.cpp 


status_t AudioTrack::obtainBuffer(Buffer* audioBuffer, const struct timespec *requested,
        struct timespec *elapsed, size_t *nonContig)
{
    // previous and new IAudioTrack sequence numbers are used to detect track re-creation
    uint32_t oldSequence = 0;
    uint32_t newSequence;

    Proxy::Buffer buffer;
    status_t status = NO_ERROR;

    static const int32_t kMaxTries = 5;
    int32_t tryCounter = kMaxTries;

    do {
        // obtainBuffer() is called with mutex unlocked, so keep extra references to these fields to
        // keep them from going away if another thread re-creates the track during obtainBuffer()
        sp<AudioTrackClientProxy> proxy;
        sp<IMemory> iMem;

        {   // start of lock scope
            AutoMutex lock(mLock);

            newSequence = mSequence;
            // did previous obtainBuffer() fail due to media server death or voluntary invalidation?
            /***
             * 只有 status = DEAD_OBJECT
             * 注意这里 status 为临时变量，
             * 且只有后面一个地方赋值
             * */
            if (status == DEAD_OBJECT) {
                // re-create track, unless someone else has already done so
                if (newSequence == oldSequence) {
                    /**
                     * 当submix切换线程时，从堆栈信息打印，
                     * 由这里触发
                    */
                    status = restoreTrack_l("obtainBuffer");
                    if (status != NO_ERROR) {
                        buffer.mFrameCount = 0;
                        buffer.mRaw = NULL;
                        buffer.mNonContig = 0;
                        break;
                    }
                }
            }
            oldSequence = newSequence;

            if (status == NOT_ENOUGH_DATA) {
                restartIfDisabled();
            }

            // Keep the extra references
            proxy = mProxy;
            iMem = mCblkMemory;

            if (mState == STATE_STOPPING) {
                status = -EINTR;
                buffer.mFrameCount = 0;
                buffer.mRaw = NULL;
                buffer.mNonContig = 0;
                break;
            }

            // Non-blocking if track is stopped or paused
            if (mState != STATE_ACTIVE) {
                requested = &ClientProxy::kNonBlocking;
            }

        }   // end of lock scope

        buffer.mFrameCount = audioBuffer->frameCount;

        // FIXME starts the requested timeout and elapsed over from scratch
        /**
         * 非静态 proxy = mProxy; 
         * mProxy 由 AudioTrack::createTrack_l() 创建
         * mProxy = new AudioTrackClientProxy(cblk, buffers, mFrameCount, mFrameSize);
        */
        status = proxy->obtainBuffer(&buffer, requested, elapsed);

    } while (((status == DEAD_OBJECT) || (status == NOT_ENOUGH_DATA)) && (tryCounter-- > 0));

    audioBuffer->frameCount = buffer.mFrameCount;
    audioBuffer->size = buffer.mFrameCount * mFrameSize;
    audioBuffer->raw = buffer.mRaw;
    if (nonContig != NULL) {
        *nonContig = buffer.mNonContig;
    }
    return status;
}


/**
 * AudioTrack::obtainBuffer
 * --> restoreTrack_l("obtainBuffer");
 * 
*/
status_t AudioTrack::restoreTrack_l(const char *from)
{
    ALOGW("dead IAudioTrack, %s, creating a new one from %s()",isOffloadedOrDirect_l() ? "Offloaded or Direct" : "PCM", from);
    ++mSequence;

    // refresh the audio configuration cache in this process to make sure we get new
    // output parameters and new IAudioFlinger in createTrack_l()
    AudioSystem::clearAudioConfigCache();

    if (isOffloadedOrDirect_l() || mDoNotReconnect) {
        // FIXME re-creation of offloaded and direct tracks is not yet implemented;
        // reconsider enabling for linear PCM encodings when position can be preserved.
        return DEAD_OBJECT;
    }

    // Save so we can return count since creation.
    mUnderrunCountOffset = getUnderrunCount_l();

    // save the old static buffer position
    uint32_t staticPosition = 0;
    size_t bufferPosition = 0;
    int loopCount = 0;
    if (mStaticProxy != 0) {
        mStaticProxy->getBufferPositionAndLoopCount(&bufferPosition, &loopCount);
        staticPosition = mStaticProxy->getPosition().unsignedValue();
    }

    // See b/74409267. Connecting to a BT A2DP device supporting multiple codecs
    // causes a lot of churn on the service side, and it can reject starting
    // playback of a previously created track. May also apply to other cases.
    const int INITIAL_RETRIES = 3;
    int retries = INITIAL_RETRIES;
retry:
    if (retries < INITIAL_RETRIES) {
        // See the comment for clearAudioConfigCache at the start of the function.
        AudioSystem::clearAudioConfigCache();
    }
    mFlags = mOrigFlags;

    // If a new IAudioTrack is successfully created, createTrack_l() will modify the
    // following member variables: mAudioTrack, mCblkMemory and mCblk.
    // It will also delete the strong references on previous IAudioTrack and IMemory.
    // If a new IAudioTrack cannot be created, the previous (dead) instance will be left intact.
    status_t result = createTrack_l();

    if (result != NO_ERROR) {
        ALOGW("%s(): createTrack_l failed, do not retry", __func__);
        retries = 0;
    } else {
        // take the frames that will be lost by track recreation into account in saved position
        // For streaming tracks, this is the amount we obtained from the user/client
        // (not the number actually consumed at the server - those are already lost).
        if (mStaticProxy == 0) {
            mPosition = mReleased;
        }
        // Continue playback from last known position and restore loop.
        if (mStaticProxy != 0) {
            if (loopCount != 0) {
                mStaticProxy->setBufferPositionAndLoop(bufferPosition,
                        mLoopStart, mLoopEnd, loopCount);
            } else {
                mStaticProxy->setBufferPosition(bufferPosition);
                if (bufferPosition == mFrameCount) {
                    ALOGD("restoring track at end of static buffer");
                }
            }
        }
        // restore volume handler
        mVolumeHandler->forall([this](const VolumeShaper &shaper) -> VolumeShaper::Status {
            sp<VolumeShaper::Operation> operationToEnd =
                    new VolumeShaper::Operation(shaper.mOperation);
            // TODO: Ideally we would restore to the exact xOffset position
            // as returned by getVolumeShaperState(), but we don't have that
            // information when restoring at the client unless we periodically poll
            // the server or create shared memory state.
            //
            // For now, we simply advance to the end of the VolumeShaper effect
            // if it has been started.
            if (shaper.isStarted()) {
                operationToEnd->setNormalizedTime(1.f);
            }
            return mAudioTrack->applyVolumeShaper(shaper.mConfiguration, operationToEnd);
        });

        if (mState == STATE_ACTIVE) {
            result = mAudioTrack->start();
        }
        // server resets to zero so we offset
        mFramesWrittenServerOffset =
                mStaticProxy.get() != nullptr ? staticPosition : mFramesWritten;
        mFramesWrittenAtRestore = mFramesWrittenServerOffset;
    }
    if (result != NO_ERROR) {
        ALOGW("%s() failed status %d, retries %d", __func__, result, retries);
        if (--retries > 0) {
            goto retry;
        }
        mState = STATE_STOPPED;
        mReleased = 0;
    }

    return result;
}