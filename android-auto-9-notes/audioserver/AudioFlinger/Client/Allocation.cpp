


class MemoryBase : public BnMemory {

}


class Allocation : public MemoryBase {
    
}

//  @   /work/workcodes/aosp-p9.x-auto-ga/frameworks/native/libs/binder/MemoryDealer.cpp

Allocation::Allocation(
        const sp<MemoryDealer>& dealer,
        const sp<IMemoryHeap>& heap, ssize_t offset, size_t size)
    : MemoryBase(heap, offset, size), mDealer(dealer)
{
#ifndef NDEBUG
    void* const start_ptr = (void*)(intptr_t(heap->base()) + offset);
    memset(start_ptr, 0xda, size);
#endif
}