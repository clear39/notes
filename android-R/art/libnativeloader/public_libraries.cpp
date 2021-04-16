//  @   D:\works\aosp\anroid-R\art\libnativeloader\public_libraries.cpp


//  @  libnativeloader/public_libraries.cpp" 
const std::string& preloadable_public_libraries() {
  static std::string list = InitDefaultPublicLibraries(/*for_preload*/ true);
  return list;
}


static std::string InitDefaultPublicLibraries(bool for_preload) {
   /**
    * root_dir得到/system 
    * constexpr const char* kDefaultPublicLibrariesFile = "/etc/public.libraries.txt";
    * */
  std::string config_file = root_dir() + kDefaultPublicLibrariesFile; // /system/etc/public.libraries.txt
  auto sonames =
      ReadConfig(config_file, [&for_preload](const struct ConfigEntry& entry) -> Result<bool> {
        if (for_preload) {
          return !entry.nopreload;
        } else {
          return true;
        }
      });
  if (!sonames.ok()) {
    LOG_ALWAYS_FATAL("Error reading public native library list from \"%s\": %s",
                     config_file.c_str(), sonames.error().message().c_str());
    return "";
  }

  std::string additional_libs = additional_public_libraries();
  if (!additional_libs.empty()) {
    auto vec = base::Split(additional_libs, ":");
    std::copy(vec.begin(), vec.end(), std::back_inserter(*sonames));
  }

  // If this is for preloading libs, don't remove the libs from APEXes.
  if (for_preload) {
    return android::base::Join(*sonames, ':');
  }

  // Remove the public libs in the art namespace.
  // These libs are listed in public.android.txt, but we don't want the rest of android
  // in default namespace to dlopen the libs.
  // For example, libicuuc.so is exposed to classloader namespace from art namespace.
  // Unfortunately, it does not have stable C symbols, and default namespace should only use
  // stable symbols in libandroidicu.so. http://b/120786417
  /**
   const std::vector<const std::string> kArtApexPublicLibraries = {
        "libicuuc.so",
        "libicui18n.so",
    };
    
  */
  for (const std::string& lib_name : kArtApexPublicLibraries) {
      
    std::string path(kArtApexLibPath);
    path.append("/").append(lib_name);

    struct stat s;
    // Do nothing if the path in /apex does not exist.
    // Runtime APEX must be mounted since libnativeloader is in the same APEX
    if (stat(path.c_str(), &s) != 0) {
      continue;
    }

    auto it = std::find(sonames->begin(), sonames->end(), lib_name);
    if (it != sonames->end()) {
      sonames->erase(it);
    }
  }

  // Remove the public libs in the nnapi namespace.
  auto it = std::find(sonames->begin(), sonames->end(), kNeuralNetworksApexPublicLibrary);
  if (it != sonames->end()) {
    sonames->erase(it);
  }
  return android::base::Join(*sonames, ':');
}