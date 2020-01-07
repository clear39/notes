//  @  frameworks/native/libs/binder/MemoryDealer.cpp

/**
 *sp<IAudioTrack> AudioFlinger::createTrack(
 *-->  registerPid(clientPid);
 * ---->AudioFlinger::Client::Client(const sp<AudioFlinger>& audioFlinger, pid_t pid)
*/
MemoryDealer::MemoryDealer(size_t size, const char* name, uint32_t flags)
    : mHeap(new MemoryHeapBase(size, flags, name)),
    mAllocator(new SimpleBestFitAllocator(size))
{    
}

/**
 * sp<IAudioTrack> AudioFlinger::createTrack(
 * ->AudioFlinger::PlaybackThread::Track::Track()
 * ---> AudioFlinger::ThreadBase::TrackBase::TrackBase()
 * ---->mCblkMemory = client->heap()->allocate(size);
*/
sp<IMemory> MemoryDealer::allocate(size_t size)
{
    sp<IMemory> memory;
    const ssize_t offset = allocator()->allocate(size);
    if (offset >= 0) {
        memory = new Allocation(this, heap(), offset, size);
    }
    return memory;
}

SimpleBestFitAllocator* MemoryDealer::allocator() const {
    return mAllocator;
}



const sp<IMemoryHeap>& MemoryDealer::heap() const {
    return mHeap;
}