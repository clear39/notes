

//	@	system/core/debuggerd/handler/debuggerd_fallback.cpp
static void debuggerd_fallback_tombstone(int output_fd, ucontext_t* ucontext, siginfo_t* siginfo, void* abort_message) {
  if (!__linker_enable_fallback_allocator()) {
    async_safe_format_log(ANDROID_LOG_ERROR, "libc", "fallback allocator already in use");
    return;
  }

  engrave_tombstone_ucontext(output_fd, reinterpret_cast<uintptr_t>(abort_message), siginfo, ucontext);

  __linker_disable_fallback_allocator();
}


void engrave_tombstone_ucontext(int tombstone_fd, uint64_t abort_msg_address, siginfo_t* siginfo, ucontext_t* ucontext) {
  pid_t pid = getpid();
  pid_t tid = gettid();

  log_t log;
  log.current_tid = tid;
  log.crashed_tid = tid;
  log.tfd = tombstone_fd;
  log.amfd_data = nullptr;

  char thread_name[16];
  char process_name[128];

  // 当前线程名,最后一个参数为默认值
  read_with_default("/proc/self/comm", thread_name, sizeof(thread_name), "<unknown>");
  // 当前进程名,最后一个参数为默认值
  read_with_default("/proc/self/cmdline", process_name, sizeof(process_name), "<unknown>");

  // 
  std::unique_ptr<Regs> regs(Regs::CreateFromUcontext(Regs::CurrentArch(), ucontext));

  std::map<pid_t, ThreadInfo> threads;
  threads[gettid()] = ThreadInfo{
      .registers = std::move(regs),
      .tid = tid,
      .thread_name = thread_name,
      .pid = pid,
      .process_name = process_name,
      .siginfo = siginfo,
  };

  std::unique_ptr<BacktraceMap> backtrace_map(BacktraceMap::Create(getpid(), false));
  if (!backtrace_map) {
    ALOGE("failed to create backtrace map");
    _exit(1);
  }

  std::shared_ptr<Memory> process_memory = backtrace_map->GetProcessMemory();

  // 
  engrave_tombstone(unique_fd(dup(tombstone_fd)), backtrace_map.get(), process_memory.get(),
                    threads, tid, abort_msg_address, nullptr, nullptr);
}


void engrave_tombstone(unique_fd output_fd, BacktraceMap* map, Memory* process_memory,
                       const std::map<pid_t, ThreadInfo>& threads, pid_t target_thread,
                       uint64_t abort_msg_address, OpenFilesList* open_files,
                       std::string* amfd_data) {
  // don't copy log messages to tombstone unless this is a dev device
  bool want_logs = android::base::GetBoolProperty("ro.debuggable", false);

  log_t log;
  log.current_tid = target_thread;
  log.crashed_tid = target_thread;
  log.tfd = output_fd.get();
  log.amfd_data = amfd_data;

  _LOG(&log, logtype::HEADER, "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");

  /*
  打印头信息

  */
  dump_header_info(&log);

  auto it = threads.find(target_thread);
  if (it == threads.end()) {
    LOG(FATAL) << "failed to find target thread";
  }
  dump_thread(&log, map, process_memory, it->second, abort_msg_address, true);

  if (want_logs) {
    dump_logs(&log, it->second.pid, 50);
  }

  for (auto& [tid, thread_info] : threads) {
    if (tid == target_thread) {
      continue;
    }

    dump_thread(&log, map, process_memory, thread_info, 0, false);
  }

  if (open_files) {
    _LOG(&log, logtype::OPEN_FILES, "\nopen files:\n");
    dump_open_files_list(&log, *open_files, "    ");
  }

  if (want_logs) {
    dump_logs(&log, it->second.pid, 0);
  }
}














static bool dump_thread(log_t* log, BacktraceMap* map, Memory* process_memory,
                        const ThreadInfo& thread_info, uint64_t abort_msg_address,
                        bool primary_thread) {
  UNUSED(process_memory);
  log->current_tid = thread_info.tid;
  if (!primary_thread) {
    _LOG(log, logtype::THREAD, "--- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---\n");
  }
  dump_thread_info(log, thread_info);

  if (thread_info.siginfo) {
    dump_signal_info(log, thread_info.siginfo);
  }

  if (primary_thread) {
    dump_abort_message(log, process_memory, abort_msg_address);
  }

  //	@	libdebuggerd/tombstone.cpp
  dump_registers(log, thread_info.registers.get());

  // Unwind will mutate the registers, so make a copy first.
  std::unique_ptr<Regs> regs_copy(thread_info.registers->Clone());
  std::vector<backtrace_frame_data_t> frames;
  if (!Backtrace::Unwind(regs_copy.get(), map, &frames, 0, nullptr)) {
    _LOG(log, logtype::THREAD, "Failed to unwind");
    return false;
  }

  if (!frames.empty()) {
    _LOG(log, logtype::BACKTRACE, "\nbacktrace:\n");
    dump_backtrace(log, frames, "    ");

    _LOG(log, logtype::STACK, "\nstack:\n");
    dump_stack(log, map, process_memory, frames);
  }

  if (primary_thread) {
    dump_memory_and_code(log, map, process_memory, thread_info.registers.get());
    if (map) {
      uint64_t addr = 0;
      siginfo_t* si = thread_info.siginfo;
      if (signal_has_si_addr(si->si_signo, si->si_code)) {
        addr = reinterpret_cast<uint64_t>(si->si_addr);
      }
      dump_all_maps(log, map, process_memory, addr);
    }
  }

  log->current_tid = log->crashed_tid;
  return true;
}

static void dump_thread_info(log_t* log, const ThreadInfo& thread_info) {
  // Blacklist logd, logd.reader, logd.writer, logd.auditd, logd.control ...
  // TODO: Why is this controlled by thread name?
  if (thread_info.thread_name == "logd" ||
      android::base::StartsWith(thread_info.thread_name, "logd.")) {
    log->should_retrieve_logcat = false;
  }

  _LOG(log, logtype::HEADER, "pid: %d, tid: %d, name: %s  >>> %s <<<\n", thread_info.pid,
       thread_info.tid, thread_info.thread_name.c_str(), thread_info.process_name.c_str());
}

static void dump_signal_info(log_t* log, const siginfo_t* si) {
  char addr_desc[32]; // ", fault addr 0x1234"
  if (signal_has_si_addr(si->si_signo, si->si_code)) {
    snprintf(addr_desc, sizeof(addr_desc), "%p", si->si_addr);
  } else {
    snprintf(addr_desc, sizeof(addr_desc), "--------");
  }

  _LOG(log, logtype::HEADER, "signal %d (%s), code %d (%s), fault addr %s\n", si->si_signo,
       get_signame(si->si_signo), si->si_code, get_sigcode(si->si_signo, si->si_code), addr_desc);

  dump_probable_cause(log, si);
}




void dump_registers(log_t* log, Regs* regs) {
  // Split lr/sp/pc into their own special row.
  static constexpr size_t column_count = 4;
  std::vector<std::pair<std::string, uint64_t>> current_row;
  std::vector<std::pair<std::string, uint64_t>> special_row;

#if defined(__arm__) || defined(__aarch64__)
  static constexpr const char* special_registers[] = {"ip", "lr", "sp", "pc"};
#elif defined(__i386__)
  static constexpr const char* special_registers[] = {"ebp", "esp", "eip"};
#elif defined(__x86_64__)
  static constexpr const char* special_registers[] = {"rbp", "rsp", "rip"};
#else
  static constexpr const char* special_registers[] = {};
#endif

  regs->IterateRegisters([log, &current_row, &special_row](const char* name, uint64_t value) {
    auto row = &current_row;
    for (const char* special_name : special_registers) {
      if (strcmp(special_name, name) == 0) {
        row = &special_row;
        break;
      }
    }

    row->emplace_back(name, value);
    if (current_row.size() == column_count) {
      print_register_row(log, current_row);
      current_row.clear();
    }
  });

  if (!current_row.empty()) {
    print_register_row(log, current_row);
  }

  print_register_row(log, special_row);
}

