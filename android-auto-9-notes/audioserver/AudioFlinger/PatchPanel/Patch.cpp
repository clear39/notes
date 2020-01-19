
/**
 * @    frameworks/av/services/audioflinger/PatchPanel.h
*/
class Patch {
        audio_patch_handle_t            mHandle;
        // handle for audio HAL patch handle present only when the audio HAL version is >= 3.0
        audio_patch_handle_t            mHalHandle;
        // below members are used by a software audio patch connecting a source device from a
        // given audio HW module to a sink device on an other audio HW module.
        // playback thread created by createAudioPatch() and released by clearPatchConnections() if
        // no existing playback thread can be used by the software patch
        sp<PlaybackThread>              mPlaybackThread;
        // audio track created by createPatchConnections() and released by clearPatchConnections()
        sp<PlaybackThread::PatchTrack>  mPatchTrack;
        // record thread created by createAudioPatch() and released by clearPatchConnections()
        sp<RecordThread>                mRecordThread;
        // audio record created by createPatchConnections() and released by clearPatchConnections()
        sp<RecordThread::PatchRecord>   mPatchRecord;
        // handle for audio patch connecting source device to record thread input.
        // created by createPatchConnections() and released by clearPatchConnections()
        audio_patch_handle_t            mRecordPatchHandle;
        // handle for audio patch connecting playback thread output to sink device
        // created by createPatchConnections() and released by clearPatchConnections()
        audio_patch_handle_t            mPlaybackPatchHandle;
}



/**
 * @    frameworks/av/services/audioflinger/PatchPanel.cpp
*/
Patch::Patch(const struct audio_patch *patch) :
            mAudioPatch(*patch), mHandle(AUDIO_PATCH_HANDLE_NONE),
            mHalHandle(AUDIO_PATCH_HANDLE_NONE), mRecordPatchHandle(AUDIO_PATCH_HANDLE_NONE),
            mPlaybackPatchHandle(AUDIO_PATCH_HANDLE_NONE) {
        
}