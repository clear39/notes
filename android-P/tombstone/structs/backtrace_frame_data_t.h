


//	@	system/core/libbacktrace/include/backtrace/Backtrace.h

struct backtrace_frame_data_t {
  size_t num;             // The current fame number.
  uint64_t pc;            // The absolute pc.
  uint64_t rel_pc;        // The relative pc.
  uint64_t sp;            // The top of the stack.
  size_t stack_size;      // The size of the stack, zero indicate an unknown stack size.
  backtrace_map_t map;    // The map associated with the given pc.
  std::string func_name;  // The function name associated with this pc, NULL if not found.
  uint64_t func_offset;  // pc relative to the start of the function, only valid if func_name is not
                         // NULL.
};