


AudioFlinger::Client::Client(const sp<AudioFlinger>& audioFlinger, pid_t pid)
    :   RefBase(),
        mAudioFlinger(audioFlinger),
        mPid(pid)
{
    mMemoryDealer = new MemoryDealer(audioFlinger->getClientSharedHeapSize(), (std::string("AudioFlinger::Client(") + std::to_string(pid) + ")").c_str());
}


sp<MemoryDealer> AudioFlinger::Client::heap() const
{
    return mMemoryDealer;
}