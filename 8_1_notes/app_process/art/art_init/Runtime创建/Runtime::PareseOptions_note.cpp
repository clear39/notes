//	@art/runtime/runtime.cc
bool Runtime::ParseOptions(const RuntimeOptions& raw_options,bool ignore_unrecognized = JNI_FALSE,RuntimeArgumentMap* runtime_options) {
  InitLogging(/* argv */ nullptr, Abort);  // Calls Locks::Init() as a side effect.			//@art/runtime/base/logging.cc
  bool parsed = ParsedOptions::(raw_options, ignore_unrecognized, runtime_options);
  if (!parsed) {
    LOG(ERROR) << "Failed to parse options";
    return false;
  }
  return true;
}


/*
06-25 21:24:56.533  2798  2798 D AndroidRuntime: >>>>>> START com.android.internal.os.ZygoteInit uid 0 <<<<<<
06-25 21:24:56.581  2798  2798 I zygote  : option[0]=-Xzygote
06-25 21:24:56.581  2798  2798 I zygote  : option[1]=-Xusetombstonedtraces
06-25 21:24:56.581  2798  2798 I zygote  : option[2]=exit
06-25 21:24:56.581  2798  2798 I zygote  : option[3]=vfprintf
06-25 21:24:56.581  2798  2798 I zygote  : option[4]=sensitiveThread
06-25 21:24:56.581  2798  2798 I zygote  : option[5]=-verbose:gc
06-25 21:24:56.581  2798  2798 I zygote  : option[6]=-Xms16m
06-25 21:24:56.581  2798  2798 I zygote  : option[7]=-Xmx512m
06-25 21:24:56.581  2798  2798 I zygote  : option[8]=-XX:HeapGrowthLimit=192m
06-25 21:24:56.581  2798  2798 I zygote  : option[9]=-XX:HeapMinFree=512k
06-25 21:24:56.581  2798  2798 I zygote  : option[10]=-XX:HeapMaxFree=8m
06-25 21:24:56.581  2798  2798 I zygote  : option[11]=-XX:HeapTargetUtilization=0.75
06-25 21:24:56.581  2798  2798 I zygote  : option[12]=-Xusejit:true
06-25 21:24:56.581  2798  2798 I zygote  : option[13]=-Xjitsaveprofilinginfo
06-25 21:24:56.581  2798  2798 I zygote  : option[14]=-agentlib:jdwp=transport=dt_android_adb,suspend=n,server=y
06-25 21:24:56.582  2798  2798 I zygote  : option[15]=-Xlockprofthreshold:500
06-25 21:24:56.582  2798  2798 I zygote  : option[16]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[17]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[18]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[19]=-Xms64m
06-25 21:24:56.582  2798  2798 I zygote  : option[20]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[21]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[22]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[23]=-Xmx64m
06-25 21:24:56.582  2798  2798 I zygote  : option[24]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[25]=--image-classes=/system/etc/preloaded-classes
06-25 21:24:56.582  2798  2798 I zygote  : option[26]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[27]=--compiled-classes=/system/etc/compiled-classes
06-25 21:24:56.582  2798  2798 I zygote  : option[28]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[29]=--dirty-image-objects=/system/etc/dirty-image-objects
06-25 21:24:56.582  2798  2798 I zygote  : option[30]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[31]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[32]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[33]=-Xms64m
06-25 21:24:56.582  2798  2798 I zygote  : option[34]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[35]=--runtime-arg
06-25 21:24:56.582  2798  2798 I zygote  : option[36]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[37]=-Xmx512m
06-25 21:24:56.582  2798  2798 I zygote  : option[38]=-Ximage-compiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[39]=--instruction-set-variant=cortex-a9
06-25 21:24:56.582  2798  2798 I zygote  : option[40]=-Xcompiler-option
06-25 21:24:56.582  2798  2798 I zygote  : option[41]=--instruction-set-variant=cortex-a9
06-25 21:24:56.583  2798  2798 I zygote  : option[42]=-Ximage-compiler-option
06-25 21:24:56.583  2798  2798 I zygote  : option[43]=--instruction-set-features=default
06-25 21:24:56.583  2798  2798 I zygote  : option[44]=-Xcompiler-option
06-25 21:24:56.583  2798  2798 I zygote  : option[45]=--instruction-set-features=default
06-25 21:24:56.583  2798  2798 I zygote  : option[46]=-Duser.locale=en-US
06-25 21:24:56.583  2798  2798 I zygote  : option[47]=--cpu-abilist=armeabi-v7a,armeabi
06-25 21:24:56.583  2798  2798 I zygote  : option[48]=-Xfingerprint:Android/autolink_8qxp/autolink_8qxp:8.1.0/1.2.0-beta2-rc3/20180724:userdebug/dev-keys
*/
bool ParsedOptions::DoParse(const RuntimeOptions& options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
  LOG(INFO) << "art ParsedOptions::DoParse start:"
  for (size_t i = 0; i < options.size(); ++i) {
    if (true && options[0].first == "-Xzygote") {
      LOG(INFO) << "option[" << i << "]=" << options[i].first;
    }
  }
  LOG(INFO) << "art ParsedOptions::DoParse end."

  auto parser = MakeParser(ignore_unrecognized);

  // Convert to a simple string list (without the magic pointer options)
  std::vector<std::string> argv_list;
  if (!ProcessSpecialOptions(options, nullptr, &argv_list)) {
    return false;
  }

  CmdlineResult parse_result = parser->Parse(argv_list);

  // Handle parse errors by displaying the usage and potentially exiting.
  if (parse_result.IsError()) {
    if (parse_result.GetStatus() == CmdlineResult::kUsage) {
      UsageMessage(stdout, "%s\n", parse_result.GetMessage().c_str());
      Exit(0);
    } else if (parse_result.GetStatus() == CmdlineResult::kUnknown && !ignore_unrecognized) {
      Usage("%s\n", parse_result.GetMessage().c_str());
      return false;
    } else {
      Usage("%s\n", parse_result.GetMessage().c_str());
      Exit(0);
    }

    UNREACHABLE();
  }

  using M = RuntimeArgumentMap;
  RuntimeArgumentMap args = parser->ReleaseArgumentsMap();

  // -help, -showversion, etc.
  if (args.Exists(M::Help)) {
    Usage(nullptr);
    return false;
  } else if (args.Exists(M::ShowVersion)) {
    UsageMessage(stdout,
                 "ART version %s %s\n",
                 Runtime::GetVersion(),
                 GetInstructionSetString(kRuntimeISA));
    Exit(0);
  } else if (args.Exists(M::BootClassPath)) {
    LOG(INFO) << "setting boot class path to " << *args.Get(M::BootClassPath);
  }

  if (args.GetOrDefault(M::UseJitCompilation) && args.GetOrDefault(M::Interpret)) {
    Usage("-Xusejit:true and -Xint cannot be specified together");
    Exit(0);
  }

  // Set a default boot class path if we didn't get an explicit one via command line.
  if (getenv("BOOTCLASSPATH") != nullptr) {
    args.SetIfMissing(M::BootClassPath, std::string(getenv("BOOTCLASSPATH")));
  }

  // Set a default class path if we didn't get an explicit one via command line.
  if (getenv("CLASSPATH") != nullptr) {
    args.SetIfMissing(M::ClassPath, std::string(getenv("CLASSPATH")));
  }

  // Default to number of processors minus one since the main GC thread also does work.
  args.SetIfMissing(M::ParallelGCThreads, gc::Heap::kDefaultEnableParallelGC ?
      static_cast<unsigned int>(sysconf(_SC_NPROCESSORS_CONF) - 1u) : 0u);

  // -verbose:
  {
    LogVerbosity *log_verbosity = args.Get(M::Verbose);
    if (log_verbosity != nullptr) {
      gLogVerbosity = *log_verbosity;
    }
  }

  MaybeOverrideVerbosity();

  SetRuntimeDebugFlagsEnabled(args.Get(M::SlowDebug));

  // -Xprofile:
  Trace::SetDefaultClockSource(args.GetOrDefault(M::ProfileClock));

  if (!ProcessSpecialOptions(options, &args, nullptr)) {
      return false;
  }

  {
    // If not set, background collector type defaults to homogeneous compaction.
    // If foreground is GSS, use GSS as background collector.
    // If not low memory mode, semispace otherwise.

    gc::CollectorType background_collector_type_;
    gc::CollectorType collector_type_ = (XGcOption{}).collector_type_;  // NOLINT [whitespace/braces] [5]
    bool low_memory_mode_ = args.Exists(M::LowMemoryMode);

    background_collector_type_ = args.GetOrDefault(M::BackgroundGc);
    {
      XGcOption* xgc = args.Get(M::GcOption);
      if (xgc != nullptr && xgc->collector_type_ != gc::kCollectorTypeNone) {
        collector_type_ = xgc->collector_type_;
      }
    }

    if (background_collector_type_ == gc::kCollectorTypeNone) {
      if (collector_type_ != gc::kCollectorTypeGSS) {
        background_collector_type_ = low_memory_mode_ ?
            gc::kCollectorTypeSS : gc::kCollectorTypeHomogeneousSpaceCompact;
      } else {
        background_collector_type_ = collector_type_;
      }
    }

    args.Set(M::BackgroundGc, BackgroundGcOption { background_collector_type_ });
  }

  // If a reference to the dalvik core.jar snuck in, replace it with
  // the art specific version. This can happen with on device
  // boot.art/boot.oat generation by GenerateImage which relies on the
  // value of BOOTCLASSPATH.
#if defined(ART_TARGET)
  std::string core_jar("/core.jar");
  std::string core_libart_jar("/core-libart.jar");
#else
  // The host uses hostdex files.
  std::string core_jar("/core-hostdex.jar");
  std::string core_libart_jar("/core-libart-hostdex.jar");
#endif
  auto boot_class_path_string = args.GetOrDefault(M::BootClassPath);

  size_t core_jar_pos = boot_class_path_string.find(core_jar);
  if (core_jar_pos != std::string::npos) {
    boot_class_path_string.replace(core_jar_pos, core_jar.size(), core_libart_jar);
    args.Set(M::BootClassPath, boot_class_path_string);
  }

  {
    auto&& boot_class_path = args.GetOrDefault(M::BootClassPath);
    auto&& boot_class_path_locations = args.GetOrDefault(M::BootClassPathLocations);
    if (args.Exists(M::BootClassPathLocations)) {
      size_t boot_class_path_count = ParseStringList<':'>::Split(boot_class_path).Size();

      if (boot_class_path_count != boot_class_path_locations.Size()) {
        Usage("The number of boot class path files does not match"
            " the number of boot class path locations given\n"
            "  boot class path files     (%zu): %s\n"
            "  boot class path locations (%zu): %s\n",
            boot_class_path.size(), boot_class_path_string.c_str(),
            boot_class_path_locations.Size(), boot_class_path_locations.Join().c_str());
        return false;
      }
    }
  }

  if (!args.Exists(M::CompilerCallbacksPtr) && !args.Exists(M::Image)) {
    std::string image = GetAndroidRoot();
    image += "/framework/boot.art";
    args.Set(M::Image, image);
  }

  // 0 means no growth limit, and growth limit should be always <= heap size
  if (args.GetOrDefault(M::HeapGrowthLimit) <= 0u ||
      args.GetOrDefault(M::HeapGrowthLimit) > args.GetOrDefault(M::MemoryMaximumSize)) {
    args.Set(M::HeapGrowthLimit, args.GetOrDefault(M::MemoryMaximumSize));
  }

  *runtime_options = std::move(args);
  return true;
}







//  VariantMapKey@art/runtime/base/variant_map.h

template <typename TValue>
struct VariantMapKey : detail::VariantMapKeyRaw {}



//  RuntimeArgumentMapKey@art/runtime/runtime_options.h
//  
// Define a key that is usable with a RuntimeArgumentMap.
// This key will *not* work with other subtypes of VariantMap.
template <typename TValue>
struct RuntimeArgumentMapKey : VariantMapKey<TValue> {
  RuntimeArgumentMapKey() {}
  explicit RuntimeArgumentMapKey(TValue default_value)
    : VariantMapKey<TValue>(std::move(default_value)) {}
  // Don't ODR-use constexpr default values, which means that Struct::Fields
  // that are declared 'static constexpr T Name = Value' don't need to have a matching definition.
};

 //  VariantMap@art/runtime/base/variant_map.h
struct RuntimeArgumentMap : VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey> {
	// This 'using' line is necessary to inherit the variadic constructor.
	using VariantMap<RuntimeArgumentMap, RuntimeArgumentMapKey>::VariantMap;

	// Make the next many usages of Key slightly shorter to type.
	template <typename TValue>
	using Key = RuntimeArgumentMapKey<TValue>;

	// List of key declarations, shorthand for 'static const Key<T> Name'
	#define RUNTIME_OPTIONS_KEY(Type, Name, ...) static const Key<Type> (Name);
	#include "runtime_options.def"	//@runtime/runtime_options.def
};


//参数据解析我们举一个例子 分析
RUNTIME_OPTIONS_KEY (Unit,                Zygote)
//==》
static const RuntimeArgumentMapKey<Unit> Zygote;	//	Unit @art/cmdline/unit.h
//==》
struct RuntimeArgumentMapKey : VariantMapKey<Unit> {
  RuntimeArgumentMapKey() {}
};
//==》
//
template <typename TValue>
struct VariantMapKey : detail::VariantMapKeyRaw {
protected:
  VariantMapKey() {}
}
// Type-erased version of VariantMapKey<T>
struct VariantMapKeyRaw {
 VariantMapKeyRaw(): key_counter_(VariantMapKeyCounterAllocator::AllocateCounter()) {

 }
}

// Allocate a unique counter value each time it's called.
struct VariantMapKeyCounterAllocator {
	static size_t AllocateCounter() {
	  static size_t counter = 0;
	  counter++;

	  return counter;
	}
};


//	@art/runtime/parsed_options.cc:36:using MemoryKiB = Memory<1024>;	//Memory@art/cmdline/memory_representation.h
RUNTIME_OPTIONS_KEY (MemoryKiB,           HeapMaxFree,                    gc::Heap::kDefaultMaxFree)












///////////////////////////////////////////////////////////////////////////////////////////////
//日志初始化
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