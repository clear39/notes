


static jlong NativeLoadFromFd(JNIEnv* env, jclass /*clazz*/, jobject file_descriptor,
                              jstring friendly_name, jboolean system, jboolean force_shared_lib) {
  ScopedUtfChars friendly_name_utf8(env, friendly_name);
  if (friendly_name_utf8.c_str() == nullptr) {
    return 0;
  }

  ATRACE_NAME(base::StringPrintf("LoadApkAssetsFd(%s)", friendly_name_utf8.c_str()).c_str());

  int fd = jniGetFDFromFileDescriptor(env, file_descriptor);
  if (fd < 0) {
    jniThrowException(env, "java/lang/IllegalArgumentException", "Bad FileDescriptor");
    return 0;
  }

  unique_fd dup_fd(::dup(fd));
  if (dup_fd < 0) {
    jniThrowIOException(env, errno);
    return 0;
  }

  std::unique_ptr<const ApkAssets> apk_assets = ApkAssets::LoadFromFd(std::move(dup_fd),
                                                                      friendly_name_utf8.c_str(),
                                                                      system, force_shared_lib);
  if (apk_assets == nullptr) {
    std::string error_msg = base::StringPrintf("Failed to load asset path %s from fd %d",
                                               friendly_name_utf8.c_str(), dup_fd.get());
    jniThrowException(env, "java/io/IOException", error_msg.c_str());
    return 0;
  }
  return reinterpret_cast<jlong>(apk_assets.release());
}
