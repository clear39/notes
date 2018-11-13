/***
if (!Runtime::Create(options, ignore_unrecognized)) {//Runtime创建
	return JNI_ERR;
}
*/



//	@art/runtime/runtime.cc
//	@art/runtime/runtime.h:98: typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;
//虚拟机创建入口
bool Runtime::Create(const RuntimeOptions& raw_options, bool ignore_unrecognized = JNI_FALSE) {
  RuntimeArgumentMap runtime_options;
  return ParseOptions(raw_options, ignore_unrecognized, &runtime_options) && Create(std::move(runtime_options));
}


struct RuntimeArgumentMap : VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey> {
	// This 'using' line is necessary to inherit the variadic constructor.
	using VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey>::VariantMap;

	// Make the next many usages of Key slightly shorter to type.
	template <typename TValue>
	using Key = RuntimeArgumentMapKey<TValue>;

	// List of key declarations, shorthand for 'static const Key<T> Name'
	#define RUNTIME_OPTIONS_KEY(Type, Name, ...) static const Key<Type> (Name);
	#include "runtime_options.def"
};


//	@art/runtime/runtime.cc
bool Runtime::ParseOptions(const RuntimeOptions& raw_options,bool ignore_unrecognized = JNI_FALSE,RuntimeArgumentMap* runtime_options) {
  InitLogging(/* argv */ nullptr, Abort);  // Calls Locks::Init() as a side effect.			//@art/runtime/base/logging.cc
  bool parsed = ParsedOptions::Parse(raw_options, ignore_unrecognized, runtime_options);
  if (!parsed) {
    LOG(ERROR) << "Failed to parse options";
    return false;
  }
  return true;
}


void InitLogging(char* argv[] = nullptr, AbortFunction& abort_function) {
  if (gCmdLine.get() != nullptr) {
    return;
  }
  // TODO: Move this to a more obvious InitART...
  Locks::Init();

  // Stash the command line for later use. We can use /proc/self/cmdline on Linux to recover this,
  // but we don't have that luxury on the Mac, and there are a couple of argv[0] variants that are
  // commonly used.
  if (argv != nullptr) {//argv  == nullptr
  	......
  } else {
    // TODO: fall back to /proc/self/cmdline when argv is null on Linux.
    gCmdLine.reset(new std::string("<unset>"));
  }

#ifdef ART_TARGET_ANDROID
#define INIT_LOGGING_DEFAULT_LOGGER android::base::LogdLogger()
#else
#define INIT_LOGGING_DEFAULT_LOGGER android::base::StderrLogger
#endif
  //	@system/core/base/logging.cpp:256
  android::base::InitLogging(argv, INIT_LOGGING_DEFAULT_LOGGER,std::move<AbortFunction>(abort_function));
#undef INIT_LOGGING_DEFAULT_LOGGER
}

void InitLogging(char* argv[], LogFunction&& logger, AbortFunction&& aborter) {
  SetLogger(std::forward<LogFunction>(logger));//	android::base::LogdLogger()
  SetAborter(std::forward<AbortFunction>(aborter));

  if (gInitialized) {//这里在init进程中已经初始化过了,zygote是由initfork
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


void LogdLogger::operator()(LogId id, LogSeverity severity, const char* tag,const char* file, unsigned int line, const char* message) {
  static constexpr android_LogPriority kLogSeverityToAndroidLogPriority[] = {
      ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO,
      ANDROID_LOG_WARN,    ANDROID_LOG_ERROR, ANDROID_LOG_FATAL,
      ANDROID_LOG_FATAL,
  };
  static_assert(arraysize(kLogSeverityToAndroidLogPriority) == FATAL + 1, "Mismatch in size of kLogSeverityToAndroidLogPriority and values in LogSeverity");

  int priority = kLogSeverityToAndroidLogPriority[severity];
  if (id == DEFAULT) {
    id = default_log_id_;
  }

  static constexpr log_id kLogIdToAndroidLogId[] = {
    LOG_ID_MAX, LOG_ID_MAIN, LOG_ID_SYSTEM,
  };
  static_assert(arraysize(kLogIdToAndroidLogId) == SYSTEM + 1,"Mismatch in size of kLogIdToAndroidLogId and values in LogId");
  log_id lg_id = kLogIdToAndroidLogId[id];

  if (priority == ANDROID_LOG_FATAL) {
    __android_log_buf_print(lg_id, priority, tag, "%s:%u] %s", file, line,message);
  } else {
    __android_log_buf_print(lg_id, priority, tag, "%s", message);
  }
}

//	@art/runtime/runtime.cc
void Runtime::Abort(const char* msg) {
  auto old_value = gAborting.fetch_add(1);  // set before taking any locks

#ifdef ART_TARGET_ANDROID
  if (old_value == 0) {
    // Only set the first abort message.
    android_set_abort_message(msg);
  }
#else
  UNUSED(old_value);
#endif

#ifdef ART_TARGET_ANDROID
  android_set_abort_message(msg);
#endif

  // Ensure that we don't have multiple threads trying to abort at once,
  // which would result in significantly worse diagnostics.
  MutexLock mu(Thread::Current(), *Locks::abort_lock_);

  // Get any pending output out of the way.
  fflush(nullptr);

  // Many people have difficulty distinguish aborts from crashes,
  // so be explicit.
  // Note: use cerr on the host to print log lines immediately, so we get at least some output
  //       in case of recursive aborts. We lose annotation with the source file and line number
  //       here, which is a minor issue. The same is significantly more complicated on device,
  //       which is why we ignore the issue there.
  AbortState state;
  if (kIsTargetBuild) {
    LOG(FATAL_WITHOUT_ABORT) << Dumpable<AbortState>(state);
  } else {
    std::cerr << Dumpable<AbortState>(state);
  }

  // Sometimes we dump long messages, and the Android abort message only retains the first line.
  // In those cases, just log the message again, to avoid logcat limits.
  if (msg != nullptr && strchr(msg, '\n') != nullptr) {
    LOG(FATAL_WITHOUT_ABORT) << msg;
  }

  // Call the abort hook if we have one.
  if (Runtime::Current() != nullptr && Runtime::Current()->abort_ != nullptr) {
    LOG(FATAL_WITHOUT_ABORT) << "Calling abort hook...";
    Runtime::Current()->abort_();
    // notreached
    LOG(FATAL_WITHOUT_ABORT) << "Unexpectedly returned from abort hook!";
  }

#if defined(__GLIBC__)
  // TODO: we ought to be able to use pthread_kill(3) here (or abort(3),
  // which POSIX defines in terms of raise(3), which POSIX defines in terms
  // of pthread_kill(3)). On Linux, though, libcorkscrew can't unwind through
  // libpthread, which means the stacks we dump would be useless. Calling
  // tgkill(2) directly avoids that.
  syscall(__NR_tgkill, getpid(), GetTid(), SIGABRT);
  // TODO: LLVM installs it's own SIGABRT handler so exit to be safe... Can we disable that in LLVM?
  // If not, we could use sigaction(3) before calling tgkill(2) and lose this call to exit(3).
  exit(1);
#else
  abort();
#endif
  // notreached
}




