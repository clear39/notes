

//  @ system/core/debuggerd/libdebuggerd/tombstone.cpp

static void dump_abort_message(log_t* log, Memory* process_memory, uint64_t address) {
  if (address == 0) {
    return;
  }

  // 读取长度
  size_t length;
  if (!process_memory->ReadFully(address, &length, sizeof(length))) {
    _LOG(log, logtype::HEADER, "Failed to read abort message header: %s\n", strerror(errno));
    return;
  }

  char msg[512];
  if (length >= sizeof(msg)) {
    _LOG(log, logtype::HEADER, "Abort message too long: claimed length = %zd\n", length);
    return;
  }

  if (!process_memory->ReadFully(address + sizeof(length), msg, length)) {
    _LOG(log, logtype::HEADER, "Failed to read abort message: %s\n", strerror(errno));
    return;
  }

  msg[length] = '\0';
  _LOG(log, logtype::HEADER, "Abort message: '%s'\n", msg);
}
