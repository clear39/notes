



void InitLogging(char* argv[], AbortFunction& abort_function) {
  if (gCmdLine.get() != nullptr) {
    return;
  }

  // Stash the command line for later use. We can use /proc/self/cmdline on Linux to recover this,
  // but we don't have that luxury on the Mac, and there are a couple of argv[0] variants that are
  // commonly used.
  if (argv != nullptr) {
    gCmdLine.reset(new std::string(argv[0]));
    for (size_t i = 1; argv[i] != nullptr; ++i) {
      gCmdLine->append(" ");
      gCmdLine->append(argv[i]);
    }
    gProgramInvocationName.reset(new std::string(argv[0]));
    const char* last_slash = strrchr(argv[0], '/');
    gProgramInvocationShortName.reset(new std::string((last_slash != nullptr) ? last_slash + 1 : argv[0]));
  } else {
    // TODO: fall back to /proc/self/cmdline when argv is null on Linux.
    gCmdLine.reset(new std::string("<unset>"));
  }

#ifdef ART_TARGET_ANDROID
#define INIT_LOGGING_DEFAULT_LOGGER android::base::LogdLogger()
#else
#define INIT_LOGGING_DEFAULT_LOGGER android::base::StderrLogger
#endif



  android::base::InitLogging(argv, INIT_LOGGING_DEFAULT_LOGGER, std::move<AbortFunction>(abort_function));
#undef INIT_LOGGING_DEFAULT_LOGGER
}
