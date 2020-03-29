//  @   frameworks/av/services/audioflinger/Tracks.cpp

/**
 * frameworks/av/services/audioflinger/AudioFlinger.h:532:    class TrackHandle : public android::BnAudioTrack {
 * 
 * sp<IAudioTrack> AudioFlinger::createTrack(const CreateTrackInput& input,CreateTrackOutput& output,status_t *status)
 * 
 * 
 * 
*/
AudioFlinger::TrackHandle::TrackHandle(const sp<AudioFlinger::PlaybackThread::Track>& track)
    : BnAudioTrack(),
      mTrack(track)
{
    
}

/**
 * AudioTrack 创建完成之后，通过该接口得到共享内存
*/
sp<IMemory> AudioFlinger::TrackHandle::getCblk() const {
  /**
   * getCblk 实现在 TrackBase中   @ frameworks/av/services/audioflinger/TrackBase.h
  */
    return mTrack->getCblk();
}



status_t AudioFlinger::TrackHandle::start() {
  /**
   * start是在 Track 中实现   @ frameworks/av/services/audioflinger/Tracks.cpp
  */
    return mTrack->start();
}
