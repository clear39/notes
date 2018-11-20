
// using allocate_cb = std::function<void(bool success, const ::android::hardware::hidl_memory& mem)>;

Return<void> AshmemAllocator::allocate(uint64_t size, allocate_cb _hidl_cb) {
    hidl_memory memory = allocateOne(size);
    _hidl_cb(memory.handle() != nullptr /* success */, memory);
    cleanup(std::move(memory));

    return Void();
}


static hidl_memory allocateOne(uint64_t size) {
    int fd = ashmem_create_region("AshmemAllocator_hidl", size);
    if (fd < 0) {
        LOG(WARNING) << "ashmem_create_region(" << size << ") fails with " << fd;
        return hidl_memory();
    }

    native_handle_t* handle = native_handle_create(1, 0);
    handle->data[0] = fd;
    LOG(WARNING) << "ashmem_create_region(" << size << ") returning hidl_memory(" << handle   << ", " << size << ")";
    return hidl_memory("ashmem", handle, size);
}
