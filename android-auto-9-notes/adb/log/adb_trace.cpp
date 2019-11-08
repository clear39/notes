//  @   system/core/adb/adb_trace.cpp
///////////////////////////////////////////////////////////////////////
/**
 * adb日志初始化接口 
*/
void adb_trace_init(char** argv) {
/**
 * ADB_HOST 定义在 Android.mk 文件中 ， pc端用来连接机器中adbd的为 ADB_HOST=1
 * 也就是pc端 ADB_HOST=1
*/
#if !ADB_HOST
    /**
     * android端 adbd执行这里
     * 
    */
    // Don't open log file if no tracing, since this will block
    // the crypto unmount of /data
    if (!get_trace_setting().empty()) {
        if (unix_isatty(STDOUT_FILENO) == 0) {
            start_device_log();
        }
    }
#endif

#if ADB_HOST && !defined(_WIN32)
    // adb historically ignored $ANDROID_LOG_TAGS but passed it through to logcat.
    // If set, move it out of the way so that libbase logging doesn't try to parse it.
    std::string log_tags;
    char* ANDROID_LOG_TAGS = getenv("ANDROID_LOG_TAGS");
    if (ANDROID_LOG_TAGS) {
        log_tags = ANDROID_LOG_TAGS;
        unsetenv("ANDROID_LOG_TAGS");
    }
#endif

    android::base::InitLogging(argv, &AdbLogger);

#if ADB_HOST && !defined(_WIN32)
    // Put $ANDROID_LOG_TAGS back so we can pass it to logcat.
    if (!log_tags.empty()) setenv("ANDROID_LOG_TAGS", log_tags.c_str(), 1);
#endif

    setup_trace_mask();

    VLOG(ADB) << adb_version();
}


std::string get_trace_setting_from_env() {

    const char* setting = getenv("ADB_TRACE");
    if (setting == nullptr) {
        setting = "";
    }

    return std::string(setting);
}

std::string get_trace_setting() {
#if ADB_HOST
    return get_trace_setting_from_env();
#else
    /**
     * android端执行这里
    */
    return android::base::GetProperty("persist.adb.trace_mask", "");
#endif
}


void start_device_log(void) {
    int fd = unix_open(get_log_file_name().c_str(),O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
    if (fd == -1) {
        return;
    }

    // Redirect stdout and stderr to the log file.
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    fprintf(stderr, "--- adb starting (pid %d) ---\n", getpid());
    unix_close(fd);
}

static std::string get_log_file_name() {
    struct tm now;
    time_t t;
    tzset();
    time(&t);
    localtime_r(&t, &now);

    char timestamp[PATH_MAX];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", &now);

    return android::base::StringPrintf("/data/adb/adb-%s-%d", timestamp,getpid());
}


void AdbLogger(android::base::LogId id, android::base::LogSeverity severity,
               const char* tag, const char* file, unsigned int line,
               const char* message) {
    android::base::StderrLogger(id, severity, tag, file, line, message);
#if !ADB_HOST
    // Only print logs of INFO or higher to logcat, so that `adb logcat` with adbd tracing on
    // doesn't result in exponential logging.
    if (severity >= android::base::INFO) {
        gLogdLogger(id, severity, tag, file, line, message);
    }
#endif
}




// Split the space separated list of tags from the trace setting and build the
// trace mask from it. note that '1' and 'all' are special cases to enable all
// tracing.
//
// adb's trace setting comes from the ADB_TRACE environment variable, whereas
// adbd's comes from the system property persist.adb.trace_mask.
static void setup_trace_mask() {
    const std::string trace_setting = get_trace_setting();
    if (trace_setting.empty()) {
        return;
    }

    std::unordered_map<std::string, int> trace_flags = {
        {"1", -1},
        {"all", -1},
        {"adb", ADB},
        {"sockets", SOCKETS},
        {"packets", PACKETS},
        {"rwx", RWX},
        {"usb", USB},
        {"sync", SYNC},
        {"sysdeps", SYSDEPS},
        {"transport", TRANSPORT},
        {"jdwp", JDWP},
        {"services", SERVICES},
        {"auth", AUTH},
        {"fdevent", FDEVENT},
        {"shell", SHELL}};

    std::vector<std::string> elements = android::base::Split(trace_setting, " ");
    for (const auto& elem : elements) {
        const auto& flag = trace_flags.find(elem);
        if (flag == trace_flags.end()) {
            LOG(ERROR) << "Unknown trace flag: " << elem;
            continue;
        }

        if (flag->second == -1) {
            // -1 is used for the special values "1" and "all" that enable all
            // tracing.
            adb_trace_mask = ~0;
            break;
        } else {
            adb_trace_mask |= 1 << flag->second;
        }
    }

    if (adb_trace_mask != 0) {
        android::base::SetMinimumLogSeverity(android::base::VERBOSE);
    }
}


std::string adb_version() {
    // Don't change the format of this --- it's parsed by ddmlib.
    return android::base::StringPrintf(
        "Android Debug Bridge version %d.%d.%d\n"
        "Version %s\n"
        "Installed as %s\n",
        ADB_VERSION_MAJOR, ADB_VERSION_MINOR, ADB_SERVER_VERSION, ADB_VERSION,
        android::base::GetExecutablePath().c_str());
}


