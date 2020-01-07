

/**
 * sp<IAudioTrack> AudioFlinger::createTrack(
 *  -->  registerPid(clientPid);
*/
AudioFlinger::Client::Client(const sp<AudioFlinger>& audioFlinger, pid_t pid)
    :   RefBase(),
        mAudioFlinger(audioFlinger),
        mPid(pid)
{
    /**
     * audioFlinger->getClientSharedHeapSize() 获取大小
    */
    mMemoryDealer = new MemoryDealer(audioFlinger->getClientSharedHeapSize(), (std::string("AudioFlinger::Client(") + std::to_string(pid) + ")").c_str());
}

/**
 * sp<IAudioTrack> AudioFlinger::createTrack(
 * ->AudioFlinger::PlaybackThread::Track::Track()
 * ---> AudioFlinger::ThreadBase::TrackBase::TrackBase()
 * ---->mCblkMemory = client->heap()->allocate(size);
*/
sp<MemoryDealer> AudioFlinger::Client::heap() const
{
    return mMemoryDealer;
}