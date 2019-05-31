
//  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/class_loader_context.cc

std::unique_ptr<ClassLoaderContext> ClassLoaderContext::Create(const std::string& spec) {
  std::unique_ptr<ClassLoaderContext> result(new ClassLoaderContext());
  if (result->Parse(spec)) {
    return result;
  } else {
    return nullptr;
  }
}


// The format: ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]...
// ClassLoaderType is either "PCL" (PathClassLoader) or "DLC" (DelegateLastClassLoader).
// ClasspathElem is the path of dex/jar/apk file.
bool ClassLoaderContext::Parse(const std::string& spec, bool parse_checksums) {
  if (spec.empty()) {
    // By default we load the dex files in a PathClassLoader.
    // So an empty spec is equivalent to an empty PathClassLoader (this happens when running
    // tests)
    //  @/work/workcodes/aosp-p9.x-auto-alpha/art/runtime/class_loader_context.h
    class_loader_chain_.push_back(ClassLoaderInfo(kPathClassLoader));   //  static constexpr char kPathClassLoaderString[] = "PCL";
    return true;
  }

  // Stop early if we detect the special shared library, which may be passed as the classpath
  // for dex2oat when we want to skip the shared libraries check.
  if (spec == OatFile::kSpecialSharedLibrary) {
    LOG(INFO) << "The ClassLoaderContext is a special shared library.";
    special_shared_library_ = true;
    return true;
  }

  std::vector<std::string> class_loaders;
  Split(spec, kClassLoaderSeparator, &class_loaders);

  for (const std::string& class_loader : class_loaders) {
    ClassLoaderType type = ExtractClassLoaderType(class_loader);
    if (type == kInvalidClassLoader) {
      LOG(ERROR) << "Invalid class loader type: " << class_loader;
      return false;
    }
    if (!ParseClassLoaderSpec(class_loader, type, parse_checksums)) {
      LOG(ERROR) << "Invalid class loader spec: " << class_loader;
      return false;
    }
  }
  return true;
}