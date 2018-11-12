//      art/runtime/runtime.cc

//虚拟机创建入口
bool Runtime::Create(const RuntimeOptions& raw_options, bool ignore_unrecognized = JNI_FALSE) {
  RuntimeArgumentMap runtime_options;
  return ParseOptions(raw_options, ignore_unrecognized, &runtime_options) && Create(std::move(runtime_options));
}


bool Runtime::ParseOptions(const RuntimeOptions& raw_options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
  InitLogging(/* argv */ nullptr, Abort);  // Calls Locks::Init() as a side effect.
  bool parsed = ParsedOptions::Parse(raw_options, ignore_unrecognized, runtime_options);
  if (!parsed) {
    LOG(ERROR) << "Failed to parse options";
    return false;
  }
  return true;
}


// Callback to check whether it is safe to call Abort (e.g., to use a call to
// LOG(FATAL)).  It is only safe to call Abort if the runtime has been created,
// properly initialized, and has not shut down.
static bool IsSafeToCallAbort() NO_THREAD_SAFETY_ANALYSIS {
  Runtime* runtime = Runtime::Current();
  return runtime != nullptr && runtime->IsStarted() && !runtime->IsShuttingDownLocked();
}

void Locks::SetClientCallback(ClientCallback* safe_to_call_abort_cb) {
  safe_to_call_abort_callback.StoreRelease(safe_to_call_abort_cb); //原子函数指针
}

// Helper to allow checking shutdown while ignoring locking requirements.
bool Locks::IsSafeToCallAbortRacy() {
  Locks::ClientCallback* safe_to_call_abort_cb = safe_to_call_abort_callback.LoadAcquire();
  return safe_to_call_abort_cb != nullptr && safe_to_call_abort_cb();
}


bool Runtime::Create(RuntimeArgumentMap&& runtime_options) {
  // TODO: acquire a static mutex on Runtime to avoid racing.
  if (Runtime::instance_ != nullptr) {
    return false;
  }
  instance_ = new Runtime;
  //  art/runtime/base/mutex.cc
  Locks::SetClientCallback(IsSafeToCallAbort);//设置虚拟机回调函数
  if (!instance_->Init(std::move(runtime_options))) {
    // TODO: Currently deleting the instance will abort the runtime on destruction. Now This will
    // leak memory, instead. Fix the destructor. b/19100793.
    // delete instance_;
    instance_ = nullptr;
    return false;
  }
  return true;
}




Runtime::Runtime()
    : resolution_method_(nullptr),
      imt_conflict_method_(nullptr),
      imt_unimplemented_method_(nullptr),
      instruction_set_(kNone),
      compiler_callbacks_(nullptr),
      is_zygote_(false),
      must_relocate_(false),
      is_concurrent_gc_enabled_(true),
      is_explicit_gc_disabled_(false),
      dex2oat_enabled_(true),
      image_dex2oat_enabled_(true),
      default_stack_size_(0),
      heap_(nullptr),
      max_spins_before_thin_lock_inflation_(Monitor::kDefaultMaxSpinsBeforeThinLockInflation),
      monitor_list_(nullptr),
      monitor_pool_(nullptr),
      thread_list_(nullptr),
      intern_table_(nullptr),
      class_linker_(nullptr),
      signal_catcher_(nullptr),
      java_vm_(nullptr),
      fault_message_lock_("Fault message lock"),
      fault_message_(""),
      threads_being_born_(0),
      shutdown_cond_(new ConditionVariable("Runtime shutdown", *Locks::runtime_shutdown_lock_)),
      shutting_down_(false),
      shutting_down_started_(false),
      started_(false),
      finished_starting_(false),
      vfprintf_(nullptr),
      exit_(nullptr),
      abort_(nullptr),
      stats_enabled_(false),
      is_running_on_memory_tool_(RUNNING_ON_MEMORY_TOOL),
      instrumentation_(),
      main_thread_group_(nullptr),
      system_thread_group_(nullptr),
      system_class_loader_(nullptr),
      dump_gc_performance_on_shutdown_(false),
      preinitialization_transaction_(nullptr),
      verify_(verifier::VerifyMode::kNone),
      allow_dex_file_fallback_(true),
      target_sdk_version_(0),
      implicit_null_checks_(false),
      implicit_so_checks_(false),
      implicit_suspend_checks_(false),
      no_sig_chain_(false),
      force_native_bridge_(false),
      is_native_bridge_loaded_(false),
      is_native_debuggable_(false),
      is_java_debuggable_(false),
      zygote_max_failed_boots_(0),
      experimental_flags_(ExperimentalFlags::kNone),
      oat_file_manager_(nullptr),
      is_low_memory_mode_(false),
      safe_mode_(false),
      dump_native_stack_on_sig_quit_(true),
      pruned_dalvik_cache_(false),
      // Initially assume we perceive jank in case the process state is never updated.
      process_state_(kProcessStateJankPerceptible),
      zygote_no_threads_(false),
      cha_(nullptr) {


  CheckAsmSupportOffsetsAndSizes();
  std::fill(callee_save_methods_, callee_save_methods_ + arraysize(callee_save_methods_), 0u);
  interpreter::CheckInterpreterAsmConstants();
  callbacks_.reset(new RuntimeCallbacks());
  for (size_t i = 0; i <= static_cast<size_t>(DeoptimizationKind::kLast); ++i) {
    deoptimization_counts_[i] = 0u;
  }


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
  Monitor::Init(runtime_options.GetOrDefault(Opt::LockProfThreshold));

  boot_class_path_string_ = runtime_options.ReleaseOrDefault(Opt::BootClassPath);
  class_path_string_ = runtime_options.ReleaseOrDefault(Opt::ClassPath);
  properties_ = runtime_options.ReleaseOrDefault(Opt::PropertiesList);

  compiler_callbacks_ = runtime_options.GetOrDefault(Opt::CompilerCallbacksPtr);
  patchoat_executable_ = runtime_options.ReleaseOrDefault(Opt::PatchOat);
  must_relocate_ = runtime_options.GetOrDefault(Opt::Relocate);
  is_zygote_ = runtime_options.Exists(Opt::Zygote);
  is_explicit_gc_disabled_ = runtime_options.Exists(Opt::DisableExplicitGC);
  dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::Dex2Oat);
  image_dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::ImageDex2Oat);
  dump_native_stack_on_sig_quit_ = runtime_options.GetOrDefault(Opt::DumpNativeStackOnSigQuit);

  vfprintf_ = runtime_options.GetOrDefault(Opt::HookVfprintf);
  exit_ = runtime_options.GetOrDefault(Opt::HookExit);
  abort_ = runtime_options.GetOrDefault(Opt::HookAbort);

  default_stack_size_ = runtime_options.GetOrDefault(Opt::StackSize);
  use_tombstoned_traces_ = runtime_options.GetOrDefault(Opt::UseTombstonedTraces);
#if !defined(ART_TARGET_ANDROID)
  CHECK(!use_tombstoned_traces_)
      << "-Xusetombstonedtraces is only supported in an Android environment";
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

  max_spins_before_thin_lock_inflation_ =
      runtime_options.GetOrDefault(Opt::MaxSpinsBeforeThinLockInflation);

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

  plugins_ = runtime_options.ReleaseOrDefault(Opt::Plugins);
  agents_ = runtime_options.ReleaseOrDefault(Opt::AgentPath);
  // TODO Add back in -agentlib
  // for (auto lib : runtime_options.ReleaseOrDefault(Opt::AgentLib)) {
  //   agents_.push_back(lib);
  // }

  XGcOption xgc_option = runtime_options.GetOrDefault(Opt::GcOption);
  heap_ = new gc::Heap(runtime_options.GetOrDefault(Opt::MemoryInitialSize),
                       runtime_options.GetOrDefault(Opt::HeapGrowthLimit),
                       runtime_options.GetOrDefault(Opt::HeapMinFree),
                       runtime_options.GetOrDefault(Opt::HeapMaxFree),
                       runtime_options.GetOrDefault(Opt::HeapTargetUtilization),
                       runtime_options.GetOrDefault(Opt::ForegroundHeapGrowthMultiplier),
                       runtime_options.GetOrDefault(Opt::MemoryMaximumSize),
                       runtime_options.GetOrDefault(Opt::NonMovingSpaceCapacity),
                       runtime_options.GetOrDefault(Opt::Image),
                       runtime_options.GetOrDefault(Opt::ImageInstructionSet),
                       // Override the collector type to CC if the read barrier config.
                       kUseReadBarrier ? gc::kCollectorTypeCC : xgc_option.collector_type_,
                       kUseReadBarrier ? BackgroundGcOption(gc::kCollectorTypeCCBackground)
                                       : runtime_options.GetOrDefault(Opt::BackgroundGc),
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
  jit_arena_pool_.reset(
      new ArenaPool(/* use_malloc */ false, /* low_4gb */ false, "CompilerMetadata"));

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
  java_vm_ = JavaVMExt::Create(this, runtime_options, &error_msg);
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
  class_linker_ = new ClassLinker(intern_table_);
  cha_ = new ClassHierarchyAnalysis;
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
    for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
      Runtime::CalleeSaveType type = Runtime::CalleeSaveType(i);
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










//虚拟机启动接口分析
bool Runtime::Start() {
  VLOG(startup) << "Runtime::Start entering";

  CHECK(!no_sig_chain_) << "A started runtime should have sig chain enabled";

  // If a debug host build, disable ptrace restriction for debugging and test timeout thread dump.
  // Only 64-bit as prctl() may fail in 32 bit userspace on a 64-bit kernel.
#if defined(__linux__) && !defined(ART_TARGET_ANDROID) && defined(__x86_64__)
  if (kIsDebugBuild) {
    CHECK_EQ(prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY), 0);
  }
#endif

  // Restore main thread state to kNative as expected by native code.
  Thread* self = Thread::Current();

  self->TransitionFromRunnableToSuspended(kNative);

  started_ = true;

  if (!IsImageDex2OatEnabled() || !GetHeap()->HasBootImageSpace()) {
    ScopedObjectAccess soa(self);
    StackHandleScope<2> hs(soa.Self());

    auto class_class(hs.NewHandle<mirror::Class>(mirror::Class::GetJavaLangClass()));
    auto field_class(hs.NewHandle<mirror::Class>(mirror::Field::StaticClass()));

    class_linker_->EnsureInitialized(soa.Self(), class_class, true, true);
    // Field class is needed for register_java_net_InetAddress in libcore, b/28153851.
    class_linker_->EnsureInitialized(soa.Self(), field_class, true, true);
  }

  // InitNativeMethods needs to be after started_ so that the classes
  // it touches will have methods linked to the oat file if necessary.
  {
    ScopedTrace trace2("InitNativeMethods");
    InitNativeMethods();
  }

  // Initialize well known thread group values that may be accessed threads while attaching.
  InitThreadGroups(self);

  Thread::FinishStartup();

  // Create the JIT either if we have to use JIT compilation or save profiling info. This is
  // done after FinishStartup as the JIT pool needs Java thread peers, which require the main
  // ThreadGroup to exist.
  //
  // TODO(calin): We use the JIT class as a proxy for JIT compilation and for
  // recoding profiles. Maybe we should consider changing the name to be more clear it's
  // not only about compiling. b/28295073.
  if (jit_options_->UseJitCompilation() || jit_options_->GetSaveProfilingInfo()) {
    std::string error_msg;
    if (!IsZygote()) {
    // If we are the zygote then we need to wait until after forking to create the code cache
    // due to SELinux restrictions on r/w/x memory regions.
      CreateJit();
    } else if (jit_options_->UseJitCompilation()) {
      if (!jit::Jit::LoadCompilerLibrary(&error_msg)) {
        // Try to load compiler pre zygote to reduce PSS. b/27744947
        LOG(WARNING) << "Failed to load JIT compiler with error " << error_msg;
      }
    }
  }

  // Send the start phase event. We have to wait till here as this is when the main thread peer
  // has just been generated, important root clinits have been run and JNI is completely functional.
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kStart);
  }

  system_class_loader_ = CreateSystemClassLoader(this);

  if (!is_zygote_) {
    if (is_native_bridge_loaded_) {
      PreInitializeNativeBridge(".");
    }
    NativeBridgeAction action = force_native_bridge_
        ? NativeBridgeAction::kInitialize
        : NativeBridgeAction::kUnload;
    InitNonZygoteOrPostFork(self->GetJniEnv(),
                            /* is_system_server */ false,
                            action,
                            GetInstructionSetString(kRuntimeISA));
  }

  // Send the initialized phase event. Send it before starting daemons, as otherwise
  // sending thread events becomes complicated.
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kInit);
  }

  StartDaemonThreads();

  {
    ScopedObjectAccess soa(self);
    self->GetJniEnv()->locals.AssertEmpty();
  }

  VLOG(startup) << "Runtime::Start exiting";
  finished_starting_ = true;

  if (trace_config_.get() != nullptr && trace_config_->trace_file != "") {
    ScopedThreadStateChange tsc(self, kWaitingForMethodTracingStart);
    Trace::Start(trace_config_->trace_file.c_str(),
                 -1,
                 static_cast<int>(trace_config_->trace_file_size),
                 0,
                 trace_config_->trace_output_mode,
                 trace_config_->trace_mode,
                 0);
  }

  return true;
}