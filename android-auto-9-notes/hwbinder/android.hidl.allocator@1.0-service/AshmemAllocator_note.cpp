
// hidl_memory @/work/workcodes/aosp-p9.x-auto-alpha/system/libhidl/base/include/hidl/HidlSupport.h
struct hidl_memory {
    /**
     * Creates a hidl_memory object, but doesn't take ownership of
     * the passed in native_handle_t; callers are responsible for
     * making sure the handle remains valid while this object is
     * used.
     */
    hidl_memory(const hidl_string &name, const native_handle_t *handle, size_t size)
      :  mHandle(handle),
         mSize(size),
         mName(name)
    {}


    const native_handle_t* handle() const {
        return mHandle;
    }
}


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

    
    //  native_handle_t @ /work/workcodes/aosp-p9.x-auto-alpha/system/core/libcutils/include/cutils/native_handle.h

    //  native_handle_create @/work/workcodes/aosp-p9.x-auto-alpha/system/core/libcutils/native_handle.cpp
    native_handle_t* handle = native_handle_create(1, 0); //构建一个 native_handle_t 结构体
    handle->data[0] = fd; //保存文件句柄
    LOG(WARNING) << "ashmem_create_region(" << size << ") returning hidl_memory(" << handle   << ", " << size << ")";
    return hidl_memory("ashmem", handle, size);
}








//  batchAllocate 为批量分配

Return<void> AshmemAllocator::batchAllocate(uint64_t size, uint64_t count, batchAllocate_cb _hidl_cb) {
    // resize fails if count > 2^32
    if (count > UINT32_MAX) {
        _hidl_cb(false /* success */, {});
        return Void();
    }
    //  @/work/workcodes/aosp-p9.x-auto-alpha/system/libhidl/base/include/hidl/HidlSupport.h
    hidl_vec<hidl_memory> batch; //hidl_vec 内部实现为创建 hidl_memory 数组
    batch.resize(count);    // 重置数组大小

    uint64_t allocated;
    for (allocated = 0; allocated < count; allocated++) {
        batch[allocated] = allocateOne(size); //为每个 hidl_memory 分配空间

        if (batch[allocated].handle() == nullptr) {
            LOG(WARNING) << "batchAllocate(" << size << ", " << count << ") fails @ #" << allocated;
            break;
        }
    }

    // batch[i].handle() != nullptr for i in [0, allocated - 1].
    // batch[i].handle() == nullptr for i in [allocated, count - 1].

    if (allocated < count) {
        _hidl_cb(false /* success */, {});
    } else {
        _hidl_cb(true /* success */, batch);
    }

    for (uint64_t i = 0; i < allocated; i++) {
        cleanup(std::move(batch[i]));
    }

    return Void();
}








static void cleanup(hidl_memory&& memory) {
    if (memory.handle() == nullptr) {
        return;
    }

    native_handle_close(const_cast<native_handle_t *>(memory.handle()));
    native_handle_delete(const_cast<native_handle_t *>(memory.handle()));
}




//  @/work/workcodes/aosp-p9.x-auto-alpha/system/core/libcutils/native_handle.cpp
// 关闭文件
int native_handle_close(const native_handle_t* h) {
    if (h->version != sizeof(native_handle_t)) return -EINVAL;

    int saved_errno = errno;
    const int numFds = h->numFds;
    for (int i = 0; i < numFds; ++i) {
        close(h->data[i]);
    }
    errno = saved_errno;
    return 0;
}



//  @/work/workcodes/aosp-p9.x-auto-alpha/system/core/libcutils/native_handle.cpp
// 释放native_handle_t 创建空间
int native_handle_delete(native_handle_t* h) {
    if (h) {
        if (h->version != sizeof(native_handle_t)) return -EINVAL;
        free(h);
    }
    return 0;
}
