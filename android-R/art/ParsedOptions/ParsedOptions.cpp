
//  @   art/runtime/parsed_options.cc
ParsedOptions::ParsedOptions()
  : hook_is_sensitive_thread_(nullptr),
    hook_vfprintf_(vfprintf),
    hook_exit_(exit),
    hook_abort_(nullptr) {                          // We don't call abort(3) by default; see
                                                    // Runtime::Abort
}



bool ParsedOptions::Parse(const RuntimeOptions& options,
                          bool ignore_unrecognized,
                          RuntimeArgumentMap* runtime_options) {
  CHECK(runtime_options != nullptr);

  ParsedOptions parser;
  return parser.DoParse(options, ignore_unrecognized, runtime_options);
}



/***
 * 
*/
bool ParsedOptions::DoParse(const RuntimeOptions& options,
                            bool ignore_unrecognized,
                            RuntimeArgumentMap* runtime_options) {
  for (size_t i = 0; i < options.size(); ++i) {
    if (true && options[0].first == "-Xzygote") {
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
    LOG(INFO) << "setting boot class path to " << args.Get(M::BootClassPath)->Join();
  }

  if (args.GetOrDefault(M::Interpret)) {
    if (args.Exists(M::UseJitCompilation) && *args.Get(M::UseJitCompilation)) {
      Usage("-Xusejit:true and -Xint cannot be specified together\n");
      Exit(0);
    }
    args.Set(M::UseJitCompilation, false);
  }

  // Set a default boot class path if we didn't get an explicit one via command line.
  const char* env_bcp = getenv("BOOTCLASSPATH");
  if (env_bcp != nullptr) {
    args.SetIfMissing(M::BootClassPath, ParseStringList<':'>::Split(env_bcp));
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

  SetRuntimeDebugFlagsEnabled(args.GetOrDefault(M::SlowDebug));

  // -Xprofile:
  Trace::SetDefaultClockSource(args.GetOrDefault(M::ProfileClock));

  if (!ProcessSpecialOptions(options, &args, nullptr)) {
      return false;
  }

  {
    // If not set, background collector type defaults to homogeneous compaction.
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
      background_collector_type_ = low_memory_mode_ ?
          gc::kCollectorTypeSS : gc::kCollectorTypeHomogeneousSpaceCompact;
    }

    args.Set(M::BackgroundGc, BackgroundGcOption { background_collector_type_ });
  }

  const ParseStringList<':'>* boot_class_path_locations = args.Get(M::BootClassPathLocations);
  if (boot_class_path_locations != nullptr && boot_class_path_locations->Size() != 0u) {
    const ParseStringList<':'>* boot_class_path = args.Get(M::BootClassPath);
    if (boot_class_path == nullptr ||
        boot_class_path_locations->Size() != boot_class_path->Size()) {
      Usage("The number of boot class path files does not match"
          " the number of boot class path locations given\n"
          "  boot class path files     (%zu): %s\n"
          "  boot class path locations (%zu): %s\n",
          (boot_class_path != nullptr) ? boot_class_path->Size() : 0u,
          (boot_class_path != nullptr) ? boot_class_path->Join().c_str() : "<nil>",
          boot_class_path_locations->Size(),
          boot_class_path_locations->Join().c_str());
      return false;
    }
  }

  if (!args.Exists(M::CompilerCallbacksPtr) && !args.Exists(M::Image)) {
    std::string image = GetDefaultBootImageLocation(GetAndroidRoot());
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



std::unique_ptr<RuntimeParser> ParsedOptions::MakeParser(bool ignore_unrecognized) {
  // RuntimeArgumentMap    @    art/runtime/runtime_options.h
  using M = RuntimeArgumentMap;

    /**
     * RuntimeParser    @art/runtime/parsed_options.h
     * using RuntimeParser = CmdlineParser<RuntimeArgumentMap, RuntimeArgumentMap::Key>;
     * 
     * RuntimeParser::Builder   @   art/cmdline/cmdline_parser.h
     * 
    */
  std::unique_ptr<RuntimeParser::Builder> parser_builder =
      std::make_unique<RuntimeParser::Builder>();

  HiddenapiPolicyValueMap hiddenapi_policy_valuemap =
      {{"disabled",  hiddenapi::EnforcementPolicy::kDisabled},
       {"just-warn", hiddenapi::EnforcementPolicy::kJustWarn},
       {"enabled",   hiddenapi::EnforcementPolicy::kEnabled}};

  DCHECK_EQ(hiddenapi_policy_valuemap.size(),
            static_cast<size_t>(hiddenapi::EnforcementPolicy::kMax) + 1);

  parser_builder->
       Define("-Xzygote")
          .IntoKey(M::Zygote)
      .Define("-Xprimaryzygote")
          .IntoKey(M::PrimaryZygote)
      .Define("-help")
          .IntoKey(M::Help)
      .Define("-showversion")
          .IntoKey(M::ShowVersion)
      .Define("-Xbootclasspath:_")
          .WithType<ParseStringList<':'>>()  // std::vector<std::string>, split by :
          .IntoKey(M::BootClassPath)
      .Define("-Xbootclasspath-locations:_")
          .WithType<ParseStringList<':'>>()  // std::vector<std::string>, split by :
          .IntoKey(M::BootClassPathLocations)
      .Define({"-classpath _", "-cp _"})
          .WithType<std::string>()
          .IntoKey(M::ClassPath)
      .Define("-Ximage:_")
          .WithType<std::string>()
          .IntoKey(M::Image)
      .Define("-Ximage-load-order:_")
          .WithType<gc::space::ImageSpaceLoadingOrder>()
          .WithValueMap({{"system", gc::space::ImageSpaceLoadingOrder::kSystemFirst},
                         {"data", gc::space::ImageSpaceLoadingOrder::kDataFirst}})
          .IntoKey(M::ImageSpaceLoadingOrder)
      .Define("-Xcheck:jni")
          .IntoKey(M::CheckJni)
      .Define("-Xjniopts:forcecopy")
          .IntoKey(M::JniOptsForceCopy)
      .Define("-XjdwpProvider:_")
          .WithType<JdwpProvider>()
          .IntoKey(M::JdwpProvider)
      .Define("-XjdwpOptions:_")
          .WithType<std::string>()
          .IntoKey(M::JdwpOptions)
      // TODO Re-enable -agentlib: once I have a good way to transform the values.
      // .Define("-agentlib:_")
      //     .WithType<std::vector<ti::Agent>>().AppendValues()
      //     .IntoKey(M::AgentLib)
      .Define("-agentpath:_")
          .WithType<std::list<ti::AgentSpec>>().AppendValues()
          .IntoKey(M::AgentPath)
      .Define("-Xms_")
          .WithType<MemoryKiB>()
          .IntoKey(M::MemoryInitialSize)
      .Define("-Xmx_")
          .WithType<MemoryKiB>()
          .IntoKey(M::MemoryMaximumSize)
      .Define("-XX:HeapGrowthLimit=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapGrowthLimit)
      .Define("-XX:HeapMinFree=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapMinFree)
      .Define("-XX:HeapMaxFree=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::HeapMaxFree)
      .Define("-XX:NonMovingSpaceCapacity=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::NonMovingSpaceCapacity)
      .Define("-XX:StopForNativeAllocs=_")
          .WithType<MemoryKiB>()
          .IntoKey(M::StopForNativeAllocs)
      .Define("-XX:HeapTargetUtilization=_")
          .WithType<double>().WithRange(0.1, 0.9)
          .IntoKey(M::HeapTargetUtilization)
      .Define("-XX:ForegroundHeapGrowthMultiplier=_")
          .WithType<double>().WithRange(0.1, 5.0)
          .IntoKey(M::ForegroundHeapGrowthMultiplier)
      .Define("-XX:ParallelGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ParallelGCThreads)
      .Define("-XX:ConcGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ConcGCThreads)
      .Define("-XX:FinalizerTimeoutMs=_")
          .WithType<unsigned int>()
          .IntoKey(M::FinalizerTimeoutMs)
      .Define("-Xss_")
          .WithType<Memory<1>>()
          .IntoKey(M::StackSize)
      .Define("-XX:MaxSpinsBeforeThinLockInflation=_")
          .WithType<unsigned int>()
          .IntoKey(M::MaxSpinsBeforeThinLockInflation)
      .Define("-XX:LongPauseLogThreshold=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::LongPauseLogThreshold)
      .Define("-XX:LongGCLogThreshold=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::LongGCLogThreshold)
      .Define("-XX:DumpGCPerformanceOnShutdown")
          .IntoKey(M::DumpGCPerformanceOnShutdown)
      .Define("-XX:DumpRegionInfoBeforeGC")
          .IntoKey(M::DumpRegionInfoBeforeGC)
      .Define("-XX:DumpRegionInfoAfterGC")
          .IntoKey(M::DumpRegionInfoAfterGC)
      .Define("-XX:DumpJITInfoOnShutdown")
          .IntoKey(M::DumpJITInfoOnShutdown)
      .Define("-XX:IgnoreMaxFootprint")
          .IntoKey(M::IgnoreMaxFootprint)
      .Define("-XX:LowMemoryMode")
          .IntoKey(M::LowMemoryMode)
      .Define("-XX:UseTLAB")
          .WithValue(true)
          .IntoKey(M::UseTLAB)
      .Define({"-XX:EnableHSpaceCompactForOOM", "-XX:DisableHSpaceCompactForOOM"})
          .WithValues({true, false})
          .IntoKey(M::EnableHSpaceCompactForOOM)
      .Define("-XX:DumpNativeStackOnSigQuit:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::DumpNativeStackOnSigQuit)
      .Define("-XX:MadviseRandomAccess:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::MadviseRandomAccess)
      .Define("-Xusejit:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::UseJitCompilation)
      .Define("-Xusetieredjit:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::UseTieredJitCompilation)
      .Define("-Xjitinitialsize:_")
          .WithType<MemoryKiB>()
          .IntoKey(M::JITCodeCacheInitialCapacity)
      .Define("-Xjitmaxsize:_")
          .WithType<MemoryKiB>()
          .IntoKey(M::JITCodeCacheMaxCapacity)
      .Define("-Xjitthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITCompileThreshold)
      .Define("-Xjitwarmupthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITWarmupThreshold)
      .Define("-Xjitosrthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITOsrThreshold)
      .Define("-Xjitprithreadweight:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITPriorityThreadWeight)
      .Define("-Xjittransitionweight:_")
          .WithType<unsigned int>()
          .IntoKey(M::JITInvokeTransitionWeight)
      .Define("-Xjitpthreadpriority:_")
          .WithType<int>()
          .IntoKey(M::JITPoolThreadPthreadPriority)
      .Define("-Xjitsaveprofilinginfo")
          .WithType<ProfileSaverOptions>()
          .AppendValues()
          .IntoKey(M::ProfileSaverOpts)
      .Define("-Xps-_")  // profile saver options -Xps-<key>:<value>
          .WithType<ProfileSaverOptions>()
          .AppendValues()
          .IntoKey(M::ProfileSaverOpts)  // NOTE: Appends into same key as -Xjitsaveprofilinginfo
      .Define("-XX:HspaceCompactForOOMMinIntervalMs=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::HSpaceCompactForOOMMinIntervalsMs)
      .Define("-D_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::PropertiesList)
      .Define("-Xjnitrace:_")
          .WithType<std::string>()
          .IntoKey(M::JniTrace)
      .Define({"-Xrelocate", "-Xnorelocate"})
          .WithValues({true, false})
          .IntoKey(M::Relocate)
      .Define({"-Ximage-dex2oat", "-Xnoimage-dex2oat"})
          .WithValues({true, false})
          .IntoKey(M::ImageDex2Oat)
      .Define("-Xint")
          .WithValue(true)
          .IntoKey(M::Interpret)
      .Define("-Xgc:_")
          .WithType<XGcOption>()
          .IntoKey(M::GcOption)
      .Define("-XX:LargeObjectSpace=_")
          .WithType<gc::space::LargeObjectSpaceType>()
          .WithValueMap({{"disabled", gc::space::LargeObjectSpaceType::kDisabled},
                         {"freelist", gc::space::LargeObjectSpaceType::kFreeList},
                         {"map",      gc::space::LargeObjectSpaceType::kMap}})
          .IntoKey(M::LargeObjectSpace)
      .Define("-XX:LargeObjectThreshold=_")
          .WithType<Memory<1>>()
          .IntoKey(M::LargeObjectThreshold)
      .Define("-XX:BackgroundGC=_")
          .WithType<BackgroundGcOption>()
          .IntoKey(M::BackgroundGc)
      .Define("-XX:+DisableExplicitGC")
          .IntoKey(M::DisableExplicitGC)
      .Define("-verbose:_")
          .WithType<LogVerbosity>()
          .IntoKey(M::Verbose)
      .Define("-Xlockprofthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::LockProfThreshold)
      .Define("-Xstackdumplockprofthreshold:_")
          .WithType<unsigned int>()
          .IntoKey(M::StackDumpLockProfThreshold)
      .Define("-Xmethod-trace")
          .IntoKey(M::MethodTrace)
      .Define("-Xmethod-trace-file:_")
          .WithType<std::string>()
          .IntoKey(M::MethodTraceFile)
      .Define("-Xmethod-trace-file-size:_")
          .WithType<unsigned int>()
          .IntoKey(M::MethodTraceFileSize)
      .Define("-Xmethod-trace-stream")
          .IntoKey(M::MethodTraceStreaming)
      .Define("-Xprofile:_")
          .WithType<TraceClockSource>()
          .WithValueMap({{"threadcpuclock", TraceClockSource::kThreadCpu},
                         {"wallclock",      TraceClockSource::kWall},
                         {"dualclock",      TraceClockSource::kDual}})
          .IntoKey(M::ProfileClock)
      .Define("-Xcompiler:_")
          .WithType<std::string>()
          .IntoKey(M::Compiler)
      .Define("-Xcompiler-option _")
          .WithType<std::vector<std::string>>()
          .AppendValues()
          .IntoKey(M::CompilerOptions)
      .Define("-Ximage-compiler-option _")
          .WithType<std::vector<std::string>>()
          .AppendValues()
          .IntoKey(M::ImageCompilerOptions)
      .Define("-Xverify:_")
          .WithType<verifier::VerifyMode>()
          .WithValueMap({{"none",     verifier::VerifyMode::kNone},
                         {"remote",   verifier::VerifyMode::kEnable},
                         {"all",      verifier::VerifyMode::kEnable},
                         {"softfail", verifier::VerifyMode::kSoftFail}})
          .IntoKey(M::Verify)
      .Define("-XX:NativeBridge=_")
          .WithType<std::string>()
          .IntoKey(M::NativeBridge)
      .Define("-Xzygote-max-boot-retry=_")
          .WithType<unsigned int>()
          .IntoKey(M::ZygoteMaxFailedBoots)
      .Define("-Xno-dex-file-fallback")
          .IntoKey(M::NoDexFileFallback)
      .Define("-Xno-sig-chain")
          .IntoKey(M::NoSigChain)
      .Define("--cpu-abilist=_")
          .WithType<std::string>()
          .IntoKey(M::CpuAbiList)
      .Define("-Xfingerprint:_")
          .WithType<std::string>()
          .IntoKey(M::Fingerprint)
      .Define("-Xexperimental:_")
          .WithType<ExperimentalFlags>()
          .AppendValues()
          .IntoKey(M::Experimental)
      .Define("-Xforce-nb-testing")
          .IntoKey(M::ForceNativeBridge)
      .Define("-Xplugin:_")
          .WithType<std::vector<Plugin>>().AppendValues()
          .IntoKey(M::Plugins)
      .Define("-XX:ThreadSuspendTimeout=_")  // in ms
          .WithType<MillisecondsToNanoseconds>()  // store as ns
          .IntoKey(M::ThreadSuspendTimeout)
      .Define("-XX:GlobalRefAllocStackTraceLimit=_")  // Number of free slots to enable tracing.
          .WithType<unsigned int>()
          .IntoKey(M::GlobalRefAllocStackTraceLimit)
      .Define("-XX:SlowDebug=_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::SlowDebug)
      .Define("-Xtarget-sdk-version:_")
          .WithType<unsigned int>()
          .IntoKey(M::TargetSdkVersion)
      .Define("-Xhidden-api-policy:_")
          .WithType<hiddenapi::EnforcementPolicy>()
          .WithValueMap(hiddenapi_policy_valuemap)
          .IntoKey(M::HiddenApiPolicy)
      .Define("-Xcore-platform-api-policy:_")
          .WithType<hiddenapi::EnforcementPolicy>()
          .WithValueMap(hiddenapi_policy_valuemap)
          .IntoKey(M::CorePlatformApiPolicy)
      .Define("-Xuse-stderr-logger")
          .IntoKey(M::UseStderrLogger)
      .Define("-Xonly-use-system-oat-files")
          .IntoKey(M::OnlyUseSystemOatFiles)
      .Define("-Xverifier-logging-threshold=_")
          .WithType<unsigned int>()
          .IntoKey(M::VerifierLoggingThreshold)
      .Define("-XX:FastClassNotFoundException=_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::FastClassNotFoundException)
      .Define("-Xopaque-jni-ids:_")
          .WithType<JniIdType>()
          .WithValueMap({{"true", JniIdType::kIndices},
                         {"false", JniIdType::kPointer},
                         {"swapable", JniIdType::kSwapablePointer},
                         {"pointer", JniIdType::kPointer},
                         {"indices", JniIdType::kIndices},
                         {"default", JniIdType::kDefault}})
          .IntoKey(M::OpaqueJniIds)
      .Define("-Xauto-promote-opaque-jni-ids:_")
          .WithType<bool>()
          .WithValueMap({{"true", true}, {"false", false}})
          .IntoKey(M::AutoPromoteOpaqueJniIds)
      .Define("-XX:VerifierMissingKThrowFatal=_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::VerifierMissingKThrowFatal)
      .Define("-XX:PerfettoHprof=_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::PerfettoHprof)
      .Ignore({
          "-ea", "-da", "-enableassertions", "-disableassertions", "--runtime-arg", "-esa",
          "-dsa", "-enablesystemassertions", "-disablesystemassertions", "-Xrs", "-Xint:_",
          "-Xdexopt:_", "-Xnoquithandler", "-Xjnigreflimit:_", "-Xgenregmap", "-Xnogenregmap",
          "-Xverifyopt:_", "-Xcheckdexsum", "-Xincludeselectedop", "-Xjitop:_",
          "-Xincludeselectedmethod", "-Xjitthreshold:_",
          "-Xjitblocking", "-Xjitmethod:_", "-Xjitclass:_", "-Xjitoffset:_",
          "-Xjitconfig:_", "-Xjitcheckcg", "-Xjitverbose", "-Xjitprofile",
          "-Xjitdisableopt", "-Xjitsuspendpoll", "-XX:mainThreadStackSize=_"})
      .IgnoreUnrecognized(ignore_unrecognized);

  // TODO: Move Usage information into this DSL.

  return std::make_unique<RuntimeParser>(parser_builder->Build());
}