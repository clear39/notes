//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/oat_file_assistant.cc
OatFileAssistant::OatFileAssistant(const char* dex_location,
                                   const InstructionSet isa,
                                   bool load_executable  /** = false */,
                                   bool only_load_system_executable  /** = false */ )
    : OatFileAssistant(dex_location,
                       isa,
                       load_executable,
                       only_load_system_executable,
                       -1 /* vdex_fd */,
                       -1 /* oat_fd */,
                       -1 /* zip_fd */) {}



OatFileAssistant::OatFileAssistant(const char* dex_location,
                                   const InstructionSet isa,
                                   bool load_executable,
                                   bool only_load_system_executable,
                                   int vdex_fd,
                                   int oat_fd,
                                   int zip_fd)
    : isa_(isa),
      load_executable_(load_executable),
      only_load_system_executable_(only_load_system_executable),
      odex_(this, /*is_oat_location*/ false),
      oat_(this, /*is_oat_location*/ true),
      zip_fd_(zip_fd) {
  CHECK(dex_location != nullptr) << "OatFileAssistant: null dex location";


    /***当 zip_fd < 0 时，oat_fd 和 vdex_fd 必须小于等于 0*/ 
  if (zip_fd < 0) {
    // CHECK_LE @ system/core/base/include/android-base/logging.h
    CHECK_LE(oat_fd, 0) << "zip_fd must be provided with valid oat_fd. zip_fd=" << zip_fd  << " oat_fd=" << oat_fd;
    CHECK_LE(vdex_fd, 0) << "zip_fd must be provided with valid vdex_fd. zip_fd=" << zip_fd << " vdex_fd=" << vdex_fd;;
  }

  dex_location_.assign(dex_location);  //std::string dex_location_;

  if (load_executable_ && isa != kRuntimeISA) {
    LOG(WARNING) << "OatFileAssistant: Load executable specified, " << "but isa is not kRuntimeISA. Will not attempt to load executable.";
    load_executable_ = false;
  }

  // Get the odex filename.
  std::string error_msg;
  std::string odex_file_name;
  //   获取 odex_file_name 路径值 ： /system/framework/oat/arm64/services.odex
  if (DexLocationToOdexFilename(dex_location_, isa_, &odex_file_name, &error_msg)) {
    odex_.Reset(odex_file_name, UseFdToReadFiles(), zip_fd, vdex_fd, oat_fd); //注意这里保存到了 odex_中
  } else {
    LOG(WARNING) << "Failed to determine odex file name: " << error_msg;
  }

  // 判断 zip_fd_ 是否大于 0
  if (!UseFdToReadFiles()) {
    // Get the oat filename.
    std::string oat_file_name;
    /***
     * DexLocationToOatFilename 主要针对apk 或者 jar ，得到 类似于 /data/dalvik-cache/arm64/system@framework@services.jar@classes.dex 目录
    */
    if (DexLocationToOatFilename(dex_location_, isa_, &oat_file_name, &error_msg)) {
      oat_.Reset(oat_file_name, false /* use_fd */); //这里保存到 oat_ 中
    } else {
      LOG(WARNING) << "Failed to determine oat file name for dex location " << dex_location_ << ": " << error_msg;
    }
  }

  // Check if the dex directory is writable.
  // This will be needed in most uses of OatFileAssistant and so it's OK to
  // compute it eagerly. (the only use which will not make use of it is
  // OatFileAssistant::GetStatusDump())
  size_t pos = dex_location_.rfind('/');
  if (pos == std::string::npos) {
    LOG(WARNING) << "Failed to determine dex file parent directory: " << dex_location_;
  } else if (!UseFdToReadFiles()) {
    // We cannot test for parent access when using file descriptors. That's ok
    // because in this case we will always pick the odex file anyway.
    std::string parent = dex_location_.substr(0, pos);
    if (access(parent.c_str(), W_OK) == 0) {
      dex_parent_writable_ = true;
    } else {
      VLOG(oat) << "Dex parent of " << dex_location_ << " is not writable: " << strerror(errno);
    }
  }
}



OatFileAssistant::DexOptNeeded OatFileAssistant::OatFileInfo::GetDexOptNeeded(
    CompilerFilter::Filter target,
    bool profile_changed,
    bool downgrade,
    ClassLoaderContext* context) {

  bool compilation_desired = CompilerFilter::IsAotCompilationEnabled(target);  // true
  //    @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/oat_file_assistant.cc
  bool filter_okay = CompilerFilterIsOkay(target, profile_changed, downgrade);

  bool class_loader_context_okay = ClassLoaderContextIsOkay(context); //    true

  // Only check the filter and relocation if the class loader context is ok.
  // If it is not, we will return kDex2OatFromScratch as the compilation needs to be redone.
  if (class_loader_context_okay) {
    if (filter_okay && Status() == kOatUpToDate) {
      // The oat file is in good shape as is.
      return kNoDexOptNeeded;
    }

    if (filter_okay && !compilation_desired && Status() == kOatRelocationOutOfDate) {
      // If no compilation is desired, then it doesn't matter if the oat
      // file needs relocation. It's in good shape as is.
      return kNoDexOptNeeded;
    }

    if (filter_okay && Status() == kOatRelocationOutOfDate) {
      return kDex2OatForRelocation;
    }

    if (IsUseable()) {
      return kDex2OatForFilter;
    }

    if (Status() == kOatBootImageOutOfDate) {
      return kDex2OatForBootImage;
    }
  }

  if (oat_file_assistant_->HasOriginalDexFiles()) {
    return kDex2OatFromScratch;
  } else {
    // Otherwise there is nothing we can do, even if we want to.
    return kNoDexOptNeeded;
  }
}


bool OatFileAssistant::OatFileInfo::CompilerFilterIsOkay(CompilerFilter::Filter target, bool profile_changed, bool downgrade) {
  const OatFile* file = GetFile();
  if (file == nullptr) {
    return false;
  }
 
  CompilerFilter::Filter current = file->GetCompilerFilter();
  if (profile_changed && CompilerFilter::DependsOnProfile(current)) {
    VLOG(oat) << "Compiler filter not okay because Profile changed";
    return false;
  }
  return downgrade ? !CompilerFilter::IsBetter(current, target) : CompilerFilter::IsAsGoodAs(current, target);
}





