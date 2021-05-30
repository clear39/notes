


//	@	system/core/libbacktrace/UnwindStackMap.cpp

//-------------------------------------------------------------------------
// BacktraceMap create function.
//-------------------------------------------------------------------------
BacktraceMap* BacktraceMap::Create(pid_t pid, bool uncached) {
  BacktraceMap* map;

  if (uncached) {
    // Force use of the base class to parse the maps when this call is made.
    map = new BacktraceMap(pid);
  } else if (pid == getpid()) {
    map = new UnwindStackMap(0);
  } else {
    map = new UnwindStackMap(pid);
  }
  // 
  if (!map->Build()) {
    delete map;
    return nullptr;
  }
  return map;
}




UnwindStackMap::UnwindStackMap(pid_t pid) : BacktraceMap(pid) {}




bool UnwindStackMap::Build() {
  if (pid_ == 0) {
    pid_ = getpid();
    stack_maps_.reset(new unwindstack::LocalMaps);
  } else {
    stack_maps_.reset(new unwindstack::RemoteMaps(pid_));
  }

  // Create the process memory object.
  process_memory_ = unwindstack::Memory::CreateProcessMemory(pid_);

  // Create a JitDebug object for getting jit unwind information.
  std::vector<std::string> search_libs_{"libart.so", "libartd.so"};
  jit_debug_.reset(new unwindstack::JitDebug(process_memory_, search_libs_));
#if !defined(NO_LIBDEXFILE_SUPPORT)
  dex_files_.reset(new unwindstack::DexFiles(process_memory_, search_libs_));
#endif

  if (!stack_maps_->Parse()) {
    return false;
  }

  // Iterate through the maps and fill in the backtrace_map_t structure.
  for (auto* map_info : *stack_maps_) {
    backtrace_map_t map;
    map.start = map_info->start;
    map.end = map_info->end;
    map.offset = map_info->offset;
    // Set to -1 so that it is demand loaded.
    map.load_bias = static_cast<uint64_t>(-1);
    map.flags = map_info->flags;
    map.name = map_info->name;

    maps_.push_back(map);
  }

  return true;
}












bool Backtrace::Unwind(unwindstack::Regs* regs, BacktraceMap* back_map,
                       std::vector<backtrace_frame_data_t>* frames, size_t num_ignore_frames,
                       std::vector<std::string>* skip_names, BacktraceUnwindError* error) {
  UnwindStackMap* stack_map = reinterpret_cast<UnwindStackMap*>(back_map);
  auto process_memory = stack_map->process_memory();
  // 
  unwindstack::Unwinder unwinder(MAX_BACKTRACE_FRAMES + num_ignore_frames, stack_map->stack_maps(), regs, stack_map->process_memory());
  // 
  unwinder.SetResolveNames(stack_map->ResolveNames());
  if (stack_map->GetJitDebug() != nullptr) {
    unwinder.SetJitDebug(stack_map->GetJitDebug(), regs->Arch());
  }
#if !defined(NO_LIBDEXFILE_SUPPORT)
  if (stack_map->GetDexFiles() != nullptr) {
    unwinder.SetDexFiles(stack_map->GetDexFiles(), regs->Arch());
  }
#endif
  unwinder.Unwind(skip_names, &stack_map->GetSuffixesToIgnore());
  if (error != nullptr) {
    switch (unwinder.LastErrorCode()) {
      case unwindstack::ERROR_NONE:
        error->error_code = BACKTRACE_UNWIND_NO_ERROR;
        break;

      case unwindstack::ERROR_MEMORY_INVALID:
        error->error_code = BACKTRACE_UNWIND_ERROR_ACCESS_MEM_FAILED;
        error->error_info.addr = unwinder.LastErrorAddress();
        break;

      case unwindstack::ERROR_UNWIND_INFO:
        error->error_code = BACKTRACE_UNWIND_ERROR_UNWIND_INFO;
        break;

      case unwindstack::ERROR_UNSUPPORTED:
        error->error_code = BACKTRACE_UNWIND_ERROR_UNSUPPORTED_OPERATION;
        break;

      case unwindstack::ERROR_INVALID_MAP:
        error->error_code = BACKTRACE_UNWIND_ERROR_MAP_MISSING;
        break;

      case unwindstack::ERROR_MAX_FRAMES_EXCEEDED:
        error->error_code = BACKTRACE_UNWIND_ERROR_EXCEED_MAX_FRAMES_LIMIT;
        break;

      case unwindstack::ERROR_REPEATED_FRAME:
        error->error_code = BACKTRACE_UNWIND_ERROR_REPEATED_FRAME;
        break;
    }
  }

  if (num_ignore_frames >= unwinder.NumFrames()) {
    frames->resize(0);
    return true;
  }

  auto unwinder_frames = unwinder.frames();
  frames->resize(unwinder.NumFrames() - num_ignore_frames);
  size_t cur_frame = 0;
  for (size_t i = num_ignore_frames; i < unwinder.NumFrames(); i++) {
    auto frame = &unwinder_frames[i];

    backtrace_frame_data_t* back_frame = &frames->at(cur_frame);

    back_frame->num = cur_frame++;

    back_frame->rel_pc = frame->rel_pc;
    back_frame->pc = frame->pc;
    back_frame->sp = frame->sp;

    back_frame->func_name = demangle(frame->function_name.c_str());
    back_frame->func_offset = frame->function_offset;

    back_frame->map.name = frame->map_name;
    back_frame->map.start = frame->map_start;
    back_frame->map.end = frame->map_end;
    back_frame->map.offset = frame->map_offset;
    back_frame->map.load_bias = frame->map_load_bias;
    back_frame->map.flags = frame->map_flags;
  }

  return true;
}
