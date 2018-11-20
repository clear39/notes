 // @system/core/libnativeloader/native_loader.cpp
void InitializeNativeLoader() {
#if defined(__ANDROID__)
  std::lock_guard<std::mutex> guard(g_namespaces_mutex);
  g_namespaces->Initialize();
#endif
}


static std::mutex g_namespaces_mutex;
static LibraryNamespaces* g_namespaces = new LibraryNamespaces;//	@system/core/libnativeloader/native_loader.cpp


class LibraryNamespaces {
 public:
  LibraryNamespaces() : initialized_(false) { }


  void Initialize() {
    // Once public namespace is initialized there is no
    // point in running this code - it will have no effect
    // on the current list of public libraries.
    if (initialized_) {
      return;
    }

    std::vector<std::string> sonames;
    const char* android_root_env = getenv("ANDROID_ROOT");
    std::string root_dir = android_root_env != nullptr ? android_root_env : "/system";
    std::string public_native_libraries_system_config = root_dir + kPublicNativeLibrariesSystemConfigPathFromRoot;	//kPublicNativeLibrariesSystemConfigPathFromRoot =  "/etc/public.libraries.txt";
    std::string llndk_native_libraries_system_config = root_dir + kLlndkNativeLibrariesSystemConfigPathFromRoot;//kLlndkNativeLibrariesSystemConfigPathFromRoot =  "/etc/llndk.libraries.txt";
    std::string vndksp_native_libraries_system_config = root_dir + kVndkspNativeLibrariesSystemConfigPathFromRoot;	//kVndkspNativeLibrariesSystemConfigPathFromRoot = "/etc/vndksp.libraries.txt";

    /*
    autolink_8qxp:/ $ cat /system/etc/public.libraries.txt
		libandroid.so
		libaaudio.so
		libc.so
		libcamera2ndk.so
		libdl.so
		libEGL.so
		libGLESv1_CM.so
		libGLESv2.so
		libGLESv3.so
		libicui18n.so
		libicuuc.so
		libjnigraphics.so
		liblog.so
		libmediandk.so
		libm.so
		libnativewindow.so
		libneuralnetworks.so
		libOpenMAXAL.so
		libOpenSLES.so
		libRS.so
		libstdc++.so
		libsync.so
		libvulkan.so
		libwebviewchromium_plat_support.so
		libz.so
    */

    std::string error_msg;
    LOG_ALWAYS_FATAL_IF(!ReadConfig(public_native_libraries_system_config, &sonames, &error_msg), "Error reading public native library list from \"%s\": %s", public_native_libraries_system_config.c_str(), error_msg.c_str());

    // For debuggable platform builds use ANDROID_ADDITIONAL_PUBLIC_LIBRARIES environment
    // variable to add libraries to the list. This is intended for platform tests only.
    if (is_debuggable()) {
      const char* additional_libs = getenv("ANDROID_ADDITIONAL_PUBLIC_LIBRARIES");
      if (additional_libs != nullptr && additional_libs[0] != '\0') {
        std::vector<std::string> additional_libs_vector = base::Split(additional_libs, ":");
        std::copy(additional_libs_vector.begin(),additional_libs_vector.end(),std::back_inserter(sonames));
      }
    }

    // android_init_namespaces() expects all the public libraries
    // to be loaded so that they can be found by soname alone.
    //
    // TODO(dimitry): this is a bit misleading since we do not know
    // if the vendor public library is going to be opened from /vendor/lib
    // we might as well end up loading them from /system/lib
    // For now we rely on CTS test to catch things like this but
    // it should probably be addressed in the future.
    for (const auto& soname : sonames) {
      LOG_ALWAYS_FATAL_IF(dlopen(soname.c_str(), RTLD_NOW | RTLD_NODELETE) == nullptr,"Error preloading public library %s: %s",soname.c_str(), dlerror());
    }

    system_public_libraries_ = base::Join(sonames, ':');//	 std::string system_public_libraries_;

	//下面代码没有实际作用,目前文件不存在
    sonames.clear();
    ReadConfig(kLlndkNativeLibrariesSystemConfigPathFromRoot, &sonames);
    system_llndk_libraries_ = base::Join(sonames, ':');

    sonames.clear();
    ReadConfig(kVndkspNativeLibrariesSystemConfigPathFromRoot, &sonames);
    system_vndksp_libraries_ = base::Join(sonames, ':');

    sonames.clear();
    // This file is optional, quietly ignore if the file does not exist.
    ReadConfig(kPublicNativeLibrariesVendorConfig, &sonames);

    vendor_public_libraries_ = base::Join(sonames, ':');
  }




private:
  bool ReadConfig(const std::string& configFile, std::vector<std::string>* sonames,std::string* error_msg = nullptr) {
    // Read list of public native libraries from the config file.
    std::string file_content;
    if(!base::ReadFileToString(configFile, &file_content)) {
      if (error_msg) *error_msg = strerror(errno);
      return false;
    }

    std::vector<std::string> lines = base::Split(file_content, "\n");

    for (auto& line : lines) {
      auto trimmed_line = base::Trim(line);
      if (trimmed_line[0] == '#' || trimmed_line.empty()) {
        continue;
      }
      size_t space_pos = trimmed_line.rfind(' ');
      if (space_pos != std::string::npos) {
        std::string type = trimmed_line.substr(space_pos + 1);
        if (type != "32" && type != "64") {
          if (error_msg) *error_msg = "Malformed line: " + line;
          return false;
        }
#if defined(__LP64__)
        // Skip 32 bit public library.
        if (type == "32") {
          continue;
        }
#else
        // Skip 64 bit public library.
        if (type == "64") {
          continue;
        }
#endif
        trimmed_line.resize(space_pos);
      }

      sonames->push_back(trimmed_line);
    }

    return true;
  }

}




