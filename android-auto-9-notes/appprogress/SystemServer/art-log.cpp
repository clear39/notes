// VLOG @/work/workcodes/aosp-p9.x-auto-alpha/art/libartbase/base/logging.h

VLOG(oat) << "ClassLoaderContext check ignored: null context";




// Is verbose logging enabled for the given module? Where the module is defined in LogVerbosity.
#define VLOG_IS_ON(module) UNLIKELY(::art::gLogVerbosity.module)
 


// Variant of LOG that logs when verbose logging is enabled for a module. For example,
// VLOG(jni) << "A JNI operation was performed";
#define VLOG(module) if (VLOG_IS_ON(module)) LOG(INFO)



//  @/work/workcodes/aosp-p9.x-auto-alpha/art/libartbase/base/logging.cc
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
    ┊ gCmdLine->append(" ");
    ┊ gCmdLine->append(argv[i]);
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



  android::base::InitLogging(argv, INIT_LOGGING_DEFAULT_LOGGER,std::move<AbortFunction>(abort_function));

#undef INIT_LOGGING_DEFAULT_LOGGER

}


//  @/work/workcodes/aosp-p9.x-auto-alpha/system/core/base/logging.cpp
void InitLogging(char* argv[], LogFunction&& logger, AbortFunction&& aborter) {                                                                                                                               
  SetLogger(std::forward<LogFunction>(logger));
  SetAborter(std::forward<AbortFunction>(aborter));
 
  if (gInitialized) {
    return;
  }
 
  gInitialized = true;
 
  // Stash the command line for later use. We can use /proc/self/cmdline on
  // Linux to recover this, but we don't have that luxury on the Mac/Windows,
  // and there are a couple of argv[0] variants that are commonly used.
  if (argv != nullptr) {
    SetDefaultTag(basename(argv[0]));
  }
 
  const char* tags = getenv("ANDROID_LOG_TAGS");
  if (tags == nullptr) {
    return;
  }
 
  std::vector<std::string> specs = Split(tags, " ");
  for (size_t i = 0; i < specs.size(); ++i) {
    // "tag-pattern:[vdiwefs]"
    std::string spec(specs[i]);
    if (spec.size() == 3 && StartsWith(spec, "*:")) {
    ┊ switch (spec[2]) {
    ┊   case 'v':
    ┊   ┊ gMinimumLogSeverity = VERBOSE;
    ┊   ┊ continue;
    ┊   case 'd':
    ┊   ┊ gMinimumLogSeverity = DEBUG;
    ┊   ┊ continue;
    ┊   case 'i':
    ┊   ┊ gMinimumLogSeverity = INFO;
    ┊   ┊ continue;
    ┊   case 'w':
    ┊   ┊ gMinimumLogSeverity = WARNING;
    ┊   ┊ continue;
    ┊   case 'e':
    ┊   ┊ gMinimumLogSeverity = ERROR;
    ┊   ┊ continue;
    ┊   case 'f':
    ┊   ┊ gMinimumLogSeverity = FATAL_WITHOUT_ABORT;
    ┊   ┊ continue;
    ┊   // liblog will even suppress FATAL if you say 's' for silent, but that's
    ┊   // crazy!
    ┊   case 's':
    ┊   ┊ gMinimumLogSeverity = FATAL_WITHOUT_ABORT;
    ┊   ┊ continue;
    ┊ }
    }
    LOG(FATAL) << "unsupported '" << spec << "' in ANDROID_LOG_TAGS (" << tags << ")";
  }
}



//  @/work/workcodes/aosp-p9.x-auto-alpha/art/openjdkjvmti/OpenjdkJvmTi.cc
static jvmtiError SetVerboseFlag(jvmtiEnv* env,jvmtiVerboseFlag flag,jboolean value) {
    ENSURE_VALID_ENV(env);
    if (flag == jvmtiVerboseFlag::JVMTI_VERBOSE_OTHER) {
        ┊ // OTHER is special, as it's 0, so can't do a bit check.
        ┊ bool val = (value == JNI_TRUE) ? true : false;
        
        ┊ art::gLogVerbosity.collector = val;
        ┊ art::gLogVerbosity.compiler = val;
        ┊ art::gLogVerbosity.deopt = val;
        ┊ art::gLogVerbosity.heap = val;
        ┊ art::gLogVerbosity.jdwp = val;
        ┊ art::gLogVerbosity.jit = val;
        ┊ art::gLogVerbosity.monitor = val;
        ┊ art::gLogVerbosity.oat = val;
        ┊ art::gLogVerbosity.profiler = val;
        ┊ art::gLogVerbosity.signals = val;
        ┊ art::gLogVerbosity.simulator = val;
        ┊ art::gLogVerbosity.startup = val;
        ┊ art::gLogVerbosity.third_party_jni = val;
        ┊ art::gLogVerbosity.threads = val;
        ┊ art::gLogVerbosity.verifier = val;
        ┊ // Do not set verifier-debug.
        ┊ art::gLogVerbosity.image = val;
        
        ┊ // Note: can't switch systrace_lock_logging. That requires changing entrypoints.
        
        ┊ art::gLogVerbosity.agents = val;
    } else {
        ┊ // Spec isn't clear whether "flag" is a mask or supposed to be single. We implement the mask
        ┊ // semantics.
        ┊ constexpr std::underlying_type<jvmtiVerboseFlag>::type kMask =
        ┊   ┊ jvmtiVerboseFlag::JVMTI_VERBOSE_GC |
        ┊   ┊ jvmtiVerboseFlag::JVMTI_VERBOSE_CLASS |
        ┊   ┊ jvmtiVerboseFlag::JVMTI_VERBOSE_JNI;
        ┊ if ((flag & ~kMask) != 0) {
        ┊   return ERR(ILLEGAL_ARGUMENT);

        ┊ bool val = (value == JNI_TRUE) ? true : false;

        ┊ if ((flag & jvmtiVerboseFlag::JVMTI_VERBOSE_GC) != 0) {
        ┊   art::gLogVerbosity.gc = val;
        ┊ }

        ┊ if ((flag & jvmtiVerboseFlag::JVMTI_VERBOSE_CLASS) != 0) {
        ┊   art::gLogVerbosity.class_linker = val;
        ┊ }

        ┊ if ((flag & jvmtiVerboseFlag::JVMTI_VERBOSE_JNI) != 0) {
        ┊   art::gLogVerbosity.jni = val;
        ┊ }
    } 
      
    return ERR(NONE);
  } 



  //    @art/openjdkjvmti/include/jvmti.h
