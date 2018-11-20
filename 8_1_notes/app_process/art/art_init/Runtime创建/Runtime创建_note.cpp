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
  //  ParseOptions函数分析 看 Runtime::PareseOptions_note.cpp文件
  return ParseOptions(raw_options, ignore_unrecognized, &runtime_options) && Create(std::move(runtime_options));
}


bool Runtime::Create(RuntimeArgumentMap&& runtime_options) {
  // TODO: acquire a static mutex on Runtime to avoid racing.
  if (Runtime::instance_ != nullptr) {
    return false;
  }
  instance_ = new Runtime;
  Locks::SetClientCallback(IsSafeToCallAbort);//  @art/runtime/base/mutex.cc
  if (!instance_->Init(std::move(runtime_options))) {
    // TODO: Currently deleting the instance will abort the runtime on destruction. Now This will
    // leak memory, instead. Fix the destructor. b/19100793.
    // delete instance_;
    instance_ = nullptr;
    return false;
  }
  return true;
}


bool Runtime::Init(RuntimeArgumentMap&& runtime_options_in) {
  // (b/30160149): protect subprocesses from modifications to LD_LIBRARY_PATH, etc.
  // Take a snapshot of the environment at the time the runtime was created, for use by Exec, etc.
  env_snapshot_.TakeSnapshot();

  RuntimeArgumentMap runtime_options(std::move(runtime_options_in));
  ScopedTrace trace(__FUNCTION__);
  CHECK_EQ(sysconf(_SC_PAGE_SIZE), kPageSize);

  MemMap::Init();

  // Try to reserve a dedicated fault page. This is allocated for clobbered registers and sentinels.
  // If we cannot reserve it, log a warning.
  // Note: We allocate this first to have a good chance of grabbing the page. The address (0xebad..)
  //       is out-of-the-way enough that it should not collide with boot image mapping.
  // Note: Don't request an error message. That will lead to a maps dump in the case of failure,
  //       leading to logspam.
  {
    constexpr uintptr_t kSentinelAddr = RoundDown(static_cast<uintptr_t>(Context::kBadGprBase), kPageSize);
    protected_fault_page_.reset(MemMap::MapAnonymous("Sentinel fault page",
                                                     reinterpret_cast<uint8_t*>(kSentinelAddr),
                                                     kPageSize,
                                                     PROT_NONE,
                                                     /* low_4g */ true,
                                                     /* reuse */ false,
                                                     /* error_msg */ nullptr));
    if (protected_fault_page_ == nullptr) {
      LOG(WARNING) << "Could not reserve sentinel fault page";
    } else if (reinterpret_cast<uintptr_t>(protected_fault_page_->Begin()) != kSentinelAddr) {
      LOG(WARNING) << "Could not reserve sentinel fault page at the right address.";
      protected_fault_page_.reset();
    }
  }

  using Opt = RuntimeArgumentMap;
  VLOG(startup) << "Runtime::Init -verbose:startup enabled";

  QuasiAtomic::Startup();

  oat_file_manager_ = new OatFileManager;

  Thread::SetSensitiveThreadHook(runtime_options.GetOrDefault(Opt::HookIsSensitiveThread));
  Monitor::Init(runtime_options.GetOrDefault(Opt::LockProfThreshold),runtime_options.GetOrDefault(Opt::StackDumpLockProfThreshold));

  boot_class_path_string_ = runtime_options.ReleaseOrDefault(Opt::BootClassPath);
  class_path_string_ = runtime_options.ReleaseOrDefault(Opt::ClassPath);
  properties_ = runtime_options.ReleaseOrDefault(Opt::PropertiesList);

  compiler_callbacks_ = runtime_options.GetOrDefault(Opt::CompilerCallbacksPtr);
  patchoat_executable_ = runtime_options.ReleaseOrDefault(Opt::PatchOat);
  must_relocate_ = runtime_options.GetOrDefault(Opt::Relocate);
  is_zygote_ = runtime_options.Exists(Opt::Zygote);
  is_explicit_gc_disabled_ = runtime_options.Exists(Opt::DisableExplicitGC);
  dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::Dex2Oat);
  image_dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::ImageDex2Oat);//没有该参数
  dump_native_stack_on_sig_quit_ = runtime_options.GetOrDefault(Opt::DumpNativeStackOnSigQuit);

  vfprintf_ = runtime_options.GetOrDefault(Opt::HookVfprintf);
  exit_ = runtime_options.GetOrDefault(Opt::HookExit);
  abort_ = runtime_options.GetOrDefault(Opt::HookAbort);

  default_stack_size_ = runtime_options.GetOrDefault(Opt::StackSize);
  use_tombstoned_traces_ = runtime_options.GetOrDefault(Opt::UseTombstonedTraces);
#if !defined(ART_TARGET_ANDROID)
  CHECK(!use_tombstoned_traces_) << "-Xusetombstonedtraces is only supported in an Android environment";
#endif
  stack_trace_file_ = runtime_options.ReleaseOrDefault(Opt::StackTraceFile);

  compiler_executable_ = runtime_options.ReleaseOrDefault(Opt::Compiler);
  compiler_options_ = runtime_options.ReleaseOrDefault(Opt::CompilerOptions);
  for (StringPiece option : Runtime::Current()->GetCompilerOptions()) {
    if (option.starts_with("--debuggable")) {
      SetJavaDebuggable(true);
      break;
    }
  }
  image_compiler_options_ = runtime_options.ReleaseOrDefault(Opt::ImageCompilerOptions);
  image_location_ = runtime_options.GetOrDefault(Opt::Image);

  max_spins_before_thin_lock_inflation_ = runtime_options.GetOrDefault(Opt::MaxSpinsBeforeThinLockInflation);

  monitor_list_ = new MonitorList;
  monitor_pool_ = MonitorPool::Create();
  thread_list_ = new ThreadList(runtime_options.GetOrDefault(Opt::ThreadSuspendTimeout));
  intern_table_ = new InternTable;

  verify_ = runtime_options.GetOrDefault(Opt::Verify);
  allow_dex_file_fallback_ = !runtime_options.Exists(Opt::NoDexFileFallback);

  no_sig_chain_ = runtime_options.Exists(Opt::NoSigChain);
  force_native_bridge_ = runtime_options.Exists(Opt::ForceNativeBridge);

  Split(runtime_options.GetOrDefault(Opt::CpuAbiList), ',', &cpu_abilist_);

  fingerprint_ = runtime_options.ReleaseOrDefault(Opt::Fingerprint);

  if (runtime_options.GetOrDefault(Opt::Interpret)) {
    GetInstrumentation()->ForceInterpretOnly();
  }

  zygote_max_failed_boots_ = runtime_options.GetOrDefault(Opt::ZygoteMaxFailedBoots);
  experimental_flags_ = runtime_options.GetOrDefault(Opt::Experimental);
  is_low_memory_mode_ = runtime_options.Exists(Opt::LowMemoryMode);
  madvise_random_access_ = runtime_options.GetOrDefault(Opt::MadviseRandomAccess);

  plugins_ = runtime_options.ReleaseOrDefault(Opt::Plugins);
  agents_ = runtime_options.ReleaseOrDefault(Opt::AgentPath);
  // TODO Add back in -agentlib
  // for (auto lib : runtime_options.ReleaseOrDefault(Opt::AgentLib)) {
  //   agents_.push_back(lib);
  // }

  float foreground_heap_growth_multiplier;
  if (is_low_memory_mode_ && !runtime_options.Exists(Opt::ForegroundHeapGrowthMultiplier)) {
    // If low memory mode, use 1.0 as the multiplier by default.
    foreground_heap_growth_multiplier = 1.0f;
  } else {
    foreground_heap_growth_multiplier =  runtime_options.GetOrDefault(Opt::ForegroundHeapGrowthMultiplier) + kExtraDefaultHeapGrowthMultiplier;
  }
  XGcOption xgc_option = runtime_options.GetOrDefault(Opt::GcOption);
  heap_ = new gc::Heap(runtime_options.GetOrDefault(Opt::MemoryInitialSize),
                       runtime_options.GetOrDefault(Opt::HeapGrowthLimit),
                       runtime_options.GetOrDefault(Opt::HeapMinFree),
                       runtime_options.GetOrDefault(Opt::HeapMaxFree),
                       runtime_options.GetOrDefault(Opt::HeapTargetUtilization),
                       foreground_heap_growth_multiplier,
                       runtime_options.GetOrDefault(Opt::MemoryMaximumSize),
                       runtime_options.GetOrDefault(Opt::NonMovingSpaceCapacity),
                       runtime_options.GetOrDefault(Opt::Image),
                       runtime_options.GetOrDefault(Opt::ImageInstructionSet),
                       // Override the collector type to CC if the read barrier config.
                       kUseReadBarrier ? gc::kCollectorTypeCC : xgc_option.collector_type_,
                       kUseReadBarrier ? BackgroundGcOption(gc::kCollectorTypeCCBackground)  : runtime_options.GetOrDefault(Opt::BackgroundGc),
                       runtime_options.GetOrDefault(Opt::LargeObjectSpace),
                       runtime_options.GetOrDefault(Opt::LargeObjectThreshold),
                       runtime_options.GetOrDefault(Opt::ParallelGCThreads),
                       runtime_options.GetOrDefault(Opt::ConcGCThreads),
                       runtime_options.Exists(Opt::LowMemoryMode),
                       runtime_options.GetOrDefault(Opt::LongPauseLogThreshold),
                       runtime_options.GetOrDefault(Opt::LongGCLogThreshold),
                       runtime_options.Exists(Opt::IgnoreMaxFootprint),
                       runtime_options.GetOrDefault(Opt::UseTLAB),
                       xgc_option.verify_pre_gc_heap_,
                       xgc_option.verify_pre_sweeping_heap_,
                       xgc_option.verify_post_gc_heap_,
                       xgc_option.verify_pre_gc_rosalloc_,
                       xgc_option.verify_pre_sweeping_rosalloc_,
                       xgc_option.verify_post_gc_rosalloc_,
                       xgc_option.gcstress_,
                       xgc_option.measure_,
                       runtime_options.GetOrDefault(Opt::EnableHSpaceCompactForOOM),
                       runtime_options.GetOrDefault(Opt::HSpaceCompactForOOMMinIntervalsMs));

  if (!heap_->HasBootImageSpace() && !allow_dex_file_fallback_) {
    LOG(ERROR) << "Dex file fallback disabled, cannot continue without image.";
    return false;
  }

  dump_gc_performance_on_shutdown_ = runtime_options.Exists(Opt::DumpGCPerformanceOnShutdown);

  if (runtime_options.Exists(Opt::JdwpOptions)) {
    Dbg::ConfigureJdwp(runtime_options.GetOrDefault(Opt::JdwpOptions));
  }
  callbacks_->AddThreadLifecycleCallback(Dbg::GetThreadLifecycleCallback());
  callbacks_->AddClassLoadCallback(Dbg::GetClassLoadCallback());

  jit_options_.reset(jit::JitOptions::CreateFromRuntimeArguments(runtime_options));
  if (IsAotCompiler()) {
    // If we are already the compiler at this point, we must be dex2oat. Don't create the jit in
    // this case.
    // If runtime_options doesn't have UseJIT set to true then CreateFromRuntimeArguments returns
    // null and we don't create the jit.
    jit_options_->SetUseJitCompilation(false);
    jit_options_->SetSaveProfilingInfo(false);
  }

  // Use MemMap arena pool for jit, malloc otherwise. Malloc arenas are faster to allocate but
  // can't be trimmed as easily.
  const bool use_malloc = IsAotCompiler();
  arena_pool_.reset(new ArenaPool(use_malloc, /* low_4gb */ false));
  jit_arena_pool_.reset(new ArenaPool(/* use_malloc */ false, /* low_4gb */ false, "CompilerMetadata"));

  if (IsAotCompiler() && Is64BitInstructionSet(kRuntimeISA)) {
    // 4gb, no malloc. Explanation in header.
    low_4gb_arena_pool_.reset(new ArenaPool(/* use_malloc */ false, /* low_4gb */ true));
  }
  linear_alloc_.reset(CreateLinearAlloc());

  BlockSignals();
  InitPlatformSignalHandlers();

  // Change the implicit checks flags based on runtime architecture.
  switch (kRuntimeISA) {
    case kArm:
    case kThumb2:
    case kX86:
    case kArm64:
    case kX86_64:
    case kMips:
    case kMips64:
      implicit_null_checks_ = true;
      // Installing stack protection does not play well with valgrind.
      implicit_so_checks_ = !(RUNNING_ON_MEMORY_TOOL && kMemoryToolIsValgrind);
      break;
    default:
      // Keep the defaults.
      break;
  }

  if (!no_sig_chain_) {
    // Dex2Oat's Runtime does not need the signal chain or the fault handler.
    if (implicit_null_checks_ || implicit_so_checks_ || implicit_suspend_checks_) {
      fault_manager.Init();

      // These need to be in a specific order.  The null point check handler must be
      // after the suspend check and stack overflow check handlers.
      //
      // Note: the instances attach themselves to the fault manager and are handled by it. The manager
      //       will delete the instance on Shutdown().
      if (implicit_suspend_checks_) {
        new SuspensionHandler(&fault_manager);
      }

      if (implicit_so_checks_) {
        new StackOverflowHandler(&fault_manager);
      }

      if (implicit_null_checks_) {
        new NullPointerHandler(&fault_manager);
      }

      if (kEnableJavaStackTraceHandler) {
        new JavaStackTraceHandler(&fault_manager);
      }
    }
  }

  std::string error_msg;
  java_vm_ = JavaVMExt::Create(this, runtime_options, &error_msg);//  @art/runtime/java_vm_ext.cc
  if (java_vm_.get() == nullptr) {
    LOG(ERROR) << "Could not initialize JavaVMExt: " << error_msg;
    return false;
  }

  // Add the JniEnv handler.
  // TODO Refactor this stuff.
  java_vm_->AddEnvironmentHook(JNIEnvExt::GetEnvHandler);

  Thread::Startup();

  // ClassLinker needs an attached thread, but we can't fully attach a thread without creating
  // objects. We can't supply a thread group yet; it will be fixed later. Since we are the main
  // thread, we do not get a java peer.
  Thread* self = Thread::Attach("main", false, nullptr, false);
  CHECK_EQ(self->GetThreadId(), ThreadList::kMainThreadId);
  CHECK(self != nullptr);

  self->SetCanCallIntoJava(!IsAotCompiler());

  // Set us to runnable so tools using a runtime can allocate and GC by default
  self->TransitionFromSuspendedToRunnable();

  // Now we're attached, we can take the heap locks and validate the heap.
  GetHeap()->EnableObjectValidation();

  CHECK_GE(GetHeap()->GetContinuousSpaces().size(), 1U);
  if (UNLIKELY(IsAotCompiler())) {
    class_linker_ = new AotClassLinker(intern_table_);
  } else {
    class_linker_ = new ClassLinker(intern_table_);
  }
  if (GetHeap()->HasBootImageSpace()) {
    bool result = class_linker_->InitFromBootImage(&error_msg);
    if (!result) {
      LOG(ERROR) << "Could not initialize from image: " << error_msg;
      return false;
    }
    if (kIsDebugBuild) {
      for (auto image_space : GetHeap()->GetBootImageSpaces()) {
        image_space->VerifyImageAllocations();
      }
    }
    if (boot_class_path_string_.empty()) {
      // The bootclasspath is not explicitly specified: construct it from the loaded dex files.
      const std::vector<const DexFile*>& boot_class_path = GetClassLinker()->GetBootClassPath();
      std::vector<std::string> dex_locations;
      dex_locations.reserve(boot_class_path.size());
      for (const DexFile* dex_file : boot_class_path) {
        dex_locations.push_back(dex_file->GetLocation());
      }
      boot_class_path_string_ = android::base::Join(dex_locations, ':');
    }
    {
      ScopedTrace trace2("AddImageStringsToTable");
      GetInternTable()->AddImagesStringsToTable(heap_->GetBootImageSpaces());
    }
    if (IsJavaDebuggable()) {
      // Now that we have loaded the boot image, deoptimize its methods if we are running
      // debuggable, as the code may have been compiled non-debuggable.
      DeoptimizeBootImage();
    }
  } else {
    std::vector<std::string> dex_filenames;
    Split(boot_class_path_string_, ':', &dex_filenames);

    std::vector<std::string> dex_locations;
    if (!runtime_options.Exists(Opt::BootClassPathLocations)) {
      dex_locations = dex_filenames;
    } else {
      dex_locations = runtime_options.GetOrDefault(Opt::BootClassPathLocations);
      CHECK_EQ(dex_filenames.size(), dex_locations.size());
    }

    std::vector<std::unique_ptr<const DexFile>> boot_class_path;
    if (runtime_options.Exists(Opt::BootClassPathDexList)) {
      boot_class_path.swap(*runtime_options.GetOrDefault(Opt::BootClassPathDexList));
    } else {
      OpenDexFiles(dex_filenames,
                   dex_locations,
                   runtime_options.GetOrDefault(Opt::Image),
                   &boot_class_path);
    }
    instruction_set_ = runtime_options.GetOrDefault(Opt::ImageInstructionSet);
    if (!class_linker_->InitWithoutImage(std::move(boot_class_path), &error_msg)) {
      LOG(ERROR) << "Could not initialize without image: " << error_msg;
      return false;
    }

    // TODO: Should we move the following to InitWithoutImage?
    SetInstructionSet(instruction_set_);
    for (uint32_t i = 0; i < kCalleeSaveSize; i++) {
      CalleeSaveType type = CalleeSaveType(i);
      if (!HasCalleeSaveMethod(type)) {
        SetCalleeSaveMethod(CreateCalleeSaveMethod(), type);
      }
    }
  }

  CHECK(class_linker_ != nullptr);

  verifier::MethodVerifier::Init();

  if (runtime_options.Exists(Opt::MethodTrace)) {
    trace_config_.reset(new TraceConfig());
    trace_config_->trace_file = runtime_options.ReleaseOrDefault(Opt::MethodTraceFile);
    trace_config_->trace_file_size = runtime_options.ReleaseOrDefault(Opt::MethodTraceFileSize);
    trace_config_->trace_mode = Trace::TraceMode::kMethodTracing;
    trace_config_->trace_output_mode = runtime_options.Exists(Opt::MethodTraceStreaming) ?
        Trace::TraceOutputMode::kStreaming :
        Trace::TraceOutputMode::kFile;
  }

  // TODO: move this to just be an Trace::Start argument
  Trace::SetDefaultClockSource(runtime_options.GetOrDefault(Opt::ProfileClock));

  // Pre-allocate an OutOfMemoryError for the double-OOME case.
  self->ThrowNewException("Ljava/lang/OutOfMemoryError;",
                          "OutOfMemoryError thrown while trying to throw OutOfMemoryError; "
                          "no stack trace available");
  pre_allocated_OutOfMemoryError_ = GcRoot<mirror::Throwable>(self->GetException());
  self->ClearException();

  // Pre-allocate a NoClassDefFoundError for the common case of failing to find a system class
  // ahead of checking the application's class loader.
  self->ThrowNewException("Ljava/lang/NoClassDefFoundError;",
                          "Class not found using the boot class loader; no stack trace available");
  pre_allocated_NoClassDefFoundError_ = GcRoot<mirror::Throwable>(self->GetException());
  self->ClearException();

  // Runtime initialization is largely done now.
  // We load plugins first since that can modify the runtime state slightly.
  // Load all plugins
  for (auto& plugin : plugins_) {
    std::string err;
    if (!plugin.Load(&err)) {
      LOG(FATAL) << plugin << " failed to load: " << err;
    }
  }

  // Look for a native bridge.
  //
  // The intended flow here is, in the case of a running system:
  //
  // Runtime::Init() (zygote):
  //   LoadNativeBridge -> dlopen from cmd line parameter.
  //  |
  //  V
  // Runtime::Start() (zygote):
  //   No-op wrt native bridge.
  //  |
  //  | start app
  //  V
  // DidForkFromZygote(action)
  //   action = kUnload -> dlclose native bridge.
  //   action = kInitialize -> initialize library
  //
  //
  // The intended flow here is, in the case of a simple dalvikvm call:
  //
  // Runtime::Init():
  //   LoadNativeBridge -> dlopen from cmd line parameter.
  //  |
  //  V
  // Runtime::Start():
  //   DidForkFromZygote(kInitialize) -> try to initialize any native bridge given.
  //   No-op wrt native bridge.
  {
    std::string native_bridge_file_name = runtime_options.ReleaseOrDefault(Opt::NativeBridge);
    is_native_bridge_loaded_ = LoadNativeBridge(native_bridge_file_name);
  }

  // Startup agents
  // TODO Maybe we should start a new thread to run these on. Investigate RI behavior more.
  for (auto& agent : agents_) {
    // TODO Check err
    int res = 0;
    std::string err = "";
    ti::Agent::LoadError result = agent.Load(&res, &err);
    if (result == ti::Agent::kInitializationError) {
      LOG(FATAL) << "Unable to initialize agent!";
    } else if (result != ti::Agent::kNoError) {
      LOG(ERROR) << "Unable to load an agent: " << err;
    }
  }
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kInitialAgents);
  }

  VLOG(startup) << "Runtime::Init exiting";

  return true;
}
















