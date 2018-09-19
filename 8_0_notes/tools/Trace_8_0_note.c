//	TRACE的作用为：将日志写入/sys/kernel/debug/tracing/trace_marker中


//类注释
/**
 * Writes trace events to the system trace buffer.  These trace events can be
 * collected and visualized（可视化的） using the Systrace tool.
 *
 * <p>This tracing mechanism(机制) is independent(独立的) of the method tracing mechanism
 * offered by {@link Debug#startMethodTracing}.  In particular, it enables
 * tracing of events that occur(发生) across multiple processes.
 * <p>For information about using the Systrace tool, read <a
 * href="{@docRoot}tools/debugging/systrace.html">Analyzing Display and Performance
 * with Systrace</a>.
 */
public final class Trace {
	/*
	 * Writes trace events to the kernel trace buffer.  These trace events can be
	 * collected using the "atrace" program for offline analysis.
	 */
	private static final String TAG = "Trace";
	 
	 
	// These tags must be kept in sync with system/core/include/cutils/trace.h.
	// They should also be added to frameworks/native/cmds/atrace/atrace.cpp.
	/** @hide */
	public static final long TRACE_TAG_NEVER = 0;
	/** @hide */
	public static final long TRACE_TAG_ALWAYS = 1L << 0;
	/** @hide */
	public static final long TRACE_TAG_GRAPHICS = 1L << 1;
	/** @hide */
	public static final long TRACE_TAG_INPUT = 1L << 2;
	/** @hide */
	public static final long TRACE_TAG_VIEW = 1L << 3;
	/** @hide */
	public static final long TRACE_TAG_WEBVIEW = 1L << 4;
	/** @hide */
	public static final long TRACE_TAG_WINDOW_MANAGER = 1L << 5;
	/** @hide */
	public static final long TRACE_TAG_ACTIVITY_MANAGER = 1L << 6;
	/** @hide */
	public static final long TRACE_TAG_SYNC_MANAGER = 1L << 7;
	/** @hide */
	public static final long TRACE_TAG_AUDIO = 1L << 8;
	/** @hide */
	public static final long TRACE_TAG_VIDEO = 1L << 9;
	/** @hide */
	public static final long TRACE_TAG_CAMERA = 1L << 10;
	/** @hide */
	public static final long TRACE_TAG_HAL = 1L << 11;
	/** @hide */
	public static final long TRACE_TAG_APP = 1L << 12;
	/** @hide */
	public static final long TRACE_TAG_RESOURCES = 1L << 13;
	/** @hide */
	public static final long TRACE_TAG_DALVIK = 1L << 14;
	/** @hide */
	public static final long TRACE_TAG_RS = 1L << 15;
	/** @hide */
	public static final long TRACE_TAG_BIONIC = 1L << 16;
	/** @hide */
	public static final long TRACE_TAG_POWER = 1L << 17;
	/** @hide */
	public static final long TRACE_TAG_PACKAGE_MANAGER = 1L << 18;
	/** @hide */
	public static final long TRACE_TAG_SYSTEM_SERVER = 1L << 19;
	/** @hide */
	public static final long TRACE_TAG_DATABASE = 1L << 20;
	/** @hide */
	public static final long TRACE_TAG_NETWORK = 1L << 21;
	/** @hide */
	public static final long TRACE_TAG_ADB = 1L << 22;

	private static final long TRACE_TAG_NOT_READY = 1L << 63;

	private static final int MAX_SECTION_NAME_LEN = 127;




	///////////////////////////////////////////////////////////////////////////////
	static {
		// We configure two separate change callbacks, one in Trace.cpp and one here.  The
		// native callback reads the tags from the system property, and this callback
		// reads the value that the native code retrieved.  It's essential that the native
		// callback executes first.
		//
		// The system provides ordering through a priority level.  Callbacks made through
		// SystemProperties.addChangeCallback currently have a negative priority, while
		// our native code is using a priority of zero.
		SystemProperties.addChangeCallback(() -> {
			cacheEnabledTags();
			if ((sZygoteDebugFlags & Zygote.DEBUG_JAVA_DEBUGGABLE) != 0) {
				traceCounter(TRACE_TAG_ALWAYS, "java_debuggable", 1);
			}
		});
	}
	
    /**
     * Caches a copy of the enabled-tag bits.  The "master" copy is held by the native code,
     * and comes from the PROPERTY_TRACE_TAG_ENABLEFLAGS property.
     * <p>
     * If the native code hasn't yet read the property, we will cause it to do one-time
     * initialization.  We don't want to do this during class init, because this class is
     * preloaded, so all apps would be stuck with whatever the zygote saw.  (The zygote
     * doesn't see the system-property update broadcasts.)
     * <p>
     * We want to defer initialization until the first use by an app, post-zygote.
     * <p>
     * We're okay if multiple threads call here simultaneously -- the native state is
     * synchronized, and sEnabledTags is volatile (prevents word tearing).
     */
    private static long cacheEnabledTags() {
        long tags = nativeGetEnabledTags();//native函数 对应jni android_os_Trace_nativeGetEnabledTags
        sEnabledTags = tags;
        return tags;
    }
	
	/**
     * Returns true if a trace tag is enabled.
     *
     * @param traceTag The trace tag to check.
     * @return True if the trace tag is valid.
     *
     * @hide
	 * 判断traceTag对应的位，在cacheEnabledTags中是否激活
     */
    public static boolean isTagEnabled(long traceTag) {
        long tags = sEnabledTags;
		//如果 sEnabledTags == TRACE_TAG_NOT_READY，通过cacheEnabledTags重新获取值
        if (tags == TRACE_TAG_NOT_READY) {
            tags = cacheEnabledTags(); 
        }
        return (tags & traceTag) != 0;
    }
	 
	 
	/**
	 * Writes a trace message to indicate that a given section of code has
	 * begun. Must be followed by a call to {@link #traceEnd} using the same
	 * tag.
	 *
	 * @param traceTag The trace tag.
	 * @param methodName The method name to appear in the trace.
	 *
	 * @hide
	 */
	public static void traceBegin(long traceTag, String methodName) {
		if (isTagEnabled(traceTag)) {
			nativeTraceBegin(traceTag, methodName);//native函数 对应jni android_os_Trace_nativeTraceBegin
		}
	}
	 

	 /**
	 * Writes a trace message to indicate that the current method has ended.
	 * Must be called exactly once for each call to {@link #traceBegin} using the same tag.
	 *
	 * @param traceTag The trace tag.
	 *
	 * @hide
	 */
	public static void traceEnd(long traceTag) {
		if (isTagEnabled(traceTag)) {
			nativeTraceEnd(traceTag);
		}
	}
}


/////////////////////////////////////////////////////////////////
//	frameworks/base/core/jni/android_os_Trace.cpp
static jlong android_os_Trace_nativeGetEnabledTags(JNIEnv* env, jclass clazz) {
    return atrace_get_enabled_tags();
}


/**
 * Get the mask of all tags currently enabled.
 * It can be used as a guard condition around more expensive trace calculations.
 * Every trace function calls this, which ensures atrace_init is run.
 */
#define ATRACE_GET_ENABLED_TAGS() atrace_get_enabled_tags()
static inline uint64_t atrace_get_enabled_tags()
{
    atrace_init();
	//	system/core/libcutils/trace-dev.c   uint64_t atrace_enabled_tags  = ATRACE_TAG_NOT_READY;
    return atrace_enabled_tags;
}


/**
 * atrace_init readies the process for tracing by opening the trace_marker file.
 * Calling any trace function causes this to be run, so calling it is optional.
 * This can be explicitly run to avoid setup delay on first trace function.
 */
#define ATRACE_INIT() atrace_init()
static inline void atrace_init()
{
	// #   define CC_UNLIKELY( exp )  (__builtin_expect( !!(exp), false )) 	system/core/libcutils/include/cutils/compiler.h
	// atrace_is_ready  bionic/libc/include/stdatomic.h   
	//atomic_load_explicit 获取值
    if (CC_UNLIKELY(!atomic_load_explicit(&atrace_is_ready, memory_order_acquire))) {
        atrace_setup();
    }
}

//	system/core/libcutils/trace-dev.c
void atrace_setup()
{
	//static pthread_once_t   atrace_once_control  = PTHREAD_ONCE_INIT;
    pthread_once(&atrace_once_control, atrace_init_once);
}


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
	//赋值操作
    atomic_store_explicit(&atrace_is_ready, true, memory_order_release);
}

static uint64_t atrace_get_property()
{
    char value[PROPERTY_VALUE_MAX];
    char *endptr;
    uint64_t tags;

    property_get("debug.atrace.tags.enableflags", value, "0");
    errno = 0;
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
            ALOGE("Error opening /proc/self/cmdline: %s (%d)", strerror(errno),errno);
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
        if (strcmp(value, cmdline) == 0) {
            return true;
        }
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////
static void android_os_Trace_nativeTraceBegin(JNIEnv* env, jclass clazz,jlong tag, jstring nameStr) {
    ScopedStringChars jchars(env, nameStr);
    String8 utf8Chars(reinterpret_cast<const char16_t*>(jchars.get()), jchars.size());
    sanitizeString(utf8Chars);

    ALOGV("%s: %" PRId64 " %s", __FUNCTION__, tag, utf8Chars.string());
    atrace_begin(tag, utf8Chars.string());
}

/**
 * Trace the beginning of a context.  name is used to identify the context.
 * This is often used to time function execution.
 */
#define ATRACE_BEGIN(name) atrace_begin(ATRACE_TAG, name)
static inline void atrace_begin(uint64_t tag, const char* name)
{
    if (CC_UNLIKELY(atrace_is_tag_enabled(tag))) {
        void atrace_begin_body(const char*);
        atrace_begin_body(name);//system/core/libcutils/trace-dev.c
    }
}


/**
 * Test if a given tag is currently enabled.
 * Returns nonzero if the tag is enabled, otherwise zero.
 * It can be used as a guard condition around more expensive trace calculations.
 */
#define ATRACE_ENABLED() atrace_is_tag_enabled(ATRACE_TAG)
static inline uint64_t atrace_is_tag_enabled(uint64_t tag)
{
	//atrace_get_enabled_tags 在前面已经分析
    return atrace_get_enabled_tags() & tag;
}

void atrace_begin_body(const char* name)
{
    char buf[ATRACE_MESSAGE_LENGTH];

    int len = snprintf(buf, sizeof(buf), "B|%d|%s", getpid(), name);
    if (len >= (int) sizeof(buf)) {
        ALOGW("Truncated name in %s: %s\n", __FUNCTION__, name);
        len = sizeof(buf) - 1;
    }
    write(atrace_marker_fd, buf, len);
}



////////////////////////////////////////////////////////////////////////////////////////////
static void android_os_Trace_nativeTraceEnd(JNIEnv* env, jclass clazz,jlong tag) {
    ALOGV("%s: %" PRId64, __FUNCTION__, tag);
    atrace_end(tag);
}

/**
 * Trace the end of a context.
 * This should match up (and occur after) a corresponding ATRACE_BEGIN.
 */
#define ATRACE_END() atrace_end(ATRACE_TAG)
static inline void atrace_end(uint64_t tag)
{
    if (CC_UNLIKELY(atrace_is_tag_enabled(tag))) {
        void atrace_end_body();
        atrace_end_body();//system/core/libcutils/trace-dev.c
    }
}

void atrace_end_body()
{
    char c = 'E';
    write(atrace_marker_fd, &c, 1);
}



//顺便展开其他jni函数

static void android_os_Trace_nativeTraceCounter(JNIEnv* env, jclass clazz,jlong tag, jstring nameStr, jint value) {
    ScopedUtfChars name(env, nameStr);

    ALOGV("%s: %" PRId64 " %s %d", __FUNCTION__, tag, name.c_str(), value);
    atrace_int(tag, name.c_str(), value);
}

/**
 * Traces an integer counter value.  name is used to identify the counter.
 * This can be used to track how a value changes over time.
 */
#define ATRACE_INT(name, value) atrace_int(ATRACE_TAG, name, value)
static inline void atrace_int(uint64_t tag, const char* name, int32_t value)
{
    if (CC_UNLIKELY(atrace_is_tag_enabled(tag))) {
        void atrace_int_body(const char*, int32_t);
        atrace_int_body(name, value);
    }
}



void atrace_int_body(const char* name, int32_t value)
{
    WRITE_MSG("C|%d|", "|%" PRId32, getpid(), name, value);
}



static void android_os_Trace_nativeAsyncTraceBegin(JNIEnv* env, jclass clazz,jlong tag, jstring nameStr, jint cookie) {
    ScopedStringChars jchars(env, nameStr);
    String8 utf8Chars(reinterpret_cast<const char16_t*>(jchars.get()), jchars.size());
    sanitizeString(utf8Chars);

    ALOGV("%s: %" PRId64 " %s %d", __FUNCTION__, tag, utf8Chars.string(), cookie);
    atrace_async_begin(tag, utf8Chars.string(), cookie);
}


void atrace_async_begin_body(const char* name, int32_t cookie)
{
    WRITE_MSG("S|%d|", "|%" PRId32, getpid(), name, cookie);
}


static void android_os_Trace_nativeAsyncTraceEnd(JNIEnv* env, jclass clazz,jlong tag, jstring nameStr, jint cookie) {
    ScopedStringChars jchars(env, nameStr);
    String8 utf8Chars(reinterpret_cast<const char16_t*>(jchars.get()), jchars.size());
    sanitizeString(utf8Chars);

    ALOGV("%s: %" PRId64 " %s %d", __FUNCTION__, tag, utf8Chars.string(), cookie);
    atrace_async_end(tag, utf8Chars.string(), cookie);
}

void atrace_async_end_body(const char* name, int32_t cookie)
{
    WRITE_MSG("F|%d|", "|%" PRId32, getpid(), name, cookie);
}


#define WRITE_MSG(format_begin, format_end, pid, name, value) { \
    char buf[ATRACE_MESSAGE_LENGTH]; \
    int len = snprintf(buf, sizeof(buf), format_begin "%s" format_end, pid,name, value); \
    if (len >= (int) sizeof(buf)) { \
        /* Given the sizeof(buf), and all of the current format buffers, \
         * it is impossible for name_len to be < 0 if len >= sizeof(buf). */ \
        int name_len = strlen(name) - (len - sizeof(buf)) - 1; \
        /* Truncate the name to make the message fit. */ \
        ALOGW("Truncated name in %s: %s\n", __FUNCTION__, name); \
        len = snprintf(buf, sizeof(buf), format_begin "%.*s" format_end, pid,name_len, name, value); \
    } \
    write(atrace_marker_fd, buf, len); \
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    bool async = false;
    bool traceStart = true;
    bool traceStop = true;
    bool traceDump = true;
    bool traceStream = false;

    if (argc == 2 && 0 == strcmp(argv[1], "--help")) {
        showHelp(argv[0]);
        exit(0);
    }

	//查找kernel创建的trace_marker，并赋值给g_traceFolder
    if (!findTraceFiles()) {
        fprintf(stderr, "No trace folder found\n");
        exit(-1);
    }

    for (;;) {
        int ret;
        int option_index = 0;
        static struct option long_options[] = {
            {"async_start",     no_argument, 0,  0 },
            {"async_stop",      no_argument, 0,  0 },
            {"async_dump",      no_argument, 0,  0 },
            {"list_categories", no_argument, 0,  0 },
            {"stream",          no_argument, 0,  0 },
            {           0,                0, 0,  0 }
        };

        ret = getopt_long(argc, argv, "a:b:cf:k:ns:t:zo:",long_options, &option_index);

        if (ret < 0) {
            for (int i = optind; i < argc; i++) {
                if (!setCategoryEnable(argv[i], true)) {
                    fprintf(stderr, "error enabling tracing category \"%s\"\n", argv[i]);
                    exit(1);
                }
            }
            break;
        }

        switch(ret) {
            case 'a':
                g_debugAppCmdLine = optarg;
            break;

            case 'b':
                g_traceBufferSizeKB = atoi(optarg);
            break;

            case 'c':
                g_traceOverwrite = true;
            break;

            case 'f':
                g_categoriesFile = optarg;
            break;

            case 'k':
                g_kernelTraceFuncs = optarg;
            break;

            case 'n':
                g_nohup = true;
            break;

            case 's':
                g_initialSleepSecs = atoi(optarg);
            break;

            case 't':
                g_traceDurationSeconds = atoi(optarg);
            break;

            case 'z':
                g_compress = true;
            break;

            case 'o':
                g_outputFile = optarg;
            break;

            case 0:
                if (!strcmp(long_options[option_index].name, "async_start")) {
                    async = true;
                    traceStop = false;
                    traceDump = false;
                    g_traceOverwrite = true;
                } else if (!strcmp(long_options[option_index].name, "async_stop")) {
                    async = true;
                    traceStart = false;
                } else if (!strcmp(long_options[option_index].name, "async_dump")) {
                    async = true;
                    traceStart = false;
                    traceStop = false;
                } else if (!strcmp(long_options[option_index].name, "stream")) {
                    traceStream = true;
                    traceDump = false;
                } else if (!strcmp(long_options[option_index].name, "list_categories")) {
                    listSupportedCategories();
                    exit(0);
                }
            break;

            default:
                fprintf(stderr, "\n");
                showHelp(argv[0]);
                exit(-1);
            break;
        }
    }

    registerSigHandler();

    if (g_initialSleepSecs > 0) {
        sleep(g_initialSleepSecs);
    }

    bool ok = true;
    ok &= setUpTrace();
    ok &= startTrace();

    if (ok && traceStart) {
        if (!traceStream) {
            printf("capturing trace...");
            fflush(stdout);
        }

        // We clear the trace after starting it because tracing gets enabled for
        // each CPU individually in the kernel. Having the beginning of the trace
        // contain entries from only one CPU can cause "begin" entries without a
        // matching "end" entry to show up if a task gets migrated from one CPU to
        // another.
        ok = clearTrace();

        writeClockSyncMarker();
        if (ok && !async && !traceStream) {
            // Sleep to allow the trace to be captured.
            struct timespec timeLeft;
            timeLeft.tv_sec = g_traceDurationSeconds;
            timeLeft.tv_nsec = 0;
            do {
                if (g_traceAborted) {
                    break;
                }
            } while (nanosleep(&timeLeft, &timeLeft) == -1 && errno == EINTR);
        }

        if (traceStream) {
            streamTrace();
        }
    }

    // Stop the trace and restore the default settings.
    if (traceStop)
        stopTrace();

    if (ok && traceDump) {
        if (!g_traceAborted) {
            printf(" done\n");
            fflush(stdout);
            int outFd = STDOUT_FILENO;
            if (g_outputFile) {
                outFd = open(g_outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (outFd == -1) {
                printf("Failed to open '%s', err=%d", g_outputFile, errno);
            } else {
                dprintf(outFd, "TRACE:\n");
                dumpTrace(outFd);
                if (g_outputFile) {
                    close(outFd);
                }
            }
        } else {
            printf("\ntrace aborted.\n");
            fflush(stdout);
        }
        clearTrace();
    } else if (!ok) {
        fprintf(stderr, "unable to start tracing\n");
    }

    // Reset the trace buffer size to 1.
    if (traceStop)
        cleanUpTrace();

    return g_traceAborted ? 1 : 0;
}


bool findTraceFiles()
{
    static const std::string debugfs_path = "/sys/kernel/debug/tracing/";
    static const std::string tracefs_path = "/sys/kernel/tracing/";
    static const std::string trace_file = "trace_marker";

    bool tracefs = access((tracefs_path + trace_file).c_str(), F_OK) != -1;
    bool debugfs = access((debugfs_path + trace_file).c_str(), F_OK) != -1;

    if (!tracefs && !debugfs) {
        fprintf(stderr, "Error: Did not find trace folder\n");
        return false;
    }

    if (tracefs) {
        g_traceFolder = tracefs_path;
    } else {
        g_traceFolder = debugfs_path;
    }

    return true;
}

struct TracingCategory {
    // The name identifying the category.
    const char* name;

    // A longer description of the category.
    const char* longname;

    // The userland tracing tags that the category enables.
    uint64_t tags;

    // The fname==NULL terminated list of /sys/ files that the category enables.
    struct {
        // Whether the file must be writable in order to enable the tracing category.
        requiredness required;

        // The path to the enable file.
        const char* path;
    } sysfiles[MAX_SYS_FILES];//#define MAX_SYS_FILES 10
};

static bool setCategoryEnable(const char* name, bool enable)
{
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        const TracingCategory& c = k_categories[i];
        if (strcmp(name, c.name) == 0) {
            if (isCategorySupported(c)) {
                g_categoryEnables[i] = enable;
                return true;
            } else {
                if (isCategorySupportedForRoot(c)) {
                    fprintf(stderr, "error: category \"%s\" requires root ""privileges.\n", name);
                } else {
                    fprintf(stderr, "error: category \"%s\" is not supported ""on this device.\n", name);
                }
                return false;
            }
        }
    }
    fprintf(stderr, "error: unknown tracing category \"%s\"\n", name);
    return false;
}

static bool isCategorySupported(const TracingCategory& category)
{
	//	const char* k_coreServiceCategory = "core_services";
    if (strcmp(category.name, k_coreServiceCategory) == 0) {
        return !android::base::GetProperty(k_coreServicesProp, "").empty();
    }

	//	const char* k_pdxServiceCategory = "pdx";
    if (strcmp(category.name, k_pdxServiceCategory) == 0) {
        return true;
    }

    bool ok = category.tags != 0;
    for (int i = 0; i < MAX_SYS_FILES; i++) {
        const char* path = category.sysfiles[i].path;
        bool req = category.sysfiles[i].required == REQ;
        if (path != NULL) {
            if (req) {
                if (!fileIsWritable(path)) {
                    return false;
                } else {
                    ok = true;
                }
            } else {
                ok = true;
            }
        }
    }
    return ok;
}

// Check whether a file is writable.
static bool fileIsWritable(const char* filename) {
    return access((g_traceFolder + filename).c_str(), W_OK) != -1;
}


// Check whether the category would be supported on the device if the user
// were root.  This function assumes that root is able to write to any file
// that exists.  It performs the same logic as isCategorySupported, but it
// uses file existence rather than writability in the /sys/ file checks.
static bool isCategorySupportedForRoot(const TracingCategory& category)
{
    bool ok = category.tags != 0;
    for (int i = 0; i < MAX_SYS_FILES; i++) {
        const char* path = category.sysfiles[i].path;
        bool req = category.sysfiles[i].required == REQ;
        if (path != NULL) {
            if (req) {
                if (!fileExists(path)) {
                    return false;
                } else {
                    ok = true;
                }
            } else {
                ok |= fileExists(path);
            }
        }
    }
    return ok;
}


// Check whether a file exists.
static bool fileExists(const char* filename) {
    return access((g_traceFolder + filename).c_str(), F_OK) != -1;
}


static void listSupportedCategories()
{
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        const TracingCategory& c = k_categories[i];
        if (isCategorySupported(c)) {
            printf("  %10s - %s\n", c.name, c.longname);
        }
    }
}


static void registerSigHandler()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handleSignal;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void handleSignal(int /*signo*/)
{
    if (!g_nohup) {
        g_traceAborted = true;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Set all the kernel tracing settings to the desired state for this trace capture.
static bool setUpTrace()
{
    bool ok = true;

    // Set up the tracing options.
    ok &= setCategoriesEnableFromFile(g_categoriesFile);
    ok &= setTraceOverwriteEnable(g_traceOverwrite);
    ok &= setTraceBufferSizeKB(g_traceBufferSizeKB);
    // TODO: Re-enable after stabilization
    //ok &= setCmdlineSize();
    ok &= setClock();
    ok &= setPrintTgidEnableIfPresent(true);
    ok &= setKernelTraceFuncs(g_kernelTraceFuncs);

    // Set up the tags property.
    uint64_t tags = 0;
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        if (g_categoryEnables[i]) {
            const TracingCategory &c = k_categories[i];
            tags |= c.tags;
        }
    }
    ok &= setTagsProperty(tags);

    bool coreServicesTagEnabled = false;
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        if (strcmp(k_categories[i].name, k_coreServiceCategory) == 0) {
            coreServicesTagEnabled = g_categoryEnables[i];
        }

        // Set whether to poke PDX services in this session.
        if (strcmp(k_categories[i].name, k_pdxServiceCategory) == 0) {
            g_tracePdx = g_categoryEnables[i];
        }
    }

    std::string packageList(g_debugAppCmdLine);
    if (coreServicesTagEnabled) {
        if (!packageList.empty()) {
            packageList += ",";
        }
        packageList += android::base::GetProperty(k_coreServicesProp, "");
    }
    ok &= setAppCmdlineProperty(&packageList[0]);
    ok &= pokeBinderServices();
    pokeHalServices();

    if (g_tracePdx) {
        ok &= ServiceUtility::PokeServices();
    }

    // Disable all the sysfs enables.  This is done as a separate loop from
    // the enables to allow the same enable to exist in multiple categories.
    ok &= disableKernelTraceEvents();

    // Enable all the sysfs enables that are in an enabled category.
    for (size_t i = 0; i < arraysize(k_categories); i++) {
        if (g_categoryEnables[i]) {
            const TracingCategory &c = k_categories[i];
            for (int j = 0; j < MAX_SYS_FILES; j++) {
                const char* path = c.sysfiles[j].path;
                bool required = c.sysfiles[j].required == REQ;
                if (path != NULL) {
                    if (fileIsWritable(path)) {
                        ok &= setKernelOptionEnable(path, true);
                    } else if (required) {
                        fprintf(stderr, "error writing file %s\n", path);
                        ok = false;
                    }
                }
            }
        }
    }

    return ok;
}

/////////////////////////////////////////////////////////////////////////////
// Enable tracing in the kernel.
static bool startTrace()
{
    return setTracingEnabled(true);
}

// Enable or disable kernel tracing.
static bool setTracingEnabled(bool enable)
{
	// static const char* k_tracingOnPath = "tracing_on";
    return setKernelOptionEnable(k_tracingOnPath, enable);
}

// Enable or disable a kernel option by writing a "1" or a "0" into a /sys file.
static bool setKernelOptionEnable(const char* filename, bool enable)
{
    return writeStr(filename, enable ? "1" : "0");
}


///////////////////////////////////////////////////////////////////////////////////////
static void writeClockSyncMarker()
{
  char buffer[128];
  int len = 0;
  //
  //	static const char* k_traceMarkerPath = "trace_marker";
  // 	static const std::string debugfs_path = "/sys/kernel/debug/tracing/";
  int fd = open((g_traceFolder + k_traceMarkerPath).c_str(), O_WRONLY);
  if (fd == -1) {
      fprintf(stderr, "error opening %s: %s (%d)\n", k_traceMarkerPath,strerror(errno), errno);
      return;
  }
  float now_in_seconds = systemTime(CLOCK_MONOTONIC) / 1000000000.0f;

  len = snprintf(buffer, 128, "trace_event_clock_sync: parent_ts=%f\n", now_in_seconds);
  if (write(fd, buffer, len) != len) {
      fprintf(stderr, "error writing clock sync marker %s (%d)\n", strerror(errno), errno);
  }

  int64_t realtime_in_ms = systemTime(CLOCK_REALTIME) / 1000000;
  len = snprintf(buffer, 128, "trace_event_clock_sync: realtime_ts=%" PRId64 "\n", realtime_in_ms);
  if (write(fd, buffer, len) != len) {
      fprintf(stderr, "error writing clock sync marker %s (%d)\n", strerror(errno), errno);
  }

  close(fd);
}










