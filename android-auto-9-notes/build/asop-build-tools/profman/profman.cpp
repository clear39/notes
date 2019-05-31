//  @ /work/workcodes/aosp-p9.x-auto-alpha/art/profman/profman.c

// See ProfileAssistant::ProcessingResult for return codes.
static int profman(int argc, char** argv) {
  ProfMan profman;

  // Parse arguments. Argument mistakes will lead to exit(EXIT_FAILURE) in UsageError.
  profman.ParseArgs(argc, argv);

  // Initialize MemMap for ZipArchive::OpenFromFd.
  MemMap::Init();

  if (profman.ShouldGenerateTestProfile()) {//false
    return profman.GenerateTestProfile();
  }
  if (profman.ShouldOnlyDumpProfile()) {//false
    return profman.DumpProfileInfo();
  }
  if (profman.ShouldOnlyDumpClassesAndMethods()) {//false
    return profman.DumpClassesAndMethods();
  }
  if (profman.ShouldCreateProfile()) {// true
    return profman.CreateProfile();   //执行这里
  }

  if (profman.ShouldCreateBootProfile()) {
    return profman.CreateBootProfile();
  }

  if (profman.ShouldCopyAndUpdateProfileKey()) {//false
    return profman.CopyAndUpdateProfileKey();
  }

  // Process profile information and assess if we need to do a profile guided compilation.
  // This operation involves I/O.
  return profman.ProcessProfiles();
}


void ProfMan::ParseArgs(int argc, char **argv) {
  original_argc = argc;
  original_argv = argv;

  Locks::Init();
  InitLogging(argv, Runtime::Abort);

  // Skip over the command name.
  argv++;
  argc--;

  if (argc == 0) {
    Usage("No arguments specified");
  }

  for (int i = 0; i < argc; ++i) {
    const StringPiece option(argv[i]);
    const bool log_options = false;
    if (log_options) {
      LOG(INFO) << "profman: option[" << i << "]=" << argv[i];
    }
    if (option == "--dump-only") {
      dump_only_ = true;
    } else if (option == "--dump-classes-and-methods") {
      dump_classes_and_methods_ = true;
    } else if (option.starts_with("--create-profile-from=")) {
      //  out/target/product/autolink_8q/dex_bootjars/system/framework/boot.prof
      create_profile_from_file_ = option.substr(strlen("--create-profile-from=")).ToString();
    } else if (option.starts_with("--dump-output-to-fd=")) {
      ParseUintOption(option, "--dump-output-to-fd", &dump_output_to_fd_, Usage);
    } else if (option == "--generate-boot-image-profile") {
      generate_boot_image_profile_ = true;
    } else if (option.starts_with("--boot-image-class-threshold=")) {
      ParseUintOption(option,
                      "--boot-image-class-threshold",
                      &boot_image_options_.image_class_theshold,
                      Usage);
    } else if (option.starts_with("--boot-image-clean-class-threshold=")) {
      ParseUintOption(option,
                      "--boot-image-clean-class-threshold",
                      &boot_image_options_.image_class_clean_theshold,
                      Usage);
    } else if (option.starts_with("--boot-image-sampled-method-threshold=")) {
      ParseUintOption(option,
                      "--boot-image-sampled-method-threshold",
                      &boot_image_options_.compiled_method_threshold,
                      Usage);
    } else if (option.starts_with("--profile-file=")) {
      profile_files_.push_back(option.substr(strlen("--profile-file=")).ToString());
    } else if (option.starts_with("--profile-file-fd=")) {
      ParseFdForCollection(option, "--profile-file-fd", &profile_files_fd_);
    } else if (option.starts_with("--reference-profile-file=")) {
      //  out/target/product/autolink_8q/dex_bootjars/system/framework/boot.prof
      reference_profile_file_ = option.substr(strlen("--reference-profile-file=")).ToString();
    } else if (option.starts_with("--reference-profile-file-fd=")) {
      ParseUintOption(option, "--reference-profile-file-fd", &reference_profile_file_fd_, Usage);
    } else if (option.starts_with("--dex-location=")) {
      //  /system/framework/framework.jar
      dex_locations_.push_back(option.substr(strlen("--dex-location=")).ToString());
    } else if (option.starts_with("--apk-fd=")) {
      ParseFdForCollection(option, "--apk-fd", &apks_fd_);
    } else if (option.starts_with("--apk=")) {
      //  out/target/common/obj/JAVA_LIBRARIES/framework_intermediates/javalib.jar
      apk_files_.push_back(option.substr(strlen("--apk=")).ToString());
    } else if (option.starts_with("--generate-test-profile=")) {
      test_profile_ = option.substr(strlen("--generate-test-profile=")).ToString();
    } else if (option.starts_with("--generate-test-profile-num-dex=")) {
      ParseUintOption(option,
                      "--generate-test-profile-num-dex",
                      &test_profile_num_dex_,
                      Usage);
    } else if (option.starts_with("--generate-test-profile-method-percentage")) {
      ParseUintOption(option,
                      "--generate-test-profile-method-percentage",
                      &test_profile_method_percerntage_,
                      Usage);
    } else if (option.starts_with("--generate-test-profile-class-percentage")) {
      ParseUintOption(option,
                      "--generate-test-profile-class-percentage",
                      &test_profile_class_percentage_,
                      Usage);
    } else if (option.starts_with("--generate-test-profile-seed=")) {
      ParseUintOption(option, "--generate-test-profile-seed", &test_profile_seed_, Usage);
    } else if (option.starts_with("--copy-and-update-profile-key")) {
      copy_and_update_profile_key_ = true;
    } else {
      Usage("Unknown argument '%s'", option.data());
    }
  }

  // Validate global consistency between file/fd options.
  if (!profile_files_.empty() && !profile_files_fd_.empty()) {
    Usage("Profile files should not be specified with both --profile-file-fd and --profile-file");
  }
  if (!reference_profile_file_.empty() && FdIsValid(reference_profile_file_fd_)) {
    Usage("Reference profile should not be specified with both "
          "--reference-profile-file-fd and --reference-profile-file");
  }
  if (!apk_files_.empty() && !apks_fd_.empty()) {
    Usage("APK files should not be specified with both --apk-fd and --apk");
  }
}




// Creates a profile from a human friendly textual representation.
// The expected input format is:
//   # Classes
//   Ljava/lang/Comparable;
//   Ljava/lang/Math;
//   # Methods with inline caches
//   LTestInline;->inlinePolymorphic(LSuper;)I+LSubA;,LSubB;,LSubC;
//   LTestInline;->noInlineCache(LSuper;)I
int ProfMan::CreateProfile() {
  // Validate parameters for this command.
  if (apk_files_.empty() && apks_fd_.empty()) {
    Usage("APK files must be specified");
  }
  if (dex_locations_.empty()) {
    Usage("DEX locations must be specified");
  }
  if (reference_profile_file_.empty() && !FdIsValid(reference_profile_file_fd_)) {
    Usage("Reference profile must be specified with --reference-profile-file or "
          "--reference-profile-file-fd");
  }
  if (!profile_files_.empty() || !profile_files_fd_.empty()) {
    Usage("Profile must be specified with --reference-profile-file or "
          "--reference-profile-file-fd");
  }
  // Open the profile output file if needed.
  int fd = OpenReferenceProfile();
  if (!FdIsValid(fd)) {
      return -1;
  }

  //  out/target/product/autolink_8q/dex_bootjars/system/framework/boot.prof
  
  // Read the user-specified list of classes and methods.
  std::unique_ptr<std::unordered_set<std::string>>
      user_lines(ReadCommentedInputFromFile<std::unordered_set<std::string>>(create_profile_from_file_.c_str(), nullptr));  // No post-processing.

  // Open the dex files to look up classes and methods.
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  OpenApkFilesFromLocations(&dex_files);

  // Process the lines one by one and add the successful ones to the profile.
  ProfileCompilationInfo info;

  for (const auto& line : *user_lines) {
    ProcessLine(dex_files, line, &info);
  }

  // Write the profile file.
  CHECK(info.Save(fd));
  if (close(fd) < 0) {
    PLOG(WARNING) << "Failed to close descriptor";
  }
  return 0;
}


static bool ProfMan::FdIsValid(int fd) {
  return fd != kInvalidFd;
}

int ProfMan::OpenReferenceProfile() const {
  int fd = reference_profile_file_fd_;  //  out/target/product/autolink_8q/dex_bootjars/system/framework/boot.prof
  if (!FdIsValid(fd)) {
    CHECK(!reference_profile_file_.empty());
    fd = open(reference_profile_file_.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
      LOG(ERROR) << "Cannot open " << reference_profile_file_ << strerror(errno);
      return kInvalidFd;
    }
  }
  return fd;
}