
//	@	system/core/libbacktrace/include/backtrace/BacktraceMap.h

struct backtrace_map_t {
  uint64_t start = 0;
  uint64_t end = 0;
  uint64_t offset = 0;
  uint64_t load_bias = 0;
  int flags = 0;
  std::string name;

  // Returns `name` if non-empty, or `<anonymous:0x...>` otherwise.
  std::string Name() const;
};