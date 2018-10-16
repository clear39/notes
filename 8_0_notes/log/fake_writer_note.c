//  @system/core/liblog/logger.h
/* Union, sock or fd of zero is not allowed unless static initialized */
union android_log_context {
  void* priv;
  atomic_int sock;
  atomic_int fd;
  struct listnode* node;
  atomic_uintptr_t atomic_pointer;
};


//  @system/core/liblog/logger.h
struct android_log_transport_write {
  struct listnode node;
  const char* name;                  /* human name to describe the transport */
  unsigned logMask;                  /* mask cache of available() success */
  union android_log_context context; /* Initialized by static allocation */

  int (*available)(log_id_t logId); /* Does not cause resources to be taken */
  int (*open)();   /* can be called multiple times, reusing current resources */
  void (*close)(); /* free up resources */
  /* write log to transport, returns number of bytes propagated, or -errno */
  int (*write)(log_id_t logId, struct timespec* ts, struct iovec* vec,size_t nr);
};



//	@system/core/liblog/fake_writer.c
LIBLOG_HIDDEN struct android_log_transport_write fakeLoggerWrite = {
  .node = { &fakeLoggerWrite.node, &fakeLoggerWrite.node },
  .context.priv = &logFds,
  .name = "fake",
  .available = NULL,
  .open = fakeOpen,
  .close = fakeClose,
  .write = fakeWrite,
};


//	@system/core/liblog/logger_name.c
/* In the future, we would like to make this list extensible */
static const char* LOG_NAME[LOG_ID_MAX] = {
  /* clang-format off */
  [LOG_ID_MAIN] = "main",
  [LOG_ID_RADIO] = "radio",
  [LOG_ID_EVENTS] = "events",
  [LOG_ID_SYSTEM] = "system",
  [LOG_ID_CRASH] = "crash",
  [LOG_ID_SECURITY] = "security",
  [LOG_ID_KERNEL] = "kernel",
  /* clang-format on */
};


//  @system/core/liblog/include/log/log_id.h
typedef enum log_id {
  LOG_ID_MIN = 0,

  LOG_ID_MAIN = 0,
  LOG_ID_RADIO = 1,
  LOG_ID_EVENTS = 2,
  LOG_ID_SYSTEM = 3,
  LOG_ID_CRASH = 4,
  LOG_ID_SECURITY = 5,
  LOG_ID_KERNEL = 6, /* place last, third-parties can not use it */

  LOG_ID_MAX
} log_id_t;

LIBLOG_ABI_PUBLIC const char* android_log_id_to_name(log_id_t log_id) {
  if (log_id >= LOG_ID_MAX) {
    log_id = LOG_ID_MAIN;
  }
  return LOG_NAME[log_id];
}


static int fakeOpen() {
  int i;

  for (i = 0; i < LOG_ID_MAX; i++) {
    /*
     * Known maximum size string, plus an 8 character margin to deal with
     * possible independent changes to android_log_id_to_name().
     */
    char buf[sizeof("/dev/log_security") + 8];
    if (logFds[i] >= 0) {
      continue;
    }
    snprintf(buf, sizeof(buf), "/dev/log_%s", android_log_id_to_name(i));//以main为例：/dev/log_main
    logFds[i] = fakeLogOpen(buf);
    if (logFds[i] < 0) {
      fprintf(stderr, "fakeLogOpen(%s) failed\n", buf);
    }
  }
  return 0;
}





//	@system/core/liblog/fake_log_device.c

/* from the long-dead utils/Log.cpp */
typedef enum {
  FORMAT_OFF = 0,
  FORMAT_BRIEF,
  FORMAT_PROCESS,
  FORMAT_TAG,
  FORMAT_THREAD,
  FORMAT_RAW,
  FORMAT_TIME,
  FORMAT_THREADTIME,
  FORMAT_LONG
} LogFormat;

/*
 * Log driver state.
 */
typedef struct LogState {
  /* the fake fd that's seen by the user */
  int fakeFd;

  /* a printable name for this fake device */
  char debugName[sizeof("/dev/log/security")];

  /* nonzero if this is a binary log */
  int isBinary;

  /* global minimum priority */
  int globalMinPriority;

  /* output format */
  LogFormat outputFormat;

  /* tags and priorities */
  struct {
    char tag[kMaxTagLen];
    int minPriority;
  } tagSet[kTagSetSize];
} LogState;


/*
 * Open a log output device and return a fake fd.
 */
LIBLOG_HIDDEN int fakeLogOpen(const char* pathName) {
  LogState* logState;
  int fd = -1;

  lock();//加锁

  logState = createLogState();//在openLogTable中查找一个fakeFd为0的成员返回
  if (logState != NULL) {
    configureInitialState(pathName, logState);
    fd = logState->fakeFd;
  } else {
    errno = ENFILE;
  }

  unlock();

  return fd;
}



/*
 * Allocate an fd and associate a new LogState with it.
 * The fd is available via the fakeFd field of the return value.
 */
//在openLogTable中查找一个fakeFd为0的成员返回
static LogState* createLogState() {
  size_t i;

  for (i = 0; i < (sizeof(openLogTable) / sizeof(openLogTable[0])); i++) {
    if (openLogTable[i].fakeFd == 0) {
      openLogTable[i].fakeFd = FAKE_FD_BASE + i;//	#define FAKE_FD_BASE 10000
      return &openLogTable[i];
    }
  }
  return NULL;
}





/*
 * Configure logging based on ANDROID_LOG_TAGS environment variable.  We
 * need to parse a string that looks like
 *
 *   *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
 *
 * The tag (or '*' for the global level) comes first, followed by a colon
 * and a letter indicating the minimum priority level we're expected to log.
 * This can be used to reveal or conceal logs with specific tags.
 *
 * We also want to check ANDROID_PRINTF_LOG to determine how the output
 * will look.
 */
static void configureInitialState(const char* pathName="/dev/log_xxxx", LogState* logState) {
  static const int kDevLogLen = sizeof("/dev/log/") - 1;

  strncpy(logState->debugName, pathName, sizeof(logState->debugName));// logState->debugName 大小 sizeof("/dev/log/security")
  logState->debugName[sizeof(logState->debugName) - 1] = '\0';

  /* identify binary logs */
  if (!strcmp(pathName + kDevLogLen, "events") || !strcmp(pathName + kDevLogLen, "security")) {
    logState->isBinary = 1;
  }

  /* global min priority defaults to "info" level */
  logState->globalMinPriority = ANDROID_LOG_INFO;

  /*
   * This is based on the the long-dead utils/Log.cpp code.
   */
  const char* tags = getenv("ANDROID_LOG_TAGS");
  TRACE("Found ANDROID_LOG_TAGS='%s'\n", tags);
  if (tags != NULL) {
    int entry = 0;

    while (*tags != '\0') {
      char tagName[kMaxTagLen];
      int i, minPrio;

      while (isspace(*tags)) tags++;

      i = 0;
      while (*tags != '\0' && !isspace(*tags) && *tags != ':' && i < kMaxTagLen) {
        tagName[i++] = *tags++;
      }
      if (i == kMaxTagLen) {
        TRACE("ERROR: env tag too long (%d chars max)\n", kMaxTagLen - 1);
        return;
      }
      tagName[i] = '\0';

      /* default priority, if there's no ":" part; also zero out '*' */
      minPrio = ANDROID_LOG_VERBOSE;
      if (tagName[0] == '*' && tagName[1] == '\0') {
        minPrio = ANDROID_LOG_DEBUG;
        tagName[0] = '\0';
      }

      if (*tags == ':') {
        tags++;
        if (*tags >= '0' && *tags <= '9') {
          if (*tags >= ('0' + ANDROID_LOG_SILENT))
            minPrio = ANDROID_LOG_VERBOSE;
          else
            minPrio = *tags - '\0';
        } else {
          switch (*tags) {
            case 'v':
              minPrio = ANDROID_LOG_VERBOSE;
              break;
            case 'd':
              minPrio = ANDROID_LOG_DEBUG;
              break;
            case 'i':
              minPrio = ANDROID_LOG_INFO;
              break;
            case 'w':
              minPrio = ANDROID_LOG_WARN;
              break;
            case 'e':
              minPrio = ANDROID_LOG_ERROR;
              break;
            case 'f':
              minPrio = ANDROID_LOG_FATAL;
              break;
            case 's':
              minPrio = ANDROID_LOG_SILENT;
              break;
            default:
              minPrio = ANDROID_LOG_DEFAULT;
              break;
          }
        }

        tags++;
        if (*tags != '\0' && !isspace(*tags)) {
          TRACE("ERROR: garbage in tag env; expected whitespace\n");
          TRACE("       env='%s'\n", tags);
          return;
        }
      }

      if (tagName[0] == 0) {
        logState->globalMinPriority = minPrio;
        TRACE("+++ global min prio %d\n", logState->globalMinPriority);
      } else {
        logState->tagSet[entry].minPriority = minPrio;
        strcpy(logState->tagSet[entry].tag, tagName);
        TRACE("+++ entry %d: %s:%d\n", entry, logState->tagSet[entry].tag,logState->tagSet[entry].minPriority);
        entry++;
      }
    }
  }

  /*
   * Taken from the long-dead utils/Log.cpp
   */
  const char* fstr = getenv("ANDROID_PRINTF_LOG");
  LogFormat format;
  if (fstr == NULL) {
    format = FORMAT_BRIEF;
  } else {
    if (strcmp(fstr, "brief") == 0)
      format = FORMAT_BRIEF;
    else if (strcmp(fstr, "process") == 0)
      format = FORMAT_PROCESS;
    else if (strcmp(fstr, "tag") == 0)
      format = FORMAT_PROCESS;
    else if (strcmp(fstr, "thread") == 0)
      format = FORMAT_PROCESS;
    else if (strcmp(fstr, "raw") == 0)
      format = FORMAT_PROCESS;
    else if (strcmp(fstr, "time") == 0)
      format = FORMAT_PROCESS;
    else if (strcmp(fstr, "long") == 0)
      format = FORMAT_PROCESS;
    else
      format = (LogFormat)atoi(fstr);  // really?!
  }

  logState->outputFormat = format;
}




static int fakeWrite(log_id_t log_id, struct timespec* ts __unused,struct iovec* vec, size_t nr) {
  ssize_t ret;
  size_t i;
  int logFd, len;

  if (/*(int)log_id >= 0 &&*/ (int)log_id >= (int)LOG_ID_MAX) {
    return -EINVAL;
  }

  len = 0;
  for (i = 0; i < nr; ++i) {
    len += vec[i].iov_len;
  }

  if (len > LOGGER_ENTRY_MAX_PAYLOAD) {
    len = LOGGER_ENTRY_MAX_PAYLOAD;
  }

  logFd = logFds[(int)log_id];
  ret = TEMP_FAILURE_RETRY(fakeLogWritev(logFd, vec, nr));
  if (ret < 0) {
    ret = -errno;
  } else if (ret > len) {
    ret = len;
  }

  return ret;
}




//	@system/core/liblog/fake_log_device.c

/*
 * Translate an fd to a LogState.
 */
static LogState* fdToLogState(int fd) {
  if (fd >= FAKE_FD_BASE && fd < FAKE_FD_BASE + MAX_OPEN_LOGS) {
    return &openLogTable[fd - FAKE_FD_BASE];
  }
  return NULL;
}



/*
 * Receive a log message.  We happen to know that "vector" has three parts:
 *
 *  priority (1 byte)
 *  tag (N bytes -- null-terminated ASCII string)
 *  message (N bytes -- null-terminated ASCII string)
 */
LIBLOG_HIDDEN ssize_t fakeLogWritev(int fd, const struct iovec* vector,int count) {
  LogState* state;

  /* Make sure that no-one frees the LogState while we're using it.
   * Also guarantees that only one thread is in showLog() at a given
   * time (if it matters).
   */
  lock();

  state = fdToLogState(fd);
  if (state == NULL) {
    errno = EBADF;
    goto error;
  }

  if (state->isBinary) {
    TRACE("%s: ignoring binary log\n", state->debugName);
    goto bail;
  }

  if (count != 3) {
    TRACE("%s: writevLog with count=%d not expected\n", state->debugName, count);
    goto error;
  }

  /* pull out the three fields */
  int logPrio = *(const char*)vector[0].iov_base;
  const char* tag = (const char*)vector[1].iov_base;
  const char* msg = (const char*)vector[2].iov_base;

  /* see if this log tag is configured */
  int i;
  int minPrio = state->globalMinPriority;
  for (i = 0; i < kTagSetSize; i++) {
    if (state->tagSet[i].minPriority == ANDROID_LOG_UNKNOWN)
      break; /* reached end of configured values */

    if (strcmp(state->tagSet[i].tag, tag) == 0) {
      // TRACE("MATCH tag '%s'\n", tag);
      minPrio = state->tagSet[i].minPriority;
      break;
    }
  }

  if (logPrio >= minPrio) {
    showLog(state, logPrio, tag, msg);
  } else {
    // TRACE("+++ NOLOG(%d): %s %s", logPrio, tag, msg);
  }

bail:
  unlock();
  int len = 0;
  for (i = 0; i < count; ++i) {
    len += vector[i].iov_len;
  }
  return len;

error:
  unlock();
  return -1;
}