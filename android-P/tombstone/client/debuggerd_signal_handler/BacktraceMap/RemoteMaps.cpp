
//  @ system/core/libunwindstack/include/unwindstack/Maps.h

class RemoteMaps : public Maps {
 public:
  RemoteMaps(pid_t pid) : pid_(pid) {}
  virtual ~RemoteMaps() = default;

  virtual const std::string GetMapsFile() const override;

 private:
  pid_t pid_;
};


const std::string RemoteMaps::GetMapsFile() const {
  return "/proc/" + std::to_string(pid_) + "/maps";
}


bool Maps::Parse() {
  int fd = open(GetMapsFile().c_str(), O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    return false;
  }

  bool return_value = true;
  char buffer[2048];
  size_t leftover = 0;
  while (true) {
    ssize_t bytes = read(fd, &buffer[leftover], 2048 - leftover);
    if (bytes == -1) {
      return_value = false;
      break;
    }
    if (bytes == 0) {
      break;
    }
    bytes += leftover;
    char* line = buffer;
    while (bytes > 0) {
      char* newline = static_cast<char*>(memchr(line, '\n', bytes));
      if (newline == nullptr) {
        memmove(buffer, line, bytes);
        break;
      }
      *newline = '\0';

      MapInfo* map_info = InternalParseLine(line);
      if (map_info == nullptr) {
        return_value = false;
        break;
      }
      maps_.push_back(map_info);

      bytes -= newline - line + 1;
      line = newline + 1;
    }
    leftover = bytes;
  }
  close(fd);
  return return_value;
}
