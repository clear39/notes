//	@art/runtime/mem_map.cc
void MemMap::Init() {
  if (mem_maps_lock_ != nullptr) {//	static std::mutex* mem_maps_lock_;
    // dex2oat calls MemMap::Init twice since its needed before the runtime is created.
    return;
  }
  mem_maps_lock_ = new std::mutex();
  // Not for thread safety, but for the annotation that gMaps is GUARDED_BY(mem_maps_lock_).
  std::lock_guard<std::mutex> mu(*mem_maps_lock_);
  DCHECK(gMaps == nullptr);

/**
art/runtime/mem_map.cc:55:using Maps = AllocationTrackingMultiMap<void*, MemMap*, kAllocatorTagMaps>;
art/runtime/mem_map.cc:58:static Maps* gMaps GUARDED_BY(MemMap::GetMemMapsLock()) = nullptr;
*/
  gMaps = new Maps;
}