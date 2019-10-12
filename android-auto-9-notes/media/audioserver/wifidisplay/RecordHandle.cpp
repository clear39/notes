/**
 * RecordHandle 是 IAudioRecode 服务端
 * 
 * 
*/


//  实现 @ frameworks/av/services/audioflinger/Tracks.cpp
/**
 * 参数是 recordTrack 通过 RecordThread 的 createRecordTrack_l 得到
*/
AudioFlinger::RecordHandle::RecordHandle(const sp<AudioFlinger::RecordThread::RecordTrack>& recordTrack)
    : BnAudioRecord(),
    mRecordTrack(recordTrack)
{
}


binder::Status AudioFlinger::RecordHandle::start(int /*AudioSystem::sync_event_t*/ event,int /*audio_session_t*/ triggerSession) {
    ALOGV("RecordHandle::start()");
    return binder::Status::fromStatusT(mRecordTrack->start((AudioSystem::sync_event_t)event, (audio_session_t) triggerSession));
}


