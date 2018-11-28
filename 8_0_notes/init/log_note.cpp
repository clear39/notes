//	InitKernelLogging(argv);


//		@system/core/init/log.cpp
void InitKernelLogging(char* argv[]) {
    // Make stdin/stdout/stderr all point to /dev/null.
    int fd = open("/sys/fs/selinux/null", O_RDWR);
    if (fd == -1) {
        int saved_errno = errno;
        android::base::InitLogging(argv, &android::base::KernelLogger);
        errno = saved_errno;
        PLOG(FATAL) << "Couldn't open /sys/fs/selinux/null";
    }

    //将stdin/stdout/stderr 重定向 到 /sys/fs/selinux/null
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2) close(fd);

    //  @system/core/base/logging.cpp
    android::base::InitLogging(argv, &android::base::KernelLogger);
}

//  @system/core/base/logging.cpp
//  void InitLogging(char* argv[], LogFunction&& logger = INIT_LOGGING_DEFAULT_LOGGER,AbortFunction&& aborter = DefaultAborter); //     #define INIT_LOGGING_DEFAULT_LOGGER LogdLogger()
void InitLogging(char* argv[], LogFunction&& logger, AbortFunction&& aborter) {
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

using LogFunction = std::function<void(LogId, LogSeverity, const char*, const char*,unsigned int, const char*)>;


void SetLogger(LogFunction&& logger) {
  std::lock_guard<std::mutex> lock(LoggingLock());
  Logger() = std::move(logger);
}



static LogFunction& Logger() {
#ifdef __ANDROID__
  static auto& logger = *new LogFunction(LogdLogger());//注意这里是静态创建
#else
  static auto& logger = *new LogFunction(StderrLogger);
#endif
  return logger;
}


class LogdLogger {
 public:
  explicit LogdLogger(LogId default_log_id = android::base::MAIN);

  void operator()(LogId, LogSeverity, const char* tag, const char* file,unsigned int line, const char* message);

 private:
  LogId default_log_id_;
};

void LogdLogger::operator()(LogId id, LogSeverity severity, const char* tag,const char* file, unsigned int line,const char* message) {
  static constexpr android_LogPriority kLogSeverityToAndroidLogPriority[] = {
      ANDROID_LOG_VERBOSE, 
      ANDROID_LOG_DEBUG, 
      ANDROID_LOG_INFO,
      ANDROID_LOG_WARN,    
      ANDROID_LOG_ERROR, 
      ANDROID_LOG_FATAL,
      ANDROID_LOG_FATAL,
  };
  static_assert(arraysize(kLogSeverityToAndroidLogPriority) == FATAL + 1,"Mismatch in size of kLogSeverityToAndroidLogPriority and values in LogSeverity");

  int priority = kLogSeverityToAndroidLogPriority[severity];
  if (id == DEFAULT) {
    id = default_log_id_;
  }

  static constexpr log_id kLogIdToAndroidLogId[] = {
    LOG_ID_MAX, LOG_ID_MAIN, LOG_ID_SYSTEM,
  };
  static_assert(arraysize(kLogIdToAndroidLogId) == SYSTEM + 1,"Mismatch in size of kLogIdToAndroidLogId and values in LogId");
  log_id lg_id = kLogIdToAndroidLogId[id];


  //  __android_log_buf_print   @system/core/liblog/logger_write.c
  if (priority == ANDROID_LOG_FATAL) {
    __android_log_buf_print(lg_id, priority, tag, "%s:%u] %s", file, line,message);
  } else {
    __android_log_buf_print(lg_id, priority, tag, "%s", message);
  }
}

using AbortFunction = std::function<void(const char*)>;

void SetAborter(AbortFunction&& aborter) {
  std::lock_guard<std::mutex> lock(LoggingLock());
  Aborter() = std::move(aborter);
}

static AbortFunction& Aborter() {
  static auto& aborter = *new AbortFunction(DefaultAborter);
  return aborter;
}

void DefaultAborter(const char* abort_message) {
#ifdef __ANDROID__
  android_set_abort_message(abort_message);//   @bionic/libc/bionic/android_set_abort_message.cpp
#else
  UNUSED(abort_message);
#endif
  abort();
}

//  @bionic/libc/bionic/android_set_abort_message.cpp
void android_set_abort_message(const char* msg) {
  ScopedPthreadMutexLocker locker(&g_abort_msg_lock);

  if (__abort_message_ptr == nullptr) {
    // We must have crashed _very_ early.
    return;
  }

  if (*__abort_message_ptr != nullptr) {
    // We already have an abort message.
    // Assume that the first crash is the one most worth reporting.
    return;
  }

  size_t size = sizeof(abort_msg_t) + strlen(msg) + 1;
  void* map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (map == MAP_FAILED) {
    return;
  }

  abort_msg_t* new_abort_message = reinterpret_cast<abort_msg_t*>(map);
  new_abort_message->size = size;
  strcpy(new_abort_message->msg, msg);
  *__abort_message_ptr = new_abort_message;
}


static std::string& ProgramInvocationName() {
  static auto& programInvocationName = *new std::string(getprogname());
  return programInvocationName;
}


const char* getprogname() {
  return program_invocation_short_name;
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



//该文档主要针对init c层日志打印

//  @system/core/base/include/android-base/logging.h
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





//  @system/core/base/logging.cpp
static bool gInitialized = false;
static LogSeverity gMinimumLogSeverity = INFO;
LogSeverity GetMinimumLogSeverity() {
    return gMinimumLogSeverity;
}


// 将字符串写入一个输出流中，在析构函数中再次写入设备

// Get an ostream that can be used for logging at the given severity and to the
// given destination. The same notes as for LOG_STREAM apply.
#define LOG_STREAM_TO(dest, severity)                             ::android::base::LogMessage(__FILE__, __LINE__,::android::base::dest,SEVERITY_LAMBDA(severity), -1).stream()

LogMessage::LogMessage(const char* file, unsigned int line, LogId id,LogSeverity severity, int error)
    //  const std::unique_ptr<LogMessageData> data_;
    : data_(new LogMessageData(file, line, id, severity, error)) {
}

LogMessageData(const char* file, unsigned int line, LogId id,LogSeverity severity, int error)
    //  const char* const file_;
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


void LogMessage::LogLine(const char* file, unsigned int line, LogId id,LogSeverity severity, const char* message) {
  const char* tag = ProgramInvocationName().c_str();
  Logger()(id, severity, tag, file, line, message);
}

//  @system/core/base/logging.cpp
static std::string& ProgramInvocationName() {
  static auto& programInvocationName = *new std::string(getprogname());
  return programInvocationName;
}


const char* getprogname() {
  return program_invocation_short_name;//这个是linux系统导出的进程名
}







