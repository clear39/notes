

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/services/audioflinger/Tracks.cpp
AudioFlinger::RecordHandle::RecordHandle(
        const sp<AudioFlinger::RecordThread::RecordTrack>& recordTrack)
    : BnAudioRecord(),
    mRecordTrack(recordTrack)
{
}


binder::Status AudioFlinger::RecordHandle::start(int /*AudioSystem::sync_event_t*/ event,int /*audio_session_t*/ triggerSession) {
    ALOGV("RecordHandle::start()");
    return binder::Status::fromStatusT(mRecordTrack->start((AudioSystem::sync_event_t)event, (audio_session_t) triggerSession));
}