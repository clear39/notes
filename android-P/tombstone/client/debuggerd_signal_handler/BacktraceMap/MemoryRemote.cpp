
//  @ system/core/libunwindstack/include/unwindstack/Memory.h


class MemoryRemote : public Memory {
 public:
  MemoryRemote(pid_t pid) : pid_(pid), read_redirect_func_(0) {}
  virtual ~MemoryRemote() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  pid_t pid() { return pid_; }

 private:
  pid_t pid_;
  std::atomic_uintptr_t read_redirect_func_;
};


size_t MemoryRemote::Read(uint64_t addr, void* dst, size_t size) {
#if !defined(__LP64__)
  // Cannot read an address greater than 32 bits in a 32 bit context.
  if (addr > UINT32_MAX) {
    return 0;
  }
#endif

  //  typedef atomic<uintptr_t>		atomic_uintptr_t;
  size_t (*read_func)(pid_t, uint64_t, void*, size_t) = reinterpret_cast<size_t (*)(pid_t, uint64_t, void*, size_t)>(read_redirect_func_.load());
  if (read_func != nullptr) {
    return read_func(pid_, addr, dst, size);
  } else {
    // Prefer process_vm_read, try it first. If it doesn't work, use the
    // ptrace function. If at least one of them returns at least some data,
    // set that as the permanent function to use.
    // This assumes that if process_vm_read works once, it will continue
    // to work.
    size_t bytes = ProcessVmRead(pid_, addr, dst, size);
    if (bytes > 0) {
      read_redirect_func_ = reinterpret_cast<uintptr_t>(ProcessVmRead);
      return bytes;
    }
    bytes = PtraceRead(pid_, addr, dst, size);
    if (bytes > 0) {
      read_redirect_func_ = reinterpret_cast<uintptr_t>(PtraceRead);
    }
    return bytes;
  }
}


static size_t ProcessVmRead(pid_t pid, uint64_t remote_src, void* dst, size_t len) {

  // Split up the remote read across page boundaries.
  // From the manpage:
  //   A partial read/write may result if one of the remote_iov elements points to an invalid
  //   memory region in the remote process.
  //
  //   Partial transfers apply at the granularity of iovec elements.  These system calls won't
  //   perform a partial transfer that splits a single iovec element.
  constexpr size_t kMaxIovecs = 64;
  struct iovec src_iovs[kMaxIovecs];

  uint64_t cur = remote_src;
  size_t total_read = 0;
  while (len > 0) {
    struct iovec dst_iov = {
        .iov_base = &reinterpret_cast<uint8_t*>(dst)[total_read], .iov_len = len,
    };

    size_t iovecs_used = 0;
    while (len > 0) {
      if (iovecs_used == kMaxIovecs) {
        break;
      }

      // struct iovec uses void* for iov_base.
      if (cur >= UINTPTR_MAX) {
        errno = EFAULT;
        return total_read;
      }

      src_iovs[iovecs_used].iov_base = reinterpret_cast<void*>(cur);

      uintptr_t misalignment = cur & (getpagesize() - 1);
      size_t iov_len = getpagesize() - misalignment;
      iov_len = std::min(iov_len, len);

      len -= iov_len;
      if (__builtin_add_overflow(cur, iov_len, &cur)) {
        errno = EFAULT;
        return total_read;
      }

      src_iovs[iovecs_used].iov_len = iov_len;
      ++iovecs_used;
    }

    ssize_t rc = process_vm_readv(pid, &dst_iov, 1, src_iovs, iovecs_used, 0);
    if (rc == -1) {
      return total_read;
    }
    total_read += rc;
  }
  return total_read;
}




static size_t PtraceRead(pid_t pid, uint64_t addr, void* dst, size_t bytes) {
  // Make sure that there is no overflow.
  uint64_t max_size;
  if (__builtin_add_overflow(addr, bytes, &max_size)) {
    return 0;
  }

  size_t bytes_read = 0;
  long data;
  size_t align_bytes = addr & (sizeof(long) - 1);
  if (align_bytes != 0) {
    if (!PtraceReadLong(pid, addr & ~(sizeof(long) - 1), &data)) {
      return 0;
    }
    size_t copy_bytes = std::min(sizeof(long) - align_bytes, bytes);
    memcpy(dst, reinterpret_cast<uint8_t*>(&data) + align_bytes, copy_bytes);
    addr += copy_bytes;
    dst = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(dst) + copy_bytes);
    bytes -= copy_bytes;
    bytes_read += copy_bytes;
  }

  for (size_t i = 0; i < bytes / sizeof(long); i++) {
    if (!PtraceReadLong(pid, addr, &data)) {
      return bytes_read;
    }
    memcpy(dst, &data, sizeof(long));
    dst = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(dst) + sizeof(long));
    addr += sizeof(long);
    bytes_read += sizeof(long);
  }

  size_t left_over = bytes & (sizeof(long) - 1);
  if (left_over) {
    if (!PtraceReadLong(pid, addr, &data)) {
      return bytes_read;
    }
    memcpy(dst, &data, left_over);
    bytes_read += left_over;
  }
  return bytes_read;
}
