//该文档主要针对c层日志打印

//	@system/core/base/include/android-base/logging.h
enum LogSeverity {
  VERBOSE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL_WITHOUT_ABORT,
  FATAL,
};

enum LogId {
  DEFAULT,
  MAIN,
  SYSTEM,
};


// Logs a message to logcat on Android otherwise to stderr. If the severity is
// FATAL it also causes an abort. For example:
//
//     LOG(FATAL) << "We didn't expect to reach here";
#define LOG(severity) LOG_TO(DEFAULT, severity)

// Logs a message to logcat with the specified log ID on Android otherwise to
// stderr. If the severity is FATAL it also causes an abort(如果severity为FATAL，导致异常退出).
// Use an expression here so we can support the << operator following the macro,
// like "LOG(DEBUG) << xxx;".
#define LOG_TO(dest, severity) LOGGING_PREAMBLE(severity) && LOG_STREAM_TO(dest, severity)



// Checks if we want to log something, and sets up appropriate RAII objects if
// so.
// Note: DO NOT USE DIRECTLY. This is an implementation detail.
#define LOGGING_PREAMBLE(severity)                                                         \
  (WOULD_LOG(severity) &&                                                                  \
   ABORT_AFTER_LOG_EXPR_IF((SEVERITY_LAMBDA(severity)) == ::android::base::FATAL, true) && \
   ::android::base::ErrnoRestorer())

// Defines whether the given severity will be logged or silently swallowed.
#define WOULD_LOG(severity) \
  (UNLIKELY((SEVERITY_LAMBDA(severity)) >= ::android::base::GetMinimumLogSeverity()) || \
   MUST_LOG_MESSAGE(severity))


// A helper macro that produces an expression that accepts both a qualified(合格的) name and an
// unqualified(不合格的) name for a LogSeverity, and returns a LogSeverity value.
// Note: DO NOT USE DIRECTLY. This is an implementation detail.
// 这里为c++ lambda表达式
#define SEVERITY_LAMBDA(severity) ([&]() {    \
  using ::android::base::VERBOSE;             \
  using ::android::base::DEBUG;               \
  using ::android::base::INFO;                \
  using ::android::base::WARNING;             \
  using ::android::base::ERROR;               \
  using ::android::base::FATAL_WITHOUT_ABORT; \
  using ::android::base::FATAL;               \
  return (severity); }())





//	@system/core/base/logging.cpp
static bool gInitialized = false;
static LogSeverity gMinimumLogSeverity = INFO;
LogSeverity GetMinimumLogSeverity() {
    return gMinimumLogSeverity;
}


// 将字符串写入一个输出流中，在析构函数中再次写入设备

// Get an ostream that can be used for logging at the given severity and to the
// given destination. The same notes as for LOG_STREAM apply.
#define LOG_STREAM_TO(dest, severity)                                   \
  ::android::base::LogMessage(__FILE__, __LINE__,::android::base::dest,SEVERITY_LAMBDA(severity), -1).stream()

LogMessage::LogMessage(const char* file, unsigned int line, LogId id,LogSeverity severity, int error)
	//	const std::unique_ptr<LogMessageData> data_;
    : data_(new LogMessageData(file, line, id, severity, error)) {
}

LogMessageData(const char* file, unsigned int line, LogId id,LogSeverity severity, int error)
	//	const char* const file_;
  : file_(GetFileBasename(file)),line_number_(line),id_(id),severity_(severity),error_(error) {
}

static const char* LogMessageData::GetFileBasename(const char* file) {
  // We can't use basename(3) even on Unix because the Mac doesn't
  // have a non-modifying basename.
  const char* last_slash = strrchr(file, '/');
  if (last_slash != nullptr) {
    return last_slash + 1;
  }
#if defined(_WIN32)
  const char* last_backslash = strrchr(file, '\\');
  if (last_backslash != nullptr) {
    return last_backslash + 1;
  }
#endif
  return file;
}



std::ostream& LogMessage::stream() {
  return data_->GetBuffer();
}


std::ostream& LogMessageData::GetBuffer() {
  return buffer_;
}


LogMessage::~LogMessage() {
  // Check severity again. This is duplicate work wrt/ LOG macros, but not LOG_STREAM.
  if (!WOULD_LOG(data_->GetSeverity())) {
    return;
  }

  // Finish constructing the message.
  if (data_->GetError() != -1) {
    data_->GetBuffer() << ": " << strerror(data_->GetError());
  }
  std::string msg(data_->ToString());

  {
    // Do the actual logging with the lock held.
    std::lock_guard<std::mutex> lock(LoggingLock());
    if (msg.find('\n') == std::string::npos) {
      LogLine(data_->GetFile(), data_->GetLineNumber(), data_->GetId(),data_->GetSeverity(), msg.c_str());
    } else {
      msg += '\n';
      size_t i = 0;
      while (i < msg.size()) {
        size_t nl = msg.find('\n', i);
        msg[nl] = '\0';
        LogLine(data_->GetFile(), data_->GetLineNumber(), data_->GetId(),data_->GetSeverity(), &msg[i]);
        // Undo the zero-termination so we can give the complete message to the aborter.
        msg[nl] = '\n';
        i = nl + 1;
      }
    }
  }

  // Abort if necessary.
  if (data_->GetSeverity() == FATAL) {
    Aborter()(msg.c_str());
  }
}












//日志初始化

//	@system/core/init/log.cpp	//该函数在init中调用
void InitKernelLogging(char* argv[]) {
    // Make stdin/stdout/stderr all point to /dev/null.
    int fd = open("/sys/fs/selinux/null", O_RDWR);
    if (fd == -1) {
        int saved_errno = errno;
        android::base::InitLogging(argv, &android::base::KernelLogger);
        errno = saved_errno;
        PLOG(FATAL) << "Couldn't open /sys/fs/selinux/null";
    }
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2) close(fd);

    android::base::InitLogging(argv, &android::base::KernelLogger);
}

void KernelLogger(android::base::LogId, android::base::LogSeverity severity,const char* tag, const char*, unsigned int, const char* msg) {
  // clang-format off
  static constexpr int kLogSeverityToKernelLogLevel[] = {
      [android::base::VERBOSE] = 7,              // KERN_DEBUG (there is no verbose kernel log
                                                 //             level)
      [android::base::DEBUG] = 7,                // KERN_DEBUG
      [android::base::INFO] = 6,                 // KERN_INFO
      [android::base::WARNING] = 4,              // KERN_WARNING
      [android::base::ERROR] = 3,                // KERN_ERROR
      [android::base::FATAL_WITHOUT_ABORT] = 2,  // KERN_CRIT
      [android::base::FATAL] = 2,                // KERN_CRIT
  };
  // clang-format on
  static_assert(arraysize(kLogSeverityToKernelLogLevel) == android::base::FATAL + 1,"Mismatch in size of kLogSeverityToKernelLogLevel and values in LogSeverity");

  static int klog_fd = TEMP_FAILURE_RETRY(open("/dev/kmsg", O_WRONLY | O_CLOEXEC));
  if (klog_fd == -1) return;

  int level = kLogSeverityToKernelLogLevel[severity];

  // The kernel's printk buffer is only 1024 bytes.
  // TODO: should we automatically break up long lines into multiple lines?
  // Or we could log but with something like "..." at the end?
  char buf[1024];
  size_t size = snprintf(buf, sizeof(buf), "<%d>%s: %s\n", level, tag, msg);
  if (size > sizeof(buf)) {
    size = snprintf(buf, sizeof(buf), "<%d>%s: %zu-byte message too long for printk\n",level, tag, size);
  }

  iovec iov[1];
  iov[0].iov_base = buf;
  iov[0].iov_len = size;
  TEMP_FAILURE_RETRY(writev(klog_fd, iov, 1));
}

void SetLogger(LogFunction&& logger) {
  std::lock_guard<std::mutex> lock(LoggingLock());
  Logger() = std::move(logger);
}


void InitLogging(char* argv[],LogFunction&& logger = INIT_LOGGING_DEFAULT_LOGGER,AbortFunction&& aborter = DefaultAborter){
  SetLogger(std::forward<LogFunction>(logger));
  SetAborter(std::forward<AbortFunction>(aborter));

  if (gInitialized) {
    return;
  }

  gInitialized = true;

  // Stash the command line for later use. We can use /proc/self/cmdline on
  // Linux to recover this, but we don't have that luxury on the Mac/Windows,
  // and there are a couple of argv[0] variants that are commonly used.
  if (argv != nullptr) {
    std::lock_guard<std::mutex> lock(LoggingLock());
    ProgramInvocationName() = basename(argv[0]);
  }

  const char* tags = getenv("ANDROID_LOG_TAGS");
  if (tags == nullptr) {
    return;
  }

  std::vector<std::string> specs = Split(tags, " ");
  for (size_t i = 0; i < specs.size(); ++i) {
    // "tag-pattern:[vdiwefs]"
    std::string spec(specs[i]);
    if (spec.size() == 3 && StartsWith(spec, "*:")) {
      switch (spec[2]) {
        case 'v':
          gMinimumLogSeverity = VERBOSE;
          continue;
        case 'd':
          gMinimumLogSeverity = DEBUG;
          continue;
        case 'i':
          gMinimumLogSeverity = INFO;
          continue;
        case 'w':
          gMinimumLogSeverity = WARNING;
          continue;
        case 'e':
          gMinimumLogSeverity = ERROR;
          continue;
        case 'f':
          gMinimumLogSeverity = FATAL_WITHOUT_ABORT;
          continue;
        // liblog will even suppress FATAL if you say 's' for silent, but that's
        // crazy!
        case 's':
          gMinimumLogSeverity = FATAL_WITHOUT_ABORT;
          continue;
      }
    }
    LOG(FATAL) << "unsupported '" << spec << "' in ANDROID_LOG_TAGS (" << tags << ")";
  }
}