//	@art/runtime/gc/heap.cc
Heap::Heap(size_t initial_size,
           size_t growth_limit,
           size_t min_free,
           size_t max_free,
           double target_utilization,
           double foreground_heap_growth_multiplier,
           size_t capacity,
           size_t non_moving_space_capacity,
           const std::string& image_file_name,
           const InstructionSet image_instruction_set,
           CollectorType foreground_collector_type,
           CollectorType background_collector_type,
           space::LargeObjectSpaceType large_object_space_type,
           size_t large_object_threshold,
           size_t parallel_gc_threads,
           size_t conc_gc_threads,
           bool low_memory_mode,
           size_t long_pause_log_threshold,
           size_t long_gc_log_threshold,
           bool ignore_max_footprint,
           bool use_tlab,
           bool verify_pre_gc_heap,
           bool verify_pre_sweeping_heap,
           bool verify_post_gc_heap,
           bool verify_pre_gc_rosalloc,
           bool verify_pre_sweeping_rosalloc,
           bool verify_post_gc_rosalloc,
           bool gc_stress_mode,
           bool measure_gc_performance,
           bool use_homogeneous_space_compaction_for_oom,
           uint64_t min_interval_homogeneous_space_compaction_by_oom)
    : non_moving_space_(nullptr),
      rosalloc_space_(nullptr),
      dlmalloc_space_(nullptr),
      main_space_(nullptr),
      collector_type_(kCollectorTypeNone),
      foreground_collector_type_(foreground_collector_type),
      background_collector_type_(background_collector_type),
      desired_collector_type_(foreground_collector_type_),
      pending_task_lock_(nullptr),
      parallel_gc_threads_(parallel_gc_threads),
      conc_gc_threads_(conc_gc_threads),
      low_memory_mode_(low_memory_mode),
      long_pause_log_threshold_(long_pause_log_threshold),
      long_gc_log_threshold_(long_gc_log_threshold),
      ignore_max_footprint_(ignore_max_footprint),
      zygote_creation_lock_("zygote creation lock", kZygoteCreationLock),
      zygote_space_(nullptr),
      large_object_threshold_(large_object_threshold),
      disable_thread_flip_count_(0),
      thread_flip_running_(false),
      collector_type_running_(kCollectorTypeNone),
      last_gc_cause_(kGcCauseNone),
      thread_running_gc_(nullptr),
      last_gc_type_(collector::kGcTypeNone),
      next_gc_type_(collector::kGcTypePartial),
      capacity_(capacity),
      growth_limit_(growth_limit),
      max_allowed_footprint_(initial_size),
      concurrent_start_bytes_(std::numeric_limits<size_t>::max()),
      total_bytes_freed_ever_(0),
      total_objects_freed_ever_(0),
      num_bytes_allocated_(0),
      new_native_bytes_allocated_(0),
      old_native_bytes_allocated_(0),
      num_bytes_freed_revoke_(0),
      verify_missing_card_marks_(false),
      verify_system_weaks_(false),
      verify_pre_gc_heap_(verify_pre_gc_heap),
      verify_pre_sweeping_heap_(verify_pre_sweeping_heap),
      verify_post_gc_heap_(verify_post_gc_heap),
      verify_mod_union_table_(false),
      verify_pre_gc_rosalloc_(verify_pre_gc_rosalloc),
      verify_pre_sweeping_rosalloc_(verify_pre_sweeping_rosalloc),
      verify_post_gc_rosalloc_(verify_post_gc_rosalloc),
      gc_stress_mode_(gc_stress_mode),
      /* For GC a lot mode, we limit the allocations stacks to be kGcAlotInterval allocations. This
       * causes a lot of GC since we do a GC for alloc whenever the stack is full. When heap
       * verification is enabled, we limit the size of allocation stacks to speed up their
       * searching.
       */
      max_allocation_stack_size_(kGCALotMode ? kGcAlotAllocationStackSize : (kVerifyObjectSupport > kVerifyObjectModeFast) ? kVerifyObjectAllocationStackSize : kDefaultAllocationStackSize),
      current_allocator_(kAllocatorTypeDlMalloc),
      current_non_moving_allocator_(kAllocatorTypeNonMoving),
      bump_pointer_space_(nullptr),
      temp_space_(nullptr),
      region_space_(nullptr),
      min_free_(min_free),
      max_free_(max_free),
      target_utilization_(target_utilization),
      foreground_heap_growth_multiplier_(foreground_heap_growth_multiplier),
      total_wait_time_(0),
      verify_object_mode_(kVerifyObjectModeDisabled),
      disable_moving_gc_count_(0),
      semi_space_collector_(nullptr),
      mark_compact_collector_(nullptr),
      concurrent_copying_collector_(nullptr),
      is_running_on_memory_tool_(Runtime::Current()->IsRunningOnMemoryTool()),
      use_tlab_(use_tlab),
      main_space_backup_(nullptr),
      min_interval_homogeneous_space_compaction_by_oom_(min_interval_homogeneous_space_compaction_by_oom),
      last_time_homogeneous_space_compaction_by_oom_(NanoTime()),
      pending_collector_transition_(nullptr),
      pending_heap_trim_(nullptr),
      use_homogeneous_space_compaction_for_oom_(use_homogeneous_space_compaction_for_oom),
      running_collection_is_blocking_(false),
      blocking_gc_count_(0U),
      blocking_gc_time_(0U),
      last_update_time_gc_count_rate_histograms_( (NanoTime() / kGcCountRateHistogramWindowDuration) * kGcCountRateHistogramWindowDuration), // Round down by the window duration.
      gc_count_last_window_(0U),
      blocking_gc_count_last_window_(0U),
      gc_count_rate_histogram_("gc count rate histogram", 1U, kGcCountRateMaxBucketCount),
      blocking_gc_count_rate_histogram_("blocking gc count rate histogram", 1U,kGcCountRateMaxBucketCount),
      alloc_tracking_enabled_(false),
      backtrace_lock_(nullptr),
      seen_backtrace_count_(0u),
      unique_backtrace_count_(0u),
      gc_disabled_for_shutdown_(false) {


  if (VLOG_IS_ON(heap) || VLOG_IS_ON(startup)) {
    LOG(INFO) << "Heap() entering";
  }
  if (kUseReadBarrier) {
    CHECK_EQ(foreground_collector_type_, kCollectorTypeCC);
    CHECK_EQ(background_collector_type_, kCollectorTypeCCBackground);
  }
  verification_.reset(new Verification(this));
  CHECK_GE(large_object_threshold, kMinLargeObjectThreshold);
  ScopedTrace trace(__FUNCTION__);
  Runtime* const runtime = Runtime::Current();
  // If we aren't the zygote, switch to the default non zygote allocator. This may update the
  // entrypoints.
  const bool is_zygote = runtime->IsZygote();
  if (!is_zygote) {
    // Background compaction is currently not supported for command line runs.
    if (background_collector_type_ != foreground_collector_type_) {
      VLOG(heap) << "Disabling background compaction for non zygote";
      background_collector_type_ = foreground_collector_type_;
    }
  }
  ChangeCollector(desired_collector_type_);
  live_bitmap_.reset(new accounting::HeapBitmap(this));
  mark_bitmap_.reset(new accounting::HeapBitmap(this));
  // Requested begin for the alloc space, to follow the mapped image and oat files
  uint8_t* requested_alloc_space_begin = nullptr;
  if (foreground_collector_type_ == kCollectorTypeCC) {
    // Need to use a low address so that we can allocate a contiguous 2 * Xmx space when there's no
    // image (dex2oat for target).
    requested_alloc_space_begin = kPreferredAllocSpaceBegin;
  }

  // Load image space(s).
  if (space::ImageSpace::LoadBootImage(image_file_name,image_instruction_set,&boot_image_spaces_,&requested_alloc_space_begin)) {
    for (auto space : boot_image_spaces_) {
      AddSpace(space);
    }
  }

  /*
  requested_alloc_space_begin ->     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
                                     +-  nonmoving space (non_moving_space_capacity)+-
                                     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
                                     +-????????????????????????????????????????????+-
                                     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
                                     +-main alloc space / bump space 1 (capacity_) +-
                                     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
                                     +-????????????????????????????????????????????+-
                                     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
                                     +-main alloc space2 / bump space 2 (capacity_)+-
                                     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
  */
  // We don't have hspace compaction enabled with GSS or CC.
  if (foreground_collector_type_ == kCollectorTypeGSS ||
      foreground_collector_type_ == kCollectorTypeCC) {
    use_homogeneous_space_compaction_for_oom_ = false;
  }
  bool support_homogeneous_space_compaction =
      background_collector_type_ == gc::kCollectorTypeHomogeneousSpaceCompact ||
      use_homogeneous_space_compaction_for_oom_;
  // We may use the same space the main space for the non moving space if we don't need to compact
  // from the main space.
  // This is not the case if we support homogeneous compaction or have a moving background
  // collector type.
  bool separate_non_moving_space = is_zygote ||
      support_homogeneous_space_compaction || IsMovingGc(foreground_collector_type_) ||
      IsMovingGc(background_collector_type_);
  if (foreground_collector_type_ == kCollectorTypeGSS) {
    separate_non_moving_space = false;
  }
  std::unique_ptr<MemMap> main_mem_map_1;
  std::unique_ptr<MemMap> main_mem_map_2;

  // Gross hack to make dex2oat deterministic.
  if (foreground_collector_type_ == kCollectorTypeMS &&
      requested_alloc_space_begin == nullptr &&
      Runtime::Current()->IsAotCompiler()) {
    // Currently only enabled for MS collector since that is what the deterministic dex2oat uses.
    // b/26849108
    requested_alloc_space_begin = reinterpret_cast<uint8_t*>(kAllocSpaceBeginForDeterministicAoT);
  }
  uint8_t* request_begin = requested_alloc_space_begin;
  if (request_begin != nullptr && separate_non_moving_space) {
    request_begin += non_moving_space_capacity;
  }
  std::string error_str;
  std::unique_ptr<MemMap> non_moving_space_mem_map;
  if (separate_non_moving_space) {
    ScopedTrace trace2("Create separate non moving space");
    // If we are the zygote, the non moving space becomes the zygote space when we run
    // PreZygoteFork the first time. In this case, call the map "zygote space" since we can't
    // rename the mem map later.
    const char* space_name = is_zygote ? kZygoteSpaceName : kNonMovingSpaceName;
    // Reserve the non moving mem map before the other two since it needs to be at a specific
    // address.
    non_moving_space_mem_map.reset(
        MemMap::MapAnonymous(space_name, requested_alloc_space_begin,
                             non_moving_space_capacity, PROT_READ | PROT_WRITE, true, false,
                             &error_str));
    CHECK(non_moving_space_mem_map != nullptr) << error_str;
    // Try to reserve virtual memory at a lower address if we have a separate non moving space.
    request_begin = kPreferredAllocSpaceBegin + non_moving_space_capacity;
  }
  // Attempt to create 2 mem maps at or after the requested begin.
  if (foreground_collector_type_ != kCollectorTypeCC) {
    ScopedTrace trace2("Create main mem map");
    if (separate_non_moving_space || !is_zygote) {
      main_mem_map_1.reset(MapAnonymousPreferredAddress(kMemMapSpaceName[0],
                                                        request_begin,
                                                        capacity_,
                                                        &error_str));
    } else {
      // If no separate non-moving space and we are the zygote, the main space must come right
      // after the image space to avoid a gap. This is required since we want the zygote space to
      // be adjacent to the image space.
      main_mem_map_1.reset(MemMap::MapAnonymous(kMemMapSpaceName[0], request_begin, capacity_,
                                                PROT_READ | PROT_WRITE, true, false,
                                                &error_str));
    }
    CHECK(main_mem_map_1.get() != nullptr) << error_str;
  }
  if (support_homogeneous_space_compaction ||
      background_collector_type_ == kCollectorTypeSS ||
      foreground_collector_type_ == kCollectorTypeSS) {
    ScopedTrace trace2("Create main mem map 2");
    main_mem_map_2.reset(MapAnonymousPreferredAddress(kMemMapSpaceName[1], main_mem_map_1->End(),
                                                      capacity_, &error_str));
    CHECK(main_mem_map_2.get() != nullptr) << error_str;
  }

  // Create the non moving space first so that bitmaps don't take up the address range.
  if (separate_non_moving_space) {
    ScopedTrace trace2("Add non moving space");
    // Non moving space is always dlmalloc since we currently don't have support for multiple
    // active rosalloc spaces.
    const size_t size = non_moving_space_mem_map->Size();
    non_moving_space_ = space::DlMallocSpace::CreateFromMemMap(
        non_moving_space_mem_map.release(), "zygote / non moving space", kDefaultStartingSize,
        initial_size, size, size, false);
    non_moving_space_->SetFootprintLimit(non_moving_space_->Capacity());
    CHECK(non_moving_space_ != nullptr) << "Failed creating non moving space "
        << requested_alloc_space_begin;
    AddSpace(non_moving_space_);
  }
  // Create other spaces based on whether or not we have a moving GC.
  if (foreground_collector_type_ == kCollectorTypeCC) {
    CHECK(separate_non_moving_space);
    MemMap* region_space_mem_map = space::RegionSpace::CreateMemMap(kRegionSpaceName,
                                                                    capacity_ * 2,
                                                                    request_begin);
    CHECK(region_space_mem_map != nullptr) << "No region space mem map";
    region_space_ = space::RegionSpace::Create(kRegionSpaceName, region_space_mem_map);
    AddSpace(region_space_);
  } else if (IsMovingGc(foreground_collector_type_) &&
      foreground_collector_type_ != kCollectorTypeGSS) {
    // Create bump pointer spaces.
    // We only to create the bump pointer if the foreground collector is a compacting GC.
    // TODO: Place bump-pointer spaces somewhere to minimize size of card table.
    bump_pointer_space_ = space::BumpPointerSpace::CreateFromMemMap("Bump pointer space 1",
                                                                    main_mem_map_1.release());
    CHECK(bump_pointer_space_ != nullptr) << "Failed to create bump pointer space";
    AddSpace(bump_pointer_space_);
    temp_space_ = space::BumpPointerSpace::CreateFromMemMap("Bump pointer space 2",
                                                            main_mem_map_2.release());
    CHECK(temp_space_ != nullptr) << "Failed to create bump pointer space";
    AddSpace(temp_space_);
    CHECK(separate_non_moving_space);
  } else {
    CreateMainMallocSpace(main_mem_map_1.release(), initial_size, growth_limit_, capacity_);
    CHECK(main_space_ != nullptr);
    AddSpace(main_space_);
    if (!separate_non_moving_space) {
      non_moving_space_ = main_space_;
      CHECK(!non_moving_space_->CanMoveObjects());
    }
    if (foreground_collector_type_ == kCollectorTypeGSS) {
      CHECK_EQ(foreground_collector_type_, background_collector_type_);
      // Create bump pointer spaces instead of a backup space.
      main_mem_map_2.release();
      bump_pointer_space_ = space::BumpPointerSpace::Create("Bump pointer space 1",
                                                            kGSSBumpPointerSpaceCapacity, nullptr);
      CHECK(bump_pointer_space_ != nullptr);
      AddSpace(bump_pointer_space_);
      temp_space_ = space::BumpPointerSpace::Create("Bump pointer space 2",
                                                    kGSSBumpPointerSpaceCapacity, nullptr);
      CHECK(temp_space_ != nullptr);
      AddSpace(temp_space_);
    } else if (main_mem_map_2.get() != nullptr) {
      const char* name = kUseRosAlloc ? kRosAllocSpaceName[1] : kDlMallocSpaceName[1];
      main_space_backup_.reset(CreateMallocSpaceFromMemMap(main_mem_map_2.release(), initial_size,
                                                           growth_limit_, capacity_, name, true));
      CHECK(main_space_backup_.get() != nullptr);
      // Add the space so its accounted for in the heap_begin and heap_end.
      AddSpace(main_space_backup_.get());
    }
  }
  CHECK(non_moving_space_ != nullptr);
  CHECK(!non_moving_space_->CanMoveObjects());
  // Allocate the large object space.
  if (large_object_space_type == space::LargeObjectSpaceType::kFreeList) {
    large_object_space_ = space::FreeListSpace::Create("free list large object space", nullptr,
                                                       capacity_);
    CHECK(large_object_space_ != nullptr) << "Failed to create large object space";
  } else if (large_object_space_type == space::LargeObjectSpaceType::kMap) {
    large_object_space_ = space::LargeObjectMapSpace::Create("mem map large object space");
    CHECK(large_object_space_ != nullptr) << "Failed to create large object space";
  } else {
    // Disable the large object space by making the cutoff excessively large.
    large_object_threshold_ = std::numeric_limits<size_t>::max();
    large_object_space_ = nullptr;
  }
  if (large_object_space_ != nullptr) {
    AddSpace(large_object_space_);
  }
  // Compute heap capacity. Continuous spaces are sorted in order of Begin().
  CHECK(!continuous_spaces_.empty());
  // Relies on the spaces being sorted.
  uint8_t* heap_begin = continuous_spaces_.front()->Begin();
  uint8_t* heap_end = continuous_spaces_.back()->Limit();
  size_t heap_capacity = heap_end - heap_begin;
  // Remove the main backup space since it slows down the GC to have unused extra spaces.
  // TODO: Avoid needing to do this.
  if (main_space_backup_.get() != nullptr) {
    RemoveSpace(main_space_backup_.get());
  }
  // Allocate the card table.
  // We currently don't support dynamically resizing the card table.
  // Since we don't know where in the low_4gb the app image will be located, make the card table
  // cover the whole low_4gb. TODO: Extend the card table in AddSpace.
  UNUSED(heap_capacity);
  // Start at 64 KB, we can be sure there are no spaces mapped this low since the address range is
  // reserved by the kernel.
  static constexpr size_t kMinHeapAddress = 4 * KB;
  card_table_.reset(accounting::CardTable::Create(reinterpret_cast<uint8_t*>(kMinHeapAddress),
                                                  4 * GB - kMinHeapAddress));
  CHECK(card_table_.get() != nullptr) << "Failed to create card table";
  if (foreground_collector_type_ == kCollectorTypeCC && kUseTableLookupReadBarrier) {
    rb_table_.reset(new accounting::ReadBarrierTable());
    DCHECK(rb_table_->IsAllCleared());
  }
  if (HasBootImageSpace()) {
    // Don't add the image mod union table if we are running without an image, this can crash if
    // we use the CardCache implementation.
    for (space::ImageSpace* image_space : GetBootImageSpaces()) {
      accounting::ModUnionTable* mod_union_table = new accounting::ModUnionTableToZygoteAllocspace(
          "Image mod-union table", this, image_space);
      CHECK(mod_union_table != nullptr) << "Failed to create image mod-union table";
      AddModUnionTable(mod_union_table);
    }
  }
  if (collector::SemiSpace::kUseRememberedSet && non_moving_space_ != main_space_) {
    accounting::RememberedSet* non_moving_space_rem_set =
        new accounting::RememberedSet("Non-moving space remembered set", this, non_moving_space_);
    CHECK(non_moving_space_rem_set != nullptr) << "Failed to create non-moving space remembered set";
    AddRememberedSet(non_moving_space_rem_set);
  }
  // TODO: Count objects in the image space here?
  num_bytes_allocated_.StoreRelaxed(0);
  mark_stack_.reset(accounting::ObjectStack::Create("mark stack", kDefaultMarkStackSize,
                                                    kDefaultMarkStackSize));
  const size_t alloc_stack_capacity = max_allocation_stack_size_ + kAllocationStackReserveSize;
  allocation_stack_.reset(accounting::ObjectStack::Create(
      "allocation stack", max_allocation_stack_size_, alloc_stack_capacity));
  live_stack_.reset(accounting::ObjectStack::Create(
      "live stack", max_allocation_stack_size_, alloc_stack_capacity));
  // It's still too early to take a lock because there are no threads yet, but we can create locks
  // now. We don't create it earlier to make it clear that you can't use locks during heap
  // initialization.
  gc_complete_lock_ = new Mutex("GC complete lock");
  gc_complete_cond_.reset(new ConditionVariable("GC complete condition variable",
                                                *gc_complete_lock_));
  native_blocking_gc_lock_ = new Mutex("Native blocking GC lock");
  native_blocking_gc_cond_.reset(new ConditionVariable("Native blocking GC condition variable",
                                                       *native_blocking_gc_lock_));
  native_blocking_gc_is_assigned_ = false;
  native_blocking_gc_in_progress_ = false;
  native_blocking_gcs_finished_ = 0;

  thread_flip_lock_ = new Mutex("GC thread flip lock");
  thread_flip_cond_.reset(new ConditionVariable("GC thread flip condition variable",
                                                *thread_flip_lock_));
  task_processor_.reset(new TaskProcessor());
  reference_processor_.reset(new ReferenceProcessor());
  pending_task_lock_ = new Mutex("Pending task lock");
  if (ignore_max_footprint_) {
    SetIdealFootprint(std::numeric_limits<size_t>::max());
    concurrent_start_bytes_ = std::numeric_limits<size_t>::max();
  }
  CHECK_NE(max_allowed_footprint_, 0U);
  // Create our garbage collectors.
  for (size_t i = 0; i < 2; ++i) {
    const bool concurrent = i != 0;
    if ((MayUseCollector(kCollectorTypeCMS) && concurrent) ||
        (MayUseCollector(kCollectorTypeMS) && !concurrent)) {
      garbage_collectors_.push_back(new collector::MarkSweep(this, concurrent));
      garbage_collectors_.push_back(new collector::PartialMarkSweep(this, concurrent));
      garbage_collectors_.push_back(new collector::StickyMarkSweep(this, concurrent));
    }
  }
  if (kMovingCollector) {
    if (MayUseCollector(kCollectorTypeSS) || MayUseCollector(kCollectorTypeGSS) ||
        MayUseCollector(kCollectorTypeHomogeneousSpaceCompact) ||
        use_homogeneous_space_compaction_for_oom_) {
      // TODO: Clean this up.
      const bool generational = foreground_collector_type_ == kCollectorTypeGSS;
      semi_space_collector_ = new collector::SemiSpace(this, generational,
                                                       generational ? "generational" : "");
      garbage_collectors_.push_back(semi_space_collector_);
    }
    if (MayUseCollector(kCollectorTypeCC)) {
      concurrent_copying_collector_ = new collector::ConcurrentCopying(this,
                                                                       "",
                                                                       measure_gc_performance);
      DCHECK(region_space_ != nullptr);
      concurrent_copying_collector_->SetRegionSpace(region_space_);
      garbage_collectors_.push_back(concurrent_copying_collector_);
    }
    if (MayUseCollector(kCollectorTypeMC)) {
      mark_compact_collector_ = new collector::MarkCompact(this);
      garbage_collectors_.push_back(mark_compact_collector_);
    }
  }
  if (!GetBootImageSpaces().empty() && non_moving_space_ != nullptr &&
      (is_zygote || separate_non_moving_space || foreground_collector_type_ == kCollectorTypeGSS)) {
    // Check that there's no gap between the image space and the non moving space so that the
    // immune region won't break (eg. due to a large object allocated in the gap). This is only
    // required when we're the zygote or using GSS.
    // Space with smallest Begin().
    space::ImageSpace* first_space = nullptr;
    for (space::ImageSpace* space : boot_image_spaces_) {
      if (first_space == nullptr || space->Begin() < first_space->Begin()) {
        first_space = space;
      }
    }
    bool no_gap = MemMap::CheckNoGaps(first_space->GetMemMap(), non_moving_space_->GetMemMap());
    if (!no_gap) {
      PrintFileToLog("/proc/self/maps", LogSeverity::ERROR);
      MemMap::DumpMaps(LOG_STREAM(ERROR), true);
      LOG(FATAL) << "There's a gap between the image space and the non-moving space";
    }
  }
  instrumentation::Instrumentation* const instrumentation = runtime->GetInstrumentation();
  if (gc_stress_mode_) {
    backtrace_lock_ = new Mutex("GC complete lock");
  }
  if (is_running_on_memory_tool_ || gc_stress_mode_) {
    instrumentation->InstrumentQuickAllocEntryPoints();
  }
  if (VLOG_IS_ON(heap) || VLOG_IS_ON(startup)) {
    LOG(INFO) << "Heap() exiting";
  }
}