//	@frameworks/native/libs/binder/MemoryDealer.cpp
MemoryDealer::MemoryDealer(size_t size, const char* name, uint32_t flags)
    : mHeap(new MemoryHeapBase(size, flags, name)),	//	sp<IMemoryHeap>             mHeap;
    mAllocator(new SimpleBestFitAllocator(size))
{    
}


//	@frameworks/native/libs/binder/MemoryHeapBase.cpp
MemoryHeapBase::MemoryHeapBase(size_t size, uint32_t flags, char const * name)
    : mFD(-1), mSize(0), mBase(MAP_FAILED), mFlags(flags),
      mDevice(0), mNeedUnmap(false), mOffset(0)
{
    const size_t pagesize = getpagesize();
    size = ((size + pagesize-1) & ~(pagesize-1));
    int fd = ashmem_create_region(name == NULL ? "MemoryHeapBase" : name, size);
    ALOGE_IF(fd<0, "error creating ashmem region: %s", strerror(errno));
    if (fd >= 0) {
        if (mapfd(fd, size) == NO_ERROR) {
            if (flags & READ_ONLY) {
                ashmem_set_prot_region(fd, PROT_READ);
            }
        }
    }
}


struct chunk_t {
    chunk_t(size_t start, size_t size)
    : start(start), size(size), free(1), prev(0), next(0) {
    }
    size_t              start;
    size_t              size : 28;
    int                 free : 4;
    mutable chunk_t*    prev;
    mutable chunk_t*    next;
};


//	@rameworks/native/libs/binder/MemoryDealer.cpp
SimpleBestFitAllocator::SimpleBestFitAllocator(size_t size)
{
    size_t pagesize = getpagesize();
    mHeapSize = ((size + pagesize-1) & ~(pagesize-1));

    chunk_t* node = new chunk_t(0, mHeapSize / kMemoryAlign);//	const int SimpleBestFitAllocator::kMemoryAlign = 32;
    mList.insertHead(node);	//	LinkedList<chunk_t> mList;
}




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


size_t SimpleBestFitAllocator::allocate(size_t size, uint32_t flags = 0)
{
    Mutex::Autolock _l(mLock);
    ssize_t offset = alloc(size, flags);
    return offset;
}


ssize_t SimpleBestFitAllocator::alloc(size_t size, uint32_t flags = 0)
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

            ALOGE_IF((flags&PAGE_ALIGNED) && ((free_chunk->start*kMemoryAlign)&(pagesize-1)),"PAGE_ALIGNED requested, but page is not aligned!!!");

            const ssize_t tail_free = free_size - (size+extra);
            if (tail_free > 0) {
                chunk_t* split = new chunk_t(free_chunk->start + free_chunk->size, tail_free);
                mList.insertAfter(free_chunk, split);
            }
        }
        return (free_chunk->start)*kMemoryAlign;
    }
    return NO_MEMORY;
}


status_t SimpleBestFitAllocator::deallocate(size_t offset)
{
    Mutex::Autolock _l(mLock);
    chunk_t const * const freed = dealloc(offset);
    if (freed) {
        return NO_ERROR;
    }
    return NAME_NOT_FOUND;
}

SimpleBestFitAllocator::chunk_t* SimpleBestFitAllocator::dealloc(size_t start)
{
    start = start / kMemoryAlign;
    chunk_t* cur = mList.head();
    while (cur) {
        if (cur->start == start) {
            LOG_FATAL_IF(cur->free,
                "block at offset 0x%08lX of size 0x%08lX already freed",
                cur->start*kMemoryAlign, cur->size*kMemoryAlign);

            // merge freed blocks together
            chunk_t* freed = cur;
            cur->free = 1;
            do {
                chunk_t* const p = cur->prev;
                chunk_t* const n = cur->next;
                if (p && (p->free || !cur->size)) {
                    freed = p;
                    p->size += cur->size;
                    mList.remove(cur);
                    delete cur;
                }
                cur = n;
            } while (cur && cur->free);

            #ifndef NDEBUG
                if (!freed->free) {
                    dump_l("dealloc (!freed->free)");
                }
            #endif
            LOG_FATAL_IF(!freed->free,
                "freed block at offset 0x%08lX of size 0x%08lX is not free!",
                freed->start * kMemoryAlign, freed->size * kMemoryAlign);

            return freed;
        }
        cur = cur->next;
    }
    return 0;
}



const sp<IMemoryHeap>& MemoryDealer::heap() const {
    return mHeap;
}


Allocation::Allocation( const sp<MemoryDealer>& dealer, const sp<IMemoryHeap>& heap, ssize_t offset, size_t size)
    : MemoryBase(heap, offset, size), mDealer(dealer)
{
#ifndef NDEBUG
    void* const start_ptr = (void*)(intptr_t(heap->base()) + offset);
    memset(start_ptr, 0xda, size);
#endif
}