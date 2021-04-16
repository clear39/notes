//  @   D:\works\aosp\anroid-R\art\libnativeloader\library_namespaces.cpp


class LibraryNamespaces {
 public:
  LibraryNamespaces() : initialized_(false), app_main_namespace_(nullptr) {}
  ......
}

void LibraryNamespaces::Initialize() {
  // Once public namespace is initialized there is no
  // point in running this code - it will have no effect
  // on the current list of public libraries.
  if (initialized_) {
    return;
  }

  // android_init_namespaces() expects all the public libraries
  // to be loaded so that they can be found by soname alone.
  //
  // TODO(dimitry): this is a bit misleading since we do not know
  // if the vendor public library is going to be opened from /vendor/lib
  // we might as well end up loading them from /system/lib or /product/lib
  // For now we rely on CTS test to catch things like this but
  // it should probably be addressed in the future.
  /**
   * preloadable_public_libraries 定义在 libnativeloader\public_libraries.cpp
  */
  for (const auto& soname : android::base::Split(preloadable_public_libraries(), ":")) {
    LOG_ALWAYS_FATAL_IF(dlopen(soname.c_str(), RTLD_NOW | RTLD_NODELETE) == nullptr,
                        "Error preloading public library %s: %s", soname.c_str(), dlerror());
  }
}


