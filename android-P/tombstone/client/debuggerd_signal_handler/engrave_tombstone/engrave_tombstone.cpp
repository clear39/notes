


//  @ system/core/debuggerd/libdebuggerd/tombstone.cpp

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


static void dump_header_info(log_t* log) {
  auto fingerprint = GetProperty("ro.build.fingerprint", "unknown");
  auto revision = GetProperty("ro.revision", "unknown");

  _LOG(log, logtype::HEADER, "Build fingerprint: '%s'\n", fingerprint.c_str());
  _LOG(log, logtype::HEADER, "Revision: '%s'\n", revision.c_str());
  _LOG(log, logtype::HEADER, "ABI: '%s'\n", ABI_STRING);
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
