//  @   art\libnativeloader\native_loader.cpp

#define __ANDROID__

LibraryNamespaces* g_namespaces = new LibraryNamespaces;

void InitializeNativeLoader() {
#if defined(__ANDROID__)
  std::lock_guard<std::mutex> guard(g_namespaces_mutex);
  g_namespaces->Initialize();
#endif
}
