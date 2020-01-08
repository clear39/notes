
class MemoryBase : public BnMemory {

}


class Allocation : public MemoryBase {
    
}

//  @   frameworks/native/libs/binder/MemoryDealer.cpp
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



void* IMemory::pointer() const {
    ssize_t offset;
    sp<IMemoryHeap> heap = getMemory(&offset);
    void* const base = heap!=0 ? heap->base() : MAP_FAILED;
    if (base == MAP_FAILED)
        return 0;
    return static_cast<char*>(base) + offset;
}