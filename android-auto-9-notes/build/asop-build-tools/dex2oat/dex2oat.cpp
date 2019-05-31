//  @/work/workcodes/aosp-p9.x-auto-alpha/art/dex2oat/dex2oat.cc
static dex2oat::ReturnCode Dex2oat(int argc, char** argv) {
  b13564922();

  TimingLogger timings("compiler", false, false);

  // Allocate `dex2oat` on the heap instead of on the stack, as Clang
  // might produce a stack frame too large for this function or for
  // functions inlining it (such as main), that would not fit the
  // requirements of the `-Wframe-larger-than` option.
  std::unique_ptr<Dex2Oat> dex2oat = std::make_unique<Dex2Oat>(&timings);

  // Parse arguments. Argument mistakes will lead to exit(EXIT_FAILURE) in UsageError.
  dex2oat->ParseArgs(argc, argv);

  // If needed, process profile information for profile guided compilation.
  // This operation involves I/O.
  if (dex2oat->UseProfile()) {
    if (!dex2oat->LoadProfile()) {
      LOG(ERROR) << "Failed to process profile file";
      return dex2oat::ReturnCode::kOther;
    }
  }

  art::MemMap::Init();  // For ZipEntry::ExtractToMemMap, and vdex.

  // Check early that the result of compilation can be written
  if (!dex2oat->OpenFile()) {
    return dex2oat::ReturnCode::kOther;
  }

  // Print the complete line when any of the following is true:
  //   1) Debug build
  //   2) Compiling an image
  //   3) Compiling with --host
  //   4) Compiling on the host (not a target build)
  // Otherwise, print a stripped command line.
  if (kIsDebugBuild || dex2oat->IsBootImage() || dex2oat->IsHost() || !kIsTargetBuild) {
    LOG(INFO) << CommandLine();
  } else {
    LOG(INFO) << StrippedCommandLine();
  }

  dex2oat::ReturnCode setup_code = dex2oat->Setup();
  if (setup_code != dex2oat::ReturnCode::kNoFailure) {
    dex2oat->EraseOutputFiles();
    return setup_code;
  }

  // TODO: Due to the cyclic dependencies, profile loading and verifying are
  // being done separately. Refactor and place the two next to each other.
  // If verification fails, we don't abort the compilation and instead log an
  // error.
  // TODO(b/62602192, b/65260586): We should consider aborting compilation when
  // the profile verification fails.
  // Note: If dex2oat fails, installd will remove the oat files causing the app
  // to fallback to apk with possible in-memory extraction. We want to avoid
  // that, and thus we're lenient towards profile corruptions.
  if (dex2oat->UseProfile()) {
    dex2oat->VerifyProfileData();
  }

  // Helps debugging on device. Can be used to determine which dalvikvm instance invoked a dex2oat
  // instance. Used by tools/bisection_search/bisection_search.py.
  VLOG(compiler) << "Running dex2oat (parent PID = " << getppid() << ")";

  dex2oat::ReturnCode result;
  if (dex2oat->IsImage()) {
    result = CompileImage(*dex2oat);
  } else {
    result = CompileApp(*dex2oat);
  }

  return result;
}




// Parse the arguments from the command line. In case of an unrecognized option or impossible
  // values/combinations, a usage error will be displayed and exit() is called. Thus, if the method
  // returns, arguments have been successfully parsed.
  void ParseArgs(int argc, char** argv) {
    original_argc = argc;
    original_argv = argv;

    Locks::Init();
    InitLogging(argv, Runtime::Abort);

    compiler_options_.reset(new CompilerOptions());

    using M = Dex2oatArgumentMap;
    std::string error_msg;
    std::unique_ptr<M> args_uptr = M::Parse(argc, const_cast<const char**>(argv), &error_msg);
    if (args_uptr == nullptr) {
      Usage("Failed to parse command line: %s", error_msg.c_str());
      UNREACHABLE();
    }

    M& args = *args_uptr;

    std::unique_ptr<ParserOptions> parser_options(new ParserOptions());

    AssignIfExists(args, M::CompactDexLevel, &compact_dex_level_);
    AssignIfExists(args, M::DexFiles, &dex_filenames_);
    AssignIfExists(args, M::DexLocations, &dex_locations_);
    AssignIfExists(args, M::OatFiles, &oat_filenames_);
    AssignIfExists(args, M::OatSymbols, &parser_options->oat_symbols);
    AssignIfExists(args, M::ImageFilenames, &image_filenames_);
    AssignIfExists(args, M::ZipFd, &zip_fd_);
    AssignIfExists(args, M::ZipLocation, &zip_location_);
    AssignIfExists(args, M::InputVdexFd, &input_vdex_fd_);
    AssignIfExists(args, M::OutputVdexFd, &output_vdex_fd_);
    AssignIfExists(args, M::InputVdex, &input_vdex_);
    AssignIfExists(args, M::OutputVdex, &output_vdex_);
    AssignIfExists(args, M::DmFd, &dm_fd_);
    AssignIfExists(args, M::DmFile, &dm_file_location_);
    AssignIfExists(args, M::OatFd, &oat_fd_);
    AssignIfExists(args, M::OatLocation, &oat_location_);
    AssignIfExists(args, M::Watchdog, &parser_options->watch_dog_enabled);
    AssignIfExists(args, M::WatchdogTimeout, &parser_options->watch_dog_timeout_in_ms);
    AssignIfExists(args, M::Threads, &thread_count_);
    AssignIfExists(args, M::ImageClasses, &image_classes_filename_);
    AssignIfExists(args, M::ImageClassesZip, &image_classes_zip_filename_);
    AssignIfExists(args, M::CompiledClasses, &compiled_classes_filename_);
    AssignIfExists(args, M::CompiledClassesZip, &compiled_classes_zip_filename_);
    AssignIfExists(args, M::CompiledMethods, &compiled_methods_filename_);
    AssignIfExists(args, M::CompiledMethodsZip, &compiled_methods_zip_filename_);
    AssignIfExists(args, M::Passes, &passes_to_run_filename_);
    AssignIfExists(args, M::BootImage, &parser_options->boot_image_filename);
    AssignIfExists(args, M::AndroidRoot, &android_root_);
    AssignIfExists(args, M::Profile, &profile_file_);
    AssignIfExists(args, M::ProfileFd, &profile_file_fd_);
    AssignIfExists(args, M::RuntimeOptions, &runtime_args_);
    AssignIfExists(args, M::SwapFile, &swap_file_name_);
    AssignIfExists(args, M::SwapFileFd, &swap_fd_);
    AssignIfExists(args, M::SwapDexSizeThreshold, &min_dex_file_cumulative_size_for_swap_);
    AssignIfExists(args, M::SwapDexCountThreshold, &min_dex_files_for_swap_);
    AssignIfExists(args, M::VeryLargeAppThreshold, &very_large_threshold_);
    AssignIfExists(args, M::AppImageFile, &app_image_file_name_);
    AssignIfExists(args, M::AppImageFileFd, &app_image_fd_);
    AssignIfExists(args, M::NoInlineFrom, &no_inline_from_string_);
    AssignIfExists(args, M::ClasspathDir, &classpath_dir_);
    AssignIfExists(args, M::DirtyImageObjects, &dirty_image_objects_filename_);
    AssignIfExists(args, M::ImageFormat, &image_storage_mode_);
    AssignIfExists(args, M::CompilationReason, &compilation_reason_);

    AssignIfExists(args, M::Backend, &compiler_kind_);
    parser_options->requested_specific_compiler = args.Exists(M::Backend);

    AssignIfExists(args, M::TargetInstructionSet, &instruction_set_);
    // arm actually means thumb2.
    if (instruction_set_ == InstructionSet::kArm) {
      instruction_set_ = InstructionSet::kThumb2;
    }

    AssignTrueIfExists(args, M::Host, &is_host_);
    AssignTrueIfExists(args, M::AvoidStoringInvocation, &avoid_storing_invocation_);
    AssignTrueIfExists(args, M::MultiImage, &multi_image_);
    AssignIfExists(args, M::CopyDexFiles, &copy_dex_files_);

    if (args.Exists(M::ForceDeterminism)) {
      if (!SupportsDeterministicCompilation()) {
        Usage("Option --force-determinism requires read barriers or a CMS/MS garbage collector");
      }
      force_determinism_ = true;
    }

    if (args.Exists(M::Base)) {
      ParseBase(*args.Get(M::Base));
    }
    if (args.Exists(M::TargetInstructionSetVariant)) {
      ParseInstructionSetVariant(*args.Get(M::TargetInstructionSetVariant), parser_options.get());
    }
    if (args.Exists(M::TargetInstructionSetFeatures)) {
      ParseInstructionSetFeatures(*args.Get(M::TargetInstructionSetFeatures), parser_options.get());
    }
    if (args.Exists(M::ClassLoaderContext)) {
      std::string class_loader_context_arg = *args.Get(M::ClassLoaderContext);
      class_loader_context_ = ClassLoaderContext::Create(class_loader_context_arg);
      if (class_loader_context_ == nullptr) {
        Usage("Option --class-loader-context has an incorrect format: %s",
              class_loader_context_arg.c_str());
      }
      if (args.Exists(M::StoredClassLoaderContext)) {
        const std::string stored_context_arg = *args.Get(M::StoredClassLoaderContext);
        stored_class_loader_context_ = ClassLoaderContext::Create(stored_context_arg);
        if (stored_class_loader_context_ == nullptr) {
          Usage("Option --stored-class-loader-context has an incorrect format: %s",
                stored_context_arg.c_str());
        } else if (!class_loader_context_->VerifyClassLoaderContextMatch(
            stored_context_arg,
            /*verify_names*/ false,
            /*verify_checksums*/ false)) {
          Usage(
              "Option --stored-class-loader-context '%s' mismatches --class-loader-context '%s'",
              stored_context_arg.c_str(),
              class_loader_context_arg.c_str());
        }
      }
    } else if (args.Exists(M::StoredClassLoaderContext)) {
      Usage("Option --stored-class-loader-context should only be used if "
            "--class-loader-context is also specified");
    }

    if (!ReadCompilerOptions(args, compiler_options_.get(), &error_msg)) {
      Usage(error_msg.c_str());
    }

    ProcessOptions(parser_options.get());

    // Insert some compiler things.
    InsertCompileOptions(argc, argv);
  }