//	art/runtime/parsed_options.cc

bool ParsedOptions::Parse(const RuntimeOptions& options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
  CHECK(runtime_options != nullptr);

  ParsedOptions parser;
  return parser.DoParse(options, ignore_unrecognized, runtime_options);
}

/*
01-09 03:14:12.064   246   246 I zygote  : option[0]=-Xzygote
01-09 03:14:12.064   246   246 I zygote  : option[1]=-Xcheck:jni
01-09 03:14:12.064   246   246 I zygote  : option[2]=-Xstacktracefile:/data/anr/traces.txt
01-09 03:14:12.064   246   246 I zygote  : option[3]=exit
01-09 03:14:12.064   246   246 I zygote  : option[4]=vfprintf
01-09 03:14:12.065   246   246 I zygote  : option[5]=sensitiveThread
01-09 03:14:12.065   246   246 I zygote  : option[6]=-verbose:gc
01-09 03:14:12.065   246   246 I zygote  : option[7]=-Xms8m
01-09 03:14:12.065   246   246 I zygote  : option[8]=-Xmx384m
01-09 03:14:12.065   246   246 I zygote  : option[9]=-XX:HeapGrowthLimit=128m
01-09 03:14:12.065   246   246 I zygote  : option[10]=-XX:HeapMinFree=512k
01-09 03:14:12.065   246   246 I zygote  : option[11]=-XX:HeapMaxFree=8m
01-09 03:14:12.065   246   246 I zygote  : option[12]=-XX:HeapTargetUtilization=0.75
01-09 03:14:12.065   246   246 I zygote  : option[13]=-Xusejit:true
01-09 03:14:12.065   246   246 I zygote  : option[14]=-Xjitsaveprofilinginfo
01-09 03:14:12.065   246   246 I zygote  : option[15]=-agentlib:jdwp=transport=dt_android_adb,suspend=n,server=y
01-09 03:14:12.065   246   246 I zygote  : option[16]=-Xlockprofthreshold:500
01-09 03:14:12.065   246   246 I zygote  : option[17]=-Ximage-compiler-option
01-09 03:14:12.065   246   246 I zygote  : option[18]=--runtime-arg
01-09 03:14:12.065   246   246 I zygote  : option[19]=-Ximage-compiler-option
01-09 03:14:12.065   246   246 I zygote  : option[20]=-Xms64m
01-09 03:14:12.065   246   246 I zygote  : option[21]=-Ximage-compiler-option
01-09 03:14:12.065   246   246 I zygote  : option[22]=--runtime-arg
01-09 03:14:12.065   246   246 I zygote  : option[23]=-Ximage-compiler-option
01-09 03:14:12.065   246   246 I zygote  : option[24]=-Xmx64m
01-09 03:14:12.065   246   246 I zygote  : option[25]=-Ximage-compiler-option
01-09 03:14:12.065   246   246 I zygote  : option[26]=--compiler-filter=verify-at-runtime
01-09 03:14:12.066   246   246 I zygote  : option[27]=-Ximage-compiler-option
01-09 03:14:12.066   246   246 I zygote  : option[28]=--image-classes=/system/etc/preloaded-classes
01-09 03:14:12.066   246   246 I zygote  : option[29]=-Ximage-compiler-option
01-09 03:14:12.066   246   246 I zygote  : option[30]=--compiled-classes=/system/etc/compiled-classes
01-09 03:14:12.066   246   246 I zygote  : option[31]=-Xcompiler-option
01-09 03:14:12.066   246   246 I zygote  : option[32]=--runtime-arg
01-09 03:14:12.066   246   246 I zygote  : option[33]=-Xcompiler-option
01-09 03:14:12.066   246   246 I zygote  : option[34]=-Xms64m
01-09 03:14:12.066   246   246 I zygote  : option[35]=-Xcompiler-option
01-09 03:14:12.066   246   246 I zygote  : option[36]=--runtime-arg
01-09 03:14:12.066   246   246 I zygote  : option[37]=-Xcompiler-option
01-09 03:14:12.066   246   246 I zygote  : option[38]=-Xmx512m
01-09 03:14:12.066   246   246 I zygote  : option[39]=-Ximage-compiler-option
01-09 03:14:12.066   246   246 I zygote  : option[40]=--instruction-set-variant=cortex-a9
01-09 03:14:12.066   246   246 I zygote  : option[41]=-Xcompiler-option
01-09 03:14:12.066   246   246 I zygote  : option[42]=--instruction-set-variant=cortex-a9
01-09 03:14:12.066   246   246 I zygote  : option[43]=-Ximage-compiler-option
01-09 03:14:12.066   246   246 I zygote  : option[44]=--instruction-set-features=default
01-09 03:14:12.066   246   246 I zygote  : option[45]=-Xcompiler-option
01-09 03:14:12.066   246   246 I zygote  : option[46]=--instruction-set-features=default
01-09 03:14:12.066   246   246 I zygote  : option[47]=-Duser.locale=zh-Hans-CN
01-09 03:14:12.066   246   246 I zygote  : option[48]=--cpu-abilist=armeabi-v7a,armeabi
01-09 03:14:12.066   246   246 I zygote  : option[49]=-Xfingerprint:Android/autolink_6dq_t21fl/autolink_6dq_t21fl:8.0.0/1.0.0-rfp-rc4/20180205:eng/dev-keys
*/


bool ParsedOptions::DoParse(const RuntimeOptions& options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
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
    UsageMessage(stdout,"ART version %s %s\n",Runtime::GetVersion(),GetInstructionSetString(kRuntimeISA));
    Exit(0);
  } else if (args.Exists(M::BootClassPath)) {
    LOG(INFO) << "setting boot class path to " << *args.Get(M::BootClassPath);
  }

  if (args.GetOrDefault(M::UseJitCompilation) && args.GetOrDefault(M::Interpret)) {
    Usage("-Xusejit:true and -Xint cannot be specified together");
    Exit(0);
  }

  /**
  	autolink_6dq_t21fl:/ # echo $BOOTCLASSPATH                                     
	/system/framework/core-oj.jar:
	/system/framework/core-libart.jar:
	/system/framework/conscrypt.jar:
	/system/framework/okhttp.jar:
	/system/framework/legacy-test.jar:
	/system/framework/bouncycastle.jar:
	/system/framework/ext.jar:
	/system/framework/framework.jar:
	/system/framework/telephony-common.jar:
	/system/framework/voip-common.jar:
	/system/framework/ims-common.jar:
	/system/framework/apache-xml.jar:
	/system/framework/org.apache.http.legacy.boot.jar:
	/system/framework/android.hidl.base-V1.0-java.jar:
	/system/framework/android.hidl.manager-V1.0-java.jar
  */
  // Set a default boot class path if we didn't get an explicit one via command line.
  if (getenv("BOOTCLASSPATH") != nullptr) {
    args.SetIfMissing(M::BootClassPath, std::string(getenv("BOOTCLASSPATH")));
  }

  // Set a default class path if we didn't get an explicit one via command line.
  if (getenv("CLASSPATH") != nullptr) {
    args.SetIfMissing(M::ClassPath, std::string(getenv("CLASSPATH")));
  }

  // Default to number of processors minus one since the main GC thread also does work.
  args.SetIfMissing(M::ParallelGCThreads, gc::Heap::kDefaultEnableParallelGC ? static_cast<unsigned int>(sysconf(_SC_NPROCESSORS_CONF) - 1u) : 0u);

  // -verbose:
  {
    LogVerbosity *log_verbosity = args.Get(M::Verbose);
    if (log_verbosity != nullptr) {
      gLogVerbosity = *log_verbosity;
    }
  }

  MaybeOverrideVerbosity();

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
        background_collector_type_ = low_memory_mode_ ? gc::kCollectorTypeSS : gc::kCollectorTypeHomogeneousSpaceCompact;
      } else {
        background_collector_type_ = collector_type_;
      }
    }

    args.Set(M::BackgroundGc, BackgroundGcOption { background_collector_type_ });
  }

  // If a reference to the dalvik core.jar snuck(溜走) in, replace it with
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

  //这里将core_jar替换成core_libart_jar
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
  if (args.GetOrDefault(M::HeapGrowthLimit) <= 0u || args.GetOrDefault(M::HeapGrowthLimit) > args.GetOrDefault(M::MemoryMaximumSize)) {
    args.Set(M::HeapGrowthLimit, args.GetOrDefault(M::MemoryMaximumSize));
  }

  *runtime_options = std::move(args);
  return true;
}





//	#define RUNTIME_OPTIONS_KEY(Type, Name, ...) const RuntimeArgumentMap::Key<Type> RuntimeArgumentMap::Name {__VA_ARGS__}; 
//	

std::unique_ptr<RuntimeParser> ParsedOptions::MakeParser(bool ignore_unrecognized) {
  using M = RuntimeArgumentMap;

  //	art/cmdline/cmdline_parser.h:43:struct CmdlineParser 
  //	art/runtime/parsed_options.cc:54:using RuntimeParser = CmdlineParser<RuntimeArgumentMap, RuntimeArgumentMap::Key>;
  std::unique_ptr<RuntimeParser::Builder> parser_builder = std::unique_ptr<RuntimeParser::Builder>(new RuntimeParser::Builder());


  parser_builder->Define("-Xzygote").IntoKey(M::Zygote)
      .Define("-help").IntoKey(M::Help)
      .Define("-showversion").IntoKey(M::ShowVersion)
      .Define("-Xbootclasspath:_").WithType<std::string>().IntoKey(M::BootClassPath)
      .Define("-Xbootclasspath-locations:_").WithType<ParseStringList<':'>>()  // std::vector<std::string>, split by :
          .IntoKey(M::BootClassPathLocations)
      .Define({"-classpath _", "-cp _"})
          .WithType<std::string>()
          .IntoKey(M::ClassPath)
      .Define("-Ximage:_")
          .WithType<std::string>()
          .IntoKey(M::Image)
      .Define("-Xcheck:jni")
          .IntoKey(M::CheckJni)
      .Define("-Xjniopts:forcecopy")
          .IntoKey(M::JniOptsForceCopy)
      .Define({"-Xrunjdwp:_", "-agentlib:jdwp=_"})
          .WithType<JDWP::JdwpOptions>()
          .IntoKey(M::JdwpOptions)
      // TODO Re-enable -agentlib: once I have a good way to transform the values.
      // .Define("-agentlib:_")
      //     .WithType<std::vector<ti::Agent>>().AppendValues()
      //     .IntoKey(M::AgentLib)
      .Define("-agentpath:_")
          .WithType<std::list<ti::Agent>>().AppendValues()
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
      .Define("-XX:HeapTargetUtilization=_")
          .WithType<double>().WithRange(0.1, 0.9)
          .IntoKey(M::HeapTargetUtilization)
      .Define("-XX:ForegroundHeapGrowthMultiplier=_")
          .WithType<double>().WithRange(0.1, 1.0)
          .IntoKey(M::ForegroundHeapGrowthMultiplier)
      .Define("-XX:ParallelGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ParallelGCThreads)
      .Define("-XX:ConcGCThreads=_")
          .WithType<unsigned int>()
          .IntoKey(M::ConcGCThreads)
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
      .Define("-Xusejit:_")
          .WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(M::UseJitCompilation)
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
      .Define("-Xpatchoat:_")
          .WithType<std::string>()
          .IntoKey(M::PatchOat)
      .Define({"-Xrelocate", "-Xnorelocate"})
          .WithValues({true, false})
          .IntoKey(M::Relocate)
      .Define({"-Xdex2oat", "-Xnodex2oat"})
          .WithValues({true, false})
          .IntoKey(M::Dex2Oat)
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
      .Define("-Xusetombstonedtraces")
          .WithValue(true)
          .IntoKey(M::UseTombstonedTraces)
      .Define("-Xstacktracefile:_")
          .WithType<std::string>()
          .IntoKey(M::StackTraceFile)
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

  return std::unique_ptr<RuntimeParser>(new RuntimeParser(parser_builder->Build()));
}