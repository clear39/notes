
//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/av/media/libaudioprocessing/AudioMixer.cpp


AudioMixerAudioMixer(size_t frameCount, uint32_t sampleRate)
    : mSampleRate(sampleRate)
    , mFrameCount(frameCount) {
    pthread_once(&sOnceControl, &sInitRoutine);
}