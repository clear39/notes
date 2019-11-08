
//  @   frameworks/av/media/libaudioprocessing/AudioMixer.cpp


AudioMixer::AudioMixer(size_t frameCount, uint32_t sampleRate)
    : mSampleRate(sampleRate)
    , mFrameCount(frameCount) {
    pthread_once(&sOnceControl, &sInitRoutine);
}