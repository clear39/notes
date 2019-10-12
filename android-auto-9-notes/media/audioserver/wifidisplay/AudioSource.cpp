
//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libstagefright/AudioSource.cpp

/**
 * 
 * psHandle->audioSource = new AudioSource(AUDIO_SOURCE_REMOTE_SUBMIX, String16("MLS_AUDIO_RECORD"), psHandle->uiRecordingSamplingRate = 48000 , psHandle->uiRecordingNumChannels  = 2);
 * 
 * 
*/
AudioSource::AudioSource(
        audio_source_t inputSource, const String16 &opPackageName,
        uint32_t sampleRate, uint32_t channelCount, uint32_t outSampleRate /*= 0*/,
        uid_t uid /*= -1*/, pid_t pid /*= -1*/, audio_port_handle_t selectedDeviceId /*= AUDIO_PORT_HANDLE_NONE*/)
    : mStarted(false),
      mSampleRate(sampleRate),
      mOutSampleRate(outSampleRate > 0 ? outSampleRate : sampleRate),
      mTrackMaxAmplitude(false),
      mStartTimeUs(0),
      mStopSystemTimeUs(-1),
      mLastFrameTimestampUs(0),
      mMaxAmplitude(0),
      mPrevSampleTimeUs(0),
      mInitialReadTimeUs(0),
      mNumFramesReceived(0),
      mNumFramesSkipped(0),
      mNumFramesLost(0),
      mNumClientOwnedBuffers(0),
      mNoMoreFramesToRead(false) {
    ALOGV("sampleRate: %u, outSampleRate: %u, channelCount: %u", sampleRate, outSampleRate, channelCount);
    CHECK(channelCount == 1 || channelCount == 2);
    CHECK(sampleRate > 0);

    size_t minFrameCount;
    status_t status = AudioRecord::getMinFrameCount(&minFrameCount,sampleRate,AUDIO_FORMAT_PCM_16_BIT,audio_channel_in_mask_from_count(channelCount));
    if (status == OK) {
        // make sure that the AudioRecord callback never returns more than the maximum
        // buffer size
        uint32_t frameCount = kMaxBufferSize / sizeof(int16_t) / channelCount;

        // make sure that the AudioRecord total buffer size is large enough
        size_t bufCount = 2;
        while ((bufCount * frameCount) < minFrameCount) {
            bufCount++;
        }

        mRecord = new AudioRecord(
                    inputSource, sampleRate, AUDIO_FORMAT_PCM_16_BIT,
                    audio_channel_in_mask_from_count(channelCount),
                    opPackageName,
                    (size_t) (bufCount * frameCount),
                    AudioRecordCallbackFunction,
                    this,
                    frameCount /*notificationFrames*/,
                    AUDIO_SESSION_ALLOCATE,
                    AudioRecord::TRANSFER_DEFAULT,
                    AUDIO_INPUT_FLAG_NONE,
                    uid,
                    pid,
                    NULL /*pAttributes*/,
                    selectedDeviceId);
        mInitCheck = mRecord->initCheck();
        if (mInitCheck != OK) {
            mRecord.clear();
        }
    } else {
        mInitCheck = status;
    }
}

static void AudioRecordCallbackFunction(int event, void *user, void *info) {
    AudioSource *source = (AudioSource *) user;
    switch (event) {
        case AudioRecord::EVENT_MORE_DATA: {
            source->dataCallback(*((AudioRecord::Buffer *) info));
            break;
        }
        case AudioRecord::EVENT_OVERRUN: {
            ALOGW("AudioRecord reported overrun!");
            break;
        }
        default:
            // does nothing
            break;
    }
}

status_t AudioSource::dataCallback(const AudioRecord::Buffer& audioBuffer) {
    int64_t timeUs, position, timeNs;
    ExtendedTimestamp ts;
    ExtendedTimestamp::Location location;
    const int32_t usPerSec = 1000000;

    if (mRecord->getTimestamp(&ts) == OK && ts.getBestTimestamp(&position, &timeNs, ExtendedTimestamp::TIMEBASE_MONOTONIC, &location) == OK) {
        // Use audio timestamp.
        timeUs = timeNs / 1000 - (position - mNumFramesSkipped - mNumFramesReceived + mNumFramesLost) * usPerSec / mSampleRate;
    } else {
        // This should not happen in normal case.
        ALOGW("Failed to get audio timestamp, fallback to use systemclock");
        timeUs = systemTime() / 1000ll;
        // Estimate the real sampling time of the 1st sample in this buffer
        // from AudioRecord's latency. (Apply this adjustment first so that
        // the start time logic is not affected.)
        timeUs -= mRecord->latency() * 1000LL;
    }

    ALOGV("dataCallbackTimestamp: %" PRId64 " us", timeUs);
    Mutex::Autolock autoLock(mLock);
    if (!mStarted) {
        ALOGW("Spurious callback from AudioRecord. Drop the audio data.");
        return OK;
    }

    const size_t bufferSize = audioBuffer.size;

    // Drop retrieved and previously lost audio data.
    if (mNumFramesReceived == 0 && timeUs < mStartTimeUs) {
        (void) mRecord->getInputFramesLost();
        int64_t receievedFrames = bufferSize / mRecord->frameSize();
        ALOGV("Drop audio data(%" PRId64 " frames) at %" PRId64 "/%" PRId64 " us", receievedFrames, timeUs, mStartTimeUs);
        mNumFramesSkipped += receievedFrames;
        return OK;
    }

    if (mStopSystemTimeUs != -1 && timeUs >= mStopSystemTimeUs) {
        ALOGV("Drop Audio frame at %lld  stop time: %lld us", (long long)timeUs, (long long)mStopSystemTimeUs);
        mNoMoreFramesToRead = true;
        mFrameAvailableCondition.signal();
        return OK;
    }

    if (mNumFramesReceived == 0 && mPrevSampleTimeUs == 0) {
        mInitialReadTimeUs = timeUs;
        // Initial delay
        if (mStartTimeUs > 0) {
            mStartTimeUs = timeUs - mStartTimeUs;
        }
        mPrevSampleTimeUs = mStartTimeUs;
    }
    mLastFrameTimestampUs = timeUs;

    size_t numLostBytes = 0;
    if (mNumFramesReceived > 0) {  // Ignore earlier frame lost
        // getInputFramesLost() returns the number of lost frames.
        // Convert number of frames lost to number of bytes lost.
        numLostBytes = mRecord->getInputFramesLost() * mRecord->frameSize();
    }

    CHECK_EQ(numLostBytes & 1, 0u);
    CHECK_EQ(audioBuffer.size & 1, 0u);
    if (numLostBytes > 0) {
        // Loss of audio frames should happen rarely; thus the LOGW should
        // not cause a logging spam
        ALOGW("Lost audio record data: %zu bytes", numLostBytes);
    }

    while (numLostBytes > 0) {
        size_t bufferSize = numLostBytes;
        if (numLostBytes > kMaxBufferSize) {
            numLostBytes -= kMaxBufferSize;
            bufferSize = kMaxBufferSize;
        } else {
            numLostBytes = 0;
        }
        MediaBuffer *lostAudioBuffer = new MediaBuffer(bufferSize);
        memset(lostAudioBuffer->data(), 0, bufferSize);
        lostAudioBuffer->set_range(0, bufferSize);
        mNumFramesLost += bufferSize / mRecord->frameSize();
        queueInputBuffer_l(lostAudioBuffer, timeUs);
    }

    if (audioBuffer.size == 0) {
        ALOGW("Nothing is available from AudioRecord callback buffer");
        return OK;
    }

    MediaBuffer *buffer = new MediaBuffer(bufferSize);
    memcpy((uint8_t *) buffer->data(), audioBuffer.i16, audioBuffer.size);
    buffer->set_range(0, bufferSize);
    queueInputBuffer_l(buffer, timeUs);
    return OK;
}


/**
 * psHandle->audioSource->initCheck() != android::OK
 * 
*/
status_t AudioSource::initCheck() const {
    return mInitCheck;
}


/**
 * 
 *  iStatus = psHandle->audioSource->start(NULL);
*/
status_t AudioSource::start(MetaData *params) {
    Mutex::Autolock autoLock(mLock);
    if (mStarted) {
        return UNKNOWN_ERROR;
    }

    if (mInitCheck != OK) {
        return NO_INIT;
    }

    mTrackMaxAmplitude = false;
    mMaxAmplitude = 0;
    mInitialReadTimeUs = 0;
    mStartTimeUs = 0;
    int64_t startTimeUs;
    if (params && params->findInt64(kKeyTime, &startTimeUs)) {
        mStartTimeUs = startTimeUs;
    }
    /**
     *  status_t    start(AudioSystem::sync_event_t event = AudioSystem::SYNC_EVENT_NONE,audio_session_t triggerSession = AUDIO_SESSION_NONE);
    */
    status_t err = mRecord->start();
    if (err == OK) {
        mStarted = true;
    } else {
        mRecord.clear();
    }


    return err;
}
















//////////////////////////////////////////////////////////////////////////////////////////////////////////

status_t AudioSource::read(MediaBufferBase **out, const ReadOptions * /* options */) {
    Mutex::Autolock autoLock(mLock);
    *out = NULL;

    if (mInitCheck != OK) {
        return NO_INIT;
    }

    while (mStarted && mBuffersReceived.empty()) {
        mFrameAvailableCondition.wait(mLock);
        if (mNoMoreFramesToRead) {
            return OK;
        }
    }
    if (!mStarted) {
        return OK;
    }
    MediaBuffer *buffer = *mBuffersReceived.begin();
    mBuffersReceived.erase(mBuffersReceived.begin());
    ++mNumClientOwnedBuffers;
    buffer->setObserver(this);
    buffer->add_ref();

    // Mute/suppress the recording sound
    int64_t timeUs;
    CHECK(buffer->meta_data().findInt64(kKeyTime, &timeUs));
    int64_t elapsedTimeUs = timeUs - mStartTimeUs;
    if (elapsedTimeUs < kAutoRampStartUs) {
        memset((uint8_t *) buffer->data(), 0, buffer->range_length());
    } else if (elapsedTimeUs < kAutoRampStartUs + kAutoRampDurationUs) {
        int32_t autoRampDurationFrames = ((int64_t)kAutoRampDurationUs * mSampleRate + 500000LL) / 1000000LL; //Need type casting

        int32_t autoRampStartFrames =  ((int64_t)kAutoRampStartUs * mSampleRate + 500000LL) / 1000000LL; //Need type casting

        int32_t nFrames = mNumFramesReceived - autoRampStartFrames;
        rampVolume(nFrames, autoRampDurationFrames, (uint8_t *) buffer->data(), buffer->range_length());
    }

    // Track the max recording signal amplitude.
    if (mTrackMaxAmplitude) {
        trackMaxAmplitude( (int16_t *) buffer->data(), buffer->range_length() >> 1);
    }

    if (mSampleRate != mOutSampleRate) {
            timeUs *= (int64_t)mSampleRate / (int64_t)mOutSampleRate;
            buffer->meta_data().setInt64(kKeyTime, timeUs);
    }

    *out = buffer;
    return OK;
}
