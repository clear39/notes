



ParsedOptions::ParsedOptions()
  : hook_is_sensitive_thread_(nullptr),
    hook_vfprintf_(vfprintf),
    hook_exit_(exit),
    hook_abort_(nullptr) {                          // We don't call abort(3) by default; see
                                                    // Runtime::Abort
}

bool ParsedOptions::Parse(const RuntimeOptions& options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
  CHECK(runtime_options != nullptr);

  ParsedOptions parser;
  return parser.DoParse(options, ignore_unrecognized, runtime_options);
}



bool ParsedOptions::DoParse(const RuntimeOptions& options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
  for (size_t i = 0; i < options.size(); ++i) {
    if (options[0].first == "-Xzygote") {
      LOG(INFO) << "option[" << i << "]=" << options[i].first;
    }
  }

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
    gc::CollectorType collector_type_ = (XGcOption{}).collector_type_;
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