//	@art/runtime/gc/space/image_space.cc
bool ImageSpace::LoadBootImage(const std::string& image_file_name,
                               const InstructionSet image_instruction_set,
                               std::vector<space::ImageSpace*>* boot_image_spaces,
                               uint8_t** oat_file_end) {
  DCHECK(boot_image_spaces != nullptr);
  DCHECK(boot_image_spaces->empty());
  DCHECK(oat_file_end != nullptr);
  DCHECK_NE(image_instruction_set, InstructionSet::kNone);

  if (image_file_name.empty()) {
    return false;
  }

  // For code reuse, handle this like a work queue.
  std::vector<std::string> image_file_names;
  image_file_names.push_back(image_file_name);

  bool error = false;
  uint8_t* oat_file_end_tmp = *oat_file_end;

  for (size_t index = 0; index < image_file_names.size(); ++index) {
    std::string& image_name = image_file_names[index];
    std::string error_msg;
    std::unique_ptr<space::ImageSpace> boot_image_space_uptr = CreateBootImage(image_name.c_str(),image_instruction_set,index > 0,&error_msg);
    if (boot_image_space_uptr != nullptr) {
      space::ImageSpace* boot_image_space = boot_image_space_uptr.release();
      boot_image_spaces->push_back(boot_image_space);
      // Oat files referenced by image files immediately follow them in memory, ensure alloc space
      // isn't going to get in the middle
      uint8_t* oat_file_end_addr = boot_image_space->GetImageHeader().GetOatFileEnd();
      CHECK_GT(oat_file_end_addr, boot_image_space->End());
      oat_file_end_tmp = AlignUp(oat_file_end_addr, kPageSize);

      if (index == 0) {
        // If this was the first space, check whether there are more images to load.
        const OatFile* boot_oat_file = boot_image_space->GetOatFile();
        if (boot_oat_file == nullptr) {
          continue;
        }

        const OatHeader& boot_oat_header = boot_oat_file->GetOatHeader();
        const char* boot_classpath =
            boot_oat_header.GetStoreValueByKey(OatHeader::kBootClassPathKey);
        if (boot_classpath == nullptr) {
          continue;
        }

        ExtractMultiImageLocations(image_file_name, boot_classpath, &image_file_names);
      }
    } else {
      error = true;
      LOG(ERROR) << "Could not create image space with image file '" << image_file_name << "'. " << "Attempting to fall back to imageless running. Error was: " << error_msg << "\nAttempted image: " << image_name;
      break;
    }
  }

  if (error) {
    // Remove already loaded spaces.
    for (space::Space* loaded_space : *boot_image_spaces) {
      delete loaded_space;
    }
    boot_image_spaces->clear();
    return false;
  }

  *oat_file_end = oat_file_end_tmp;
  return true;
}


const char* GetInstructionSetString(InstructionSet isa) {
  switch (isa) {
    case kArm:
    case kThumb2:
      return "arm";
    case kArm64:
      return "arm64";
    case kX86:
      return "x86";
    case kX86_64:
      return "x86_64";
    case kMips:
      return "mips";
    case kMips64:
      return "mips64";
    case kNone:
      return "none";
  }
  LOG(FATAL) << "Unknown ISA " << isa;
  UNREACHABLE();
}


std::string GetDalvikCache(const char* subdir) {
  CHECK(subdir != nullptr);
  const char* android_data = GetAndroidData();
  const std::string dalvik_cache_root(StringPrintf("%s/dalvik-cache/", android_data));
  const std::string dalvik_cache = dalvik_cache_root + subdir;
  if (!OS::DirectoryExists(dalvik_cache.c_str())) {
    // TODO: Check callers. Traditional behavior is to not abort.
    return "";
  }
  return dalvik_cache;
}



static bool CanWriteToDalvikCache(const InstructionSet isa) {
  //	@art/runtime/arch/instruction_set.cc:44:const char* GetInstructionSetString(InstructionSet isa)
  //	@art/runtime/utils.cc:815:std::string GetDalvikCache(const char* subdir) 
  const std::string dalvik_cache = GetDalvikCache(GetInstructionSetString(isa));
  if (access(dalvik_cache.c_str(), O_RDWR) == 0) {
    return true;
  } else if (errno != EACCES) {
    PLOG(WARNING) << "CanWriteToDalvikCache returned error other than EACCES";
  }
  return false;
}


std::unique_ptr<ImageSpace> ImageSpace::CreateBootImage(const char* image_location,const InstructionSet image_isa,bool secondary_image = false,std::string* error_msg) {
  ScopedTrace trace(__FUNCTION__);

  // Step 0: Extra zygote work.

  // Step 0.a: If we're the zygote, mark boot.
  const bool is_zygote = Runtime::Current()->IsZygote();
  //	CanWriteToDalvikCache	@art/runtime/gc/space/image_space.cc
  //	CanWriteToDalvikCache 主要是获取/data/dalvik-cache/arm/目录是否可读可写
  if (is_zygote && !secondary_image && CanWriteToDalvikCache(image_isa)) {
    MarkZygoteStart(image_isa, Runtime::Current()->GetZygoteMaxFailedBoots());
  }

  // Step 0.b: If we're the zygote, check for free space, and prune the cache preemptively,
  //           if necessary. While the runtime may be fine (it is pretty tolerant to
  //           out-of-disk-space situations), other parts of the platform are not.
  //
  //           The advantage of doing this proactively is that the later steps are simplified,
  //           i.e., we do not need to code retries.
  std::string system_filename;
  bool has_system = false;
  std::string cache_filename;
  bool has_cache = false;
  bool dalvik_cache_exists = false;
  bool is_global_cache = true;
  std::string dalvik_cache;
  bool found_image = FindImageFilenameImpl(image_location,image_isa,&has_system,&system_filename,&dalvik_cache_exists,&dalvik_cache,&is_global_cache,&has_cache,&cache_filename);

  if (is_zygote && dalvik_cache_exists) {
    DCHECK(!dalvik_cache.empty());
    std::string local_error_msg;
    if (!CheckSpace(dalvik_cache, &local_error_msg)) {
      LOG(WARNING) << local_error_msg << " Preemptively pruning the dalvik cache.";
      PruneDalvikCache(image_isa);

      // Re-evaluate the image.
      found_image = FindImageFilenameImpl(image_location,
                                          image_isa,
                                          &has_system,
                                          &system_filename,
                                          &dalvik_cache_exists,
                                          &dalvik_cache,
                                          &is_global_cache,
                                          &has_cache,
                                          &cache_filename);
    }
  }

  // Collect all the errors.
  std::vector<std::string> error_msgs;

  // Step 1: Check if we have an existing and relocated image.

  // Step 1.a: Have files in system and cache. Then they need to match.
  if (found_image && has_system && has_cache) {
    std::string local_error_msg;
    // Check that the files are matching.
    if (ChecksumsMatch(system_filename.c_str(), cache_filename.c_str(), &local_error_msg)) {
      std::unique_ptr<ImageSpace> relocated_space =
          ImageSpaceLoader::Load(image_location,
                                 cache_filename,
                                 is_zygote,
                                 is_global_cache,
                                 /* validate_oat_file */ false,
                                 &local_error_msg);
      if (relocated_space != nullptr) {
        return relocated_space;
      }
    }
    error_msgs.push_back(local_error_msg);
  }

  // Step 1.b: Only have a cache file.
  if (found_image && !has_system && has_cache) {
    std::string local_error_msg;
    std::unique_ptr<ImageSpace> cache_space =
        ImageSpaceLoader::Load(image_location,cache_filename,is_zygote,is_global_cache,/* validate_oat_file */ true,&local_error_msg);
    if (cache_space != nullptr) {
      return cache_space;
    }
    error_msgs.push_back(local_error_msg);
  }

  // Step 2: We have an existing image in /system.

  // Step 2.a: We are not required to relocate it. Then we can use it directly.
  bool relocate = Runtime::Current()->ShouldRelocate();

  if (found_image && has_system && !relocate) {
    std::string local_error_msg;
    std::unique_ptr<ImageSpace> system_space =
        ImageSpaceLoader::Load(image_location,
                               system_filename,
                               is_zygote,
                               is_global_cache,
                               /* validate_oat_file */ false,
                               &local_error_msg);
    if (system_space != nullptr) {
      return system_space;
    }
    error_msgs.push_back(local_error_msg);
  }

  // Step 2.b: We require a relocated image. Then we must patch it. This step fails if this is a
  //           secondary image.
  if (found_image && has_system && relocate) {
    std::string local_error_msg;
    if (!Runtime::Current()->IsImageDex2OatEnabled()) {
      local_error_msg = "Patching disabled.";
    } else if (secondary_image) {
      // We really want a working image. Prune and restart.
      PruneDalvikCache(image_isa);
      _exit(1);
    } else if (ImageCreationAllowed(is_global_cache, image_isa, &local_error_msg)) {
      bool patch_success =
          RelocateImage(image_location, cache_filename.c_str(), image_isa, &local_error_msg);
      if (patch_success) {
        std::unique_ptr<ImageSpace> patched_space =
            ImageSpaceLoader::Load(image_location,
                                   cache_filename,
                                   is_zygote,
                                   is_global_cache,
                                   /* validate_oat_file */ false,
                                   &local_error_msg);
        if (patched_space != nullptr) {
          return patched_space;
        }
      }
    }
    error_msgs.push_back(StringPrintf("Cannot relocate image %s to %s: %s",
                                      image_location,
                                      cache_filename.c_str(),
                                      local_error_msg.c_str()));
  }

  // Step 3: We do not have an existing image in /system, so generate an image into the dalvik
  //         cache. This step fails if this is a secondary image.
  if (!has_system) {
    std::string local_error_msg;
    if (!Runtime::Current()->IsImageDex2OatEnabled()) {
      local_error_msg = "Image compilation disabled.";
    } else if (secondary_image) {
      local_error_msg = "Cannot compile a secondary image.";
    } else if (ImageCreationAllowed(is_global_cache, image_isa, &local_error_msg)) {
      bool compilation_success = GenerateImage(cache_filename, image_isa, &local_error_msg);
      if (compilation_success) {
        std::unique_ptr<ImageSpace> compiled_space =
            ImageSpaceLoader::Load(image_location,
                                   cache_filename,
                                   is_zygote,
                                   is_global_cache,
                                   /* validate_oat_file */ false,
                                   &local_error_msg);
        if (compiled_space != nullptr) {
          return compiled_space;
        }
      }
    }
    error_msgs.push_back(StringPrintf("Cannot compile image to %s: %s",
                                      cache_filename.c_str(),
                                      local_error_msg.c_str()));
  }

  // We failed. Prune the cache the free up space, create a compound error message and return no
  // image.
  PruneDalvikCache(image_isa);

  std::ostringstream oss;
  bool first = true;
  for (const auto& msg : error_msgs) {
    if (!first) {
      oss << "\n    ";
    }
    oss << msg;
  }
  *error_msg = oss.str();

  return nullptr;
}
