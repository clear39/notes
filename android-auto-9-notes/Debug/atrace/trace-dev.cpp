

//  @  system/core/libcutils/trace-dev.cpp



static void atrace_init_once()
{
    atrace_marker_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY | O_CLOEXEC);
    if (atrace_marker_fd == -1) {
        ALOGE("Error opening trace file: %s (%d)", strerror(errno), errno);
        atrace_enabled_tags = 0;
        goto done;
    }

    atrace_enabled_tags = atrace_get_property();

done:
  // 初始化 atrace_is_ready  已经 打开 /sys/kernel/debug/tracing/trace_marker 完成
    atomic_store_explicit(&atrace_is_ready, true, memory_order_release);
}

void atrace_setup()
{
    pthread_once(&atrace_once_control, atrace_init_once);
}































// Set whether tracing is enabled in this process.  This is used to prevent
// the Zygote process from tracing.
void atrace_set_tracing_enabled(bool enabled)
{
    /**
     * system/core/libcutils/trace-dev.inc:49:static atomic_bool      atrace_is_enabled    = ATOMIC_VAR_INIT(true);
     */ 
    atomic_store_explicit(&atrace_is_enabled, enabled, memory_order_release);
    atrace_update_tags();
}


// Set whether this process is debuggable, which determines whether
// application-level tracing is allowed when the ro.debuggable system property
// is not set to '1'.
void atrace_set_debuggable(bool debuggable)
{
    //  @   system/core/libcutils/trace-dev.inc:48:static bool             atrace_is_debuggable = false;
    atrace_is_debuggable = debuggable;
    atrace_update_tags();
}


//  @   system/core/libcutils/trace-dev.inc
// Update tags if tracing is ready. Useful as a sysprop change callback.
void atrace_update_tags()
{
    /**
     * system/core/libcutils/trace-dev.inc:45:atomic_bool             atrace_is_ready      = ATOMIC_VAR_INIT(false);
    */
    uint64_t tags;
    if (CC_UNLIKELY(atomic_load_explicit(&atrace_is_ready, memory_order_acquire))) {
        if (atomic_load_explicit(&atrace_is_enabled, memory_order_acquire)) {
            tags = atrace_get_property();
            pthread_mutex_lock(&atrace_tags_mutex);
            atrace_enabled_tags = tags;
            pthread_mutex_unlock(&atrace_tags_mutex);
        } else {
            // Tracing is disabled for this process, so we simply don't
            // initialize the tags.
            pthread_mutex_lock(&atrace_tags_mutex);
            //  @   system/core/libcutils/include/cutils/trace.h:80:#define ATRACE_TAG_NOT_READY        (1ULL<<63)
            atrace_enabled_tags = ATRACE_TAG_NOT_READY;
            pthread_mutex_unlock(&atrace_tags_mutex);
        }
    }
}

//  @   system/core/libcutils/trace-dev.inc
// Read the sysprop and return the value tags should be set to
static uint64_t atrace_get_property()
{
    char value[PROPERTY_VALUE_MAX];
    char *endptr;
    uint64_t tags;

    property_get("debug.atrace.tags.enableflags", value, "0");
    errno = 0;
    //  convert a string to an unsigned long integer
    tags = strtoull(value, &endptr, 0);
    if (value[0] == '\0' || *endptr != '\0') {
        ALOGE("Error parsing trace property: Not a number: %s", value);
        return 0;
    } else if (errno == ERANGE || tags == ULLONG_MAX) {
        ALOGE("Error parsing trace property: Number too large: %s", value);
        return 0;
    }

    // Only set the "app" tag if this process was selected for app-level debug
    // tracing.
    if (atrace_is_app_tracing_enabled()) {
        tags |= ATRACE_TAG_APP;
    } else {
        tags &= ~ATRACE_TAG_APP;
    }

    return (tags | ATRACE_TAG_ALWAYS) & ATRACE_TAG_VALID_MASK;
}


// Determine whether application-level tracing is enabled for this process.
static bool atrace_is_app_tracing_enabled()
{
    bool sys_debuggable = __android_log_is_debuggable();
    bool result = false;

    if (sys_debuggable || atrace_is_debuggable) {
        // Check whether tracing is enabled for this process.
        FILE * file = fopen("/proc/self/cmdline", "re");
        if (file) {
            char cmdline[4096];
            if (fgets(cmdline, sizeof(cmdline), file)) {
                result = atrace_is_cmdline_match(cmdline);
            } else {
                ALOGE("Error reading cmdline: %s (%d)", strerror(errno), errno);
            }
            fclose(file);
        } else {
            ALOGE("Error opening /proc/self/cmdline: %s (%d)", strerror(errno),  errno);
        }
    }

    return result;
}



// Check whether the given command line matches one of the comma-separated
// values listed in the app_cmdlines property.
static bool atrace_is_cmdline_match(const char* cmdline)
{
    int count = property_get_int32("debug.atrace.app_number", 0);

    char buf[PROPERTY_KEY_MAX];
    char value[PROPERTY_VALUE_MAX];

    for (int i = 0; i < count; i++) {
        snprintf(buf, sizeof(buf), "debug.atrace.app_%d", i);
        property_get(buf, value, "");
        if (strcmp(value, "*") == 0 || strcmp(value, cmdline) == 0) {
            return true;
        }
    }

    return false;
}
