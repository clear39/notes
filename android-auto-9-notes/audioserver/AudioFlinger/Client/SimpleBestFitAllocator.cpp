//  @   frameworks/native/libs/binder/MemoryDealer.cpp

// align all the memory blocks on a cache-line boundary
const int SimpleBestFitAllocator::kMemoryAlign = 32;

SimpleBestFitAllocator::SimpleBestFitAllocator(size_t size)
{
    size_t pagesize = getpagesize();
    mHeapSize = ((size + pagesize-1) & ~(pagesize-1));

    chunk_t* node = new chunk_t(0, mHeapSize / kMemoryAlign);
    mList.insertHead(node);
}


size_t SimpleBestFitAllocator::allocate(size_t size, uint32_t flags)
{
    Mutex::Autolock _l(mLock);
    ssize_t offset = alloc(size, flags);
    return offset;
}


ssize_t SimpleBestFitAllocator::alloc(size_t size, uint32_t flags)
{
    if (size == 0) {
        return 0;
    }
    size = (size + kMemoryAlign-1) / kMemoryAlign;
    chunk_t* free_chunk = 0;
    chunk_t* cur = mList.head();

    size_t pagesize = getpagesize();
    while (cur) {
        int extra = 0;
        if (flags & PAGE_ALIGNED)
            extra = ( -cur->start & ((pagesize/kMemoryAlign)-1) ) ;

        // best fit
        if (cur->free && (cur->size >= (size+extra))) {
            if ((!free_chunk) || (cur->size < free_chunk->size)) {
                free_chunk = cur;
            }
            if (cur->size == size) {
                break;
            }
        }
        cur = cur->next;
    }

    if (free_chunk) {
        const size_t free_size = free_chunk->size;
        free_chunk->free = 0;
        free_chunk->size = size;
        if (free_size > size) {
            int extra = 0;
            if (flags & PAGE_ALIGNED)
                extra = ( -free_chunk->start & ((pagesize/kMemoryAlign)-1) ) ;
            if (extra) {
                chunk_t* split = new chunk_t(free_chunk->start, extra);
                free_chunk->start += extra;
                mList.insertBefore(free_chunk, split);
            }

            ALOGE_IF((flags&PAGE_ALIGNED) && 
                    ((free_chunk->start*kMemoryAlign)&(pagesize-1)),
                    "PAGE_ALIGNED requested, but page is not aligned!!!");

            const ssize_t tail_free = free_size - (size+extra);
            if (tail_free > 0) {
                chunk_t* split = new chunk_t(
                        free_chunk->start + free_chunk->size, tail_free);
                mList.insertAfter(free_chunk, split);
            }
        }
        return (free_chunk->start)*kMemoryAlign;
    }
    return NO_MEMORY;
}