//该文档主要针对java层以及jni层日志打印
private static final String TAG = "MediaRouterService";
private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG); 


public final class Log {
  public static native boolean isLoggable(String tag, int level);
  /** @hide */ public static native int println_native(int bufID,int priority, String tag, String msg);
}



static jboolean android_util_Log_isLoggable(JNIEnv* env, jobject clazz, jstring tag, jint level)
{
    if (tag == NULL) {
        return false;
    }

    const char* chars = env->GetStringUTFChars(tag, NULL);
    if (!chars) {
        return false;
    }

    jboolean result = isLoggable(chars, level);

    env->ReleaseStringUTFChars(tag, chars);
    return result;
}

static jboolean isLoggable(const char* tag, jint level) {
    return __android_log_is_loggable(level, tag, ANDROID_LOG_INFO);
}


LIBLOG_ABI_PUBLIC int __android_log_is_loggable(int prio, const char* tag,int default_prio) {
  int logLevel = __android_log_level(tag, (tag && *tag) ? strlen(tag) : 0, default_prio); //default_prio == ANDROID_LOG_INFO
  return logLevel >= 0 && prio >= logLevel;
}

//tag = "MediaRouterService"   default_prio == ANDROID_LOG_INFO
static int __android_log_level(const char* tag, size_t len, int default_prio) {
  /* sizeof() is used on this array below */
  static const char log_namespace[] = "persist.log.tag.";
  static const size_t base_offset = 8; /* skip "persist." */
  /* calculate the size of our key temporary buffer */
  const size_t taglen = tag ? len : 0;
  /* sizeof(log_namespace) = strlen(log_namespace) + 1 */
  char key[sizeof(log_namespace) + taglen];
  char* kp;
  size_t i;
  char c = 0;
  /*
   * Single layer cache of four properties. Priorities are:
   *    log.tag.<tag>         // log.tag.MediaRouterService
   *    persist.log.tag.<tag>       // persist.log.tag.MediaRouterService
   *    log.tag
   *    persist.log.tag
   * Where the missing tag matches all tags and becomes the
   * system global default. We do not support ro.log.tag* .
   */
  static char* last_tag;
  static size_t last_tag_len;
  static uint32_t global_serial;
  /* some compilers erroneously see uninitialized use. !not_locked */
  uint32_t current_global_serial = 0;
  static struct cache_char tag_cache[2];
  static struct cache_char global_cache[2];
  int change_detected;
  int global_change_detected;
  int not_locked;

  strcpy(key, log_namespace); //key == "persist.log.tag."

  global_change_detected = change_detected = not_locked = lock();//加锁

  if (!not_locked) {//如果加锁失败
    /*
     *  check all known serial numbers to changes.
     */
    for (i = 0; i < (sizeof(tag_cache) / sizeof(tag_cache[0])); ++i) {
      if (check_cache(&tag_cache[i].cache)) { //check_cache 校验tag_cache[i].cache.serial 与 cache->pinfo
        change_detected = 1;
      }
    }
    for (i = 0; i < (sizeof(global_cache) / sizeof(global_cache[0])); ++i) {
      if (check_cache(&global_cache[i].cache)) {
        global_change_detected = 1;
      }
    }

    current_global_serial = __system_property_area_serial();//获取全局序列号值
    if (current_global_serial != global_serial) {//如果全局序列号不相等，则标志change_detected 和 global_change_detected
      change_detected = 1;
      global_change_detected = 1;
    }
  }

  if (taglen) {
    int local_change_detected = change_detected;
    if (!not_locked) {//如果获取锁失败，执行
      if (!last_tag || !last_tag[0] || (last_tag[0] != tag[0]) || strncmp(last_tag + 1, tag + 1, last_tag_len - 1)) {
        /* invalidate log.tag.<tag> cache */
        for (i = 0; i < (sizeof(tag_cache) / sizeof(tag_cache[0])); ++i) {
          tag_cache[i].cache.pinfo = NULL;
          tag_cache[i].c = '\0';
        }
        if (last_tag) last_tag[0] = '\0';
        local_change_detected = 1;
      }
      if (!last_tag || !last_tag[0]) {
        if (!last_tag) {
          last_tag = calloc(1, len + 1);
          last_tag_len = 0;
          if (last_tag) last_tag_len = len + 1;
        } else if (len >= last_tag_len) {
          last_tag = realloc(last_tag, len + 1);
          last_tag_len = 0;
          if (last_tag) last_tag_len = len + 1;
        }
        if (last_tag) {
          strncpy(last_tag, tag, len);
          last_tag[len] = '\0';
        }
      }
    }


    strncpy(key + sizeof(log_namespace) - 1, tag, len); //key == "persist.log.tag.MediaRouterService"
    key[sizeof(log_namespace) - 1 + len] = '\0';

    kp = key;//kp == "persist.log.tag.MediaRouterService"
    for (i = 0; i < (sizeof(tag_cache) / sizeof(tag_cache[0])); ++i) {
      struct cache_char* cache = &tag_cache[i];
      struct cache_char temp_cache;

      if (not_locked) {//获取锁成功
        temp_cache.cache.pinfo = NULL;
        temp_cache.c = '\0';
        cache = &temp_cache;
      }
      if (local_change_detected) {
        refresh_cache(cache, kp);//根据kp值key再system_property查找对应值
      }

      if (cache->c) {
        c = cache->c;
        break;
      }

      kp = key + base_offset; // kp == "log.tag.MediaRouterService"
    }
  } //if (taglen) 

  switch (toupper(c)) { /* if invalid, resort to global */
    case 'V':
    case 'D':
    case 'I':
    case 'W':
    case 'E':
    case 'F': /* Not officially supported */
    case 'A':
    case 'S':
    case BOOLEAN_FALSE: /* Not officially supported */
      break;
    default:
      /* clear '.' after log.tag */
      key[sizeof(log_namespace) - 2] = '\0'; //key == "persist.log.tag"

      kp = key;//kp == "persist.log.tag"
      for (i = 0; i < (sizeof(global_cache) / sizeof(global_cache[0])); ++i) {
        struct cache_char* cache = &global_cache[i];
        struct cache_char temp_cache;

        if (not_locked) {
          temp_cache = *cache;
          if (temp_cache.cache.pinfo != cache->cache.pinfo) { /* check atomic */
            temp_cache.cache.pinfo = NULL;
            temp_cache.c = '\0';
          }
          cache = &temp_cache;
        }
        if (global_change_detected) {
          refresh_cache(cache, kp);
        }

        if (cache->c) {
          c = cache->c;
          break;
        }

        kp = key + base_offset;//kp == "log.tag"
      }
      break;
  }

  if (!not_locked) {
    global_serial = current_global_serial;
    unlock();
  }

  switch (toupper(c)) {
    /* clang-format off */
    case 'V': return ANDROID_LOG_VERBOSE;
    case 'D': return ANDROID_LOG_DEBUG;
    case 'I': return ANDROID_LOG_INFO;
    case 'W': return ANDROID_LOG_WARN;
    case 'E': return ANDROID_LOG_ERROR;
    case 'F': /* FALLTHRU */ /* Not officially supported */
    case 'A': return ANDROID_LOG_FATAL;
    case BOOLEAN_FALSE: /* FALLTHRU */ /* Not Officially supported */
    case 'S': return -1; /* ANDROID_LOG_SUPPRESS */
    /* clang-format on */
  }
  return default_prio;
}



static int lock() {
  /*
   * If we trigger a signal handler in the middle of locked activity and the
   * signal handler logs a message, we could get into a deadlock state.
   */
  /*
   *  Any contention, and we can turn around and use the non-cached method
   * in less time than the system call associated with a mutex to deal with
   * the contention.
   */
  return pthread_mutex_trylock(&lock_loggable);
}

static void unlock() {
  pthread_mutex_unlock(&lock_loggable);
}

struct cache {
  const prop_info* pinfo;
  uint32_t serial;
};

struct cache_char {
  struct cache cache;
  unsigned char c;
};

static int check_cache(struct cache* cache) {
  return cache->pinfo && __system_property_serial(cache->pinfo) != cache->serial; //__system_property_serial
}


static void refresh_cache(struct cache_char* cache, const char* key) {
  char buf[PROP_VALUE_MAX];

  if (!cache->cache.pinfo) {
    cache->cache.pinfo = __system_property_find(key);
    if (!cache->cache.pinfo) {
      return;
    }
  }
  cache->cache.serial = __system_property_serial(cache->cache.pinfo);
  __system_property_read(cache->cache.pinfo, 0, buf);
  switch (buf[0]) {
    case 't':
    case 'T':
      cache->c = strcasecmp(buf + 1, "rue") ? buf[0] : BOOLEAN_TRUE;
      break;
    case 'f':
    case 'F':
      cache->c = strcasecmp(buf + 1, "alse") ? buf[0] : BOOLEAN_FALSE;
      break;
    default:
      cache->c = buf[0];
  }
}





//  bionic/libc/bionic/system_properties.cpp
// 获取system_property全局操作序列号
uint32_t __system_property_area_serial() {
  prop_area* pa = __system_property_area__;
  if (!pa) {
    return -1;
  }
  // Make sure this read fulfilled before __system_property_serial
  return atomic_load_explicit(pa->serial(), memory_order_acquire); // memory_order_acquire 内存模式
}

const prop_info* __system_property_find(const char* name) {
  if (!__system_property_area__) {
    return nullptr;
  }

  prop_area* pa = get_prop_area_for_name(name);
  if (!pa) {
    async_safe_format_log(ANDROID_LOG_ERROR, "libc", "Access denied finding property \"%s\"", name);
    return nullptr;
  }

  return pa->find(name);
}




/*
 * In class android.util.Log:
 *  public static native int println_native(int buffer, int priority, String tag, String msg)
 */
static jint android_util_Log_println_native(JNIEnv* env, jobject clazz,jint bufID, jint priority, jstring tagObj, jstring msgObj)
{
    const char* tag = NULL;
    const char* msg = NULL;

    if (msgObj == NULL) {
        jniThrowNullPointerException(env, "println needs a message");
        return -1;
    }

    if (bufID < 0 || bufID >= LOG_ID_MAX) {
        jniThrowNullPointerException(env, "bad bufID");
        return -1;
    }

    if (tagObj != NULL)
        tag = env->GetStringUTFChars(tagObj, NULL);
    msg = env->GetStringUTFChars(msgObj, NULL);

    int res = __android_log_buf_write(bufID, (android_LogPriority)priority, tag, msg);

    if (tag != NULL)
        env->ReleaseStringUTFChars(tagObj, tag);

    env->ReleaseStringUTFChars(msgObj, msg);

    return res;
}

//  @system/core/liblog/logger_write.c
LIBLOG_ABI_PUBLIC int __android_log_buf_write(int bufID, int prio,const char* tag, const char* msg) {
  struct iovec vec[3];
  char tmp_tag[32];

  if (!tag) tag = "";

  /* XXX: This needs to go! */
  if (bufID != LOG_ID_RADIO) {
    switch (tag[0]) {
      case 'H':
        if (strcmp(tag + 1, "HTC_RIL" + 1)) break;
        goto inform;
      case 'R':
        /* Any log tag with "RIL" as the prefix */
        if (strncmp(tag + 1, "RIL" + 1, strlen("RIL") - 1)) break;
        goto inform;
      case 'Q':
        /* Any log tag with "QC_RIL" as the prefix */
        if (strncmp(tag + 1, "QC_RIL" + 1, strlen("QC_RIL") - 1)) break;
        goto inform;
      case 'I':
        /* Any log tag with "IMS" as the prefix */
        if (strncmp(tag + 1, "IMS" + 1, strlen("IMS") - 1)) break;
        goto inform;
      case 'A':
        if (strcmp(tag + 1, "AT" + 1)) break;
        goto inform;
      case 'G':
        if (strcmp(tag + 1, "GSM" + 1)) break;
        goto inform;
      case 'S':
        if (strcmp(tag + 1, "STK" + 1) && strcmp(tag + 1, "SMS" + 1)) break;
        goto inform;
      case 'C':
        if (strcmp(tag + 1, "CDMA" + 1)) break;
        goto inform;
      case 'P':
        if (strcmp(tag + 1, "PHONE" + 1)) break;
      /* FALLTHRU */
      inform:
        bufID = LOG_ID_RADIO;
        snprintf(tmp_tag, sizeof(tmp_tag), "use-Rlog/RLOG-%s", tag);
        tag = tmp_tag;
      /* FALLTHRU */
      default:
        break;
    }
  }

#if __BIONIC__
  if (prio == ANDROID_LOG_FATAL) {
    android_set_abort_message(msg);
  }
#endif

  vec[0].iov_base = (unsigned char*)&prio;
  vec[0].iov_len = 1;
  vec[1].iov_base = (void*)tag;
  vec[1].iov_len = strlen(tag) + 1;
  vec[2].iov_base = (void*)msg;
  vec[2].iov_len = strlen(msg) + 1;

  //第一次调用的时候  static int (*write_to_log)(log_id_t, struct iovec* vec,size_t nr) = __write_to_log_init;
  //以后调用 write_to_log ==  __write_to_log_daemon
  return write_to_log(bufID, vec, 3); 
}


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

//  @
static int __write_to_log_daemon(log_id_t log_id, struct iovec* vec, size_t nr) {
  struct android_log_transport_write* node;
  int ret;
  struct timespec ts;
  size_t len, i;

  //计算总长度
  for (len = i = 0; i < nr; ++i) {
    len += vec[i].iov_len;
  }
  if (!len) {
    return -EINVAL;
  }

#if defined(__ANDROID__)
  clock_gettime(android_log_clockid(), &ts);

  if (log_id == LOG_ID_SECURITY) {
    if (vec[0].iov_len < 4) {
      return -EINVAL;
    }

    ret = check_log_uid_permissions();
    if (ret < 0) {
      return ret;
    }
    if (!__android_log_security()) {
      /* If only we could reset downstream logd counter */
      return -EPERM;
    }
  } else if (log_id == LOG_ID_EVENTS) {
    const char* tag;
    size_t len;
    EventTagMap *m, *f;

    if (vec[0].iov_len < 4) {
      return -EINVAL;
    }

    tag = NULL;
    len = 0;
    f = NULL;
    m = (EventTagMap*)atomic_load(&tagMap);

    if (!m) {
      ret = __android_log_trylock();
      m = (EventTagMap*)atomic_load(&tagMap); /* trylock flush cache */
      if (!m) {
        m = android_openEventTagMap(NULL);
        if (ret) { /* trylock failed, use local copy, mark for close */
          f = m;
        } else {
          if (!m) { /* One chance to open map file */
            m = (EventTagMap*)(uintptr_t)-1LL;
          }
          atomic_store(&tagMap, (uintptr_t)m);
        }
      }
      if (!ret) { /* trylock succeeded, unlock */
        __android_log_unlock();
      }
    }
    if (m && (m != (EventTagMap*)(uintptr_t)-1LL)) {
      tag = android_lookupEventTag_len(m, &len, get4LE(vec[0].iov_base));
    }
    ret = __android_log_is_loggable_len(ANDROID_LOG_INFO, tag, len,ANDROID_LOG_VERBOSE);
    if (f) { /* local copy marked for close */
      android_closeEventTagMap(f);
    }
    if (!ret) {
      return -EPERM;
    }
  } else {
    /* Validate the incoming tag, tag content can not split across iovec */
    char prio = ANDROID_LOG_VERBOSE;
    const char* tag = vec[0].iov_base;
    size_t len = vec[0].iov_len;
    if (!tag) {
      len = 0;
    }
    if (len > 0) {
      prio = *tag;
      if (len > 1) {
        --len;
        ++tag;
      } else {
        len = vec[1].iov_len;
        tag = ((const char*)vec[1].iov_base);
        if (!tag) {
          len = 0;
        }
      }
    }
    /* tag must be nul terminated */
    if (tag && strnlen(tag, len) >= len) {
      tag = NULL;
    }

    if (!__android_log_is_loggable_len(prio, tag, len - 1, ANDROID_LOG_VERBOSE)) {
      return -EPERM;
    }
  }

  
#else
  /* simulate clock_gettime(CLOCK_REALTIME, &ts); */
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
  }
#endif

  ret = 0;
  i = 1 << log_id;
  write_transport_for_each(node, &__android_log_transport_write) {
    if (node->logMask & i) {
      ssize_t retval;
      retval = (*node->write)(log_id, &ts, vec, nr);
      if (ret >= 0) {
        ret = retval;
      }
    }
  }

  write_transport_for_each(node, &__android_log_persist_write) {
    if (node->logMask & i) {
      (void)(*node->write)(log_id, &ts, vec, nr);
    }
  }

  return ret;
}

//  当log_id == LOG_ID_SECURITY
//  @system/core/liblog/properties.c


struct cache {
  const prop_info* pinfo;
  uint32_t serial;
};

struct cache_char {
  struct cache cache;
  unsigned char c;
};

/*
 * For properties that are read often, but generally remain constant.
 * Since a change is rare, we will accept a trylock failure gracefully.
 * Use a separate lock from is_loggable to keep contention down b/25563384.
 */
struct cache2_char {
  pthread_mutex_t lock;
  uint32_t serial;
  const char* key_persist;
  struct cache_char cache_persist;
  const char* key_ro;
  struct cache_char cache_ro;
  unsigned char (*const evaluate)(const struct cache2_char* self);
};

/*
 * Security state generally remains constant, but the DO must be able
 * to turn off logging should it become spammy after an attack is detected.
 */
static unsigned char evaluate_security(const struct cache2_char* self) {
  unsigned char c = self->cache_ro.c;

  return (c != BOOLEAN_FALSE) && c && (self->cache_persist.c == BOOLEAN_TRUE);
}

LIBLOG_ABI_PUBLIC int __android_log_security() {
  static struct cache2_char security = {
    PTHREAD_MUTEX_INITIALIZER, 0,
    "persist.logd.security",   { { NULL, -1 }, BOOLEAN_FALSE },
    "ro.device_owner",         { { NULL, -1 }, BOOLEAN_FALSE },
    evaluate_security
  };

  return do_cache2_char(&security);
}


static inline unsigned char do_cache2_char(struct cache2_char* self) {
  uint32_t current_serial;
  int change_detected;
  unsigned char c;

  if (pthread_mutex_trylock(&self->lock)) {
    /* We are willing to accept some race in this context */
    return self->evaluate(self);
  }

  change_detected = check_cache(&self->cache_persist.cache) || check_cache(&self->cache_ro.cache);
  current_serial = __system_property_area_serial();
  if (current_serial != self->serial) {
    change_detected = 1;
  }
  if (change_detected) {
    refresh_cache(&self->cache_persist, self->key_persist);
    refresh_cache(&self->cache_ro, self->key_ro);
    self->serial = current_serial;
  }
  c = self->evaluate(self);

  pthread_mutex_unlock(&self->lock);

  return c;
}










//日志初始化分析

//  @system/core/liblog/logger_write.c
static int __write_to_log_init(log_id_t log_id, struct iovec* vec, size_t nr) {
  //  @system/core/liblog/logger_lock.c
  __android_log_lock();//加锁

  //  static int (*write_to_log)(log_id_t, struct iovec* vec,size_t nr) = __write_to_log_init;
  if (write_to_log == __write_to_log_init) {
    int ret;

    ret = __write_to_log_initialize();
    if (ret < 0) {
      __android_log_unlock();
      if (!list_empty(&__android_log_persist_write)) {
        __write_to_log_daemon(log_id, vec, nr);
      }
      return ret;
    }

    write_to_log = __write_to_log_daemon;
  }

  __android_log_unlock();

  return write_to_log(log_id, vec, nr);
}


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

static int __write_to_log_initialize() {
  struct android_log_transport_write* transport;
  struct listnode* n;
  int i = 0, ret = 0;

  //通过 __android_log_transport 配置__android_log_transport_write 和 __android_log_persist_write
  __android_log_config_write();

  write_transport_for_each_safe(transport, n, &__android_log_transport_write) {
    __android_log_cache_available(transport);//?????
    if (!transport->logMask) {
      list_remove(&transport->node);
      continue;
    }
    if (!transport->open || ((*transport->open)() < 0)) {
      if (transport->close) {
        (*transport->close)();
      }
      list_remove(&transport->node);
      continue;
    }
    ++ret;
  }

  write_transport_for_each_safe(transport, n, &__android_log_persist_write) {
    __android_log_cache_available(transport);
    if (!transport->logMask) {
      list_remove(&transport->node);
      continue;
    }
    if (!transport->open || ((*transport->open)() < 0)) {
      if (transport->close) {
        (*transport->close)();
      }
      list_remove(&transport->node);
      continue;
    }
    ++i;
  }
  if (!ret && !i) {
    return -ENODEV;
  }

  return ret;
}

//  @system/core/liblog/include/log/log_transport.h
/* clang-format off */
#define LOGGER_DEFAULT 0x00
#define LOGGER_LOGD    0x01
#define LOGGER_KERNEL  0x02 /* Reserved/Deprecated */
#define LOGGER_NULL    0x04 /* Does not release resources of other selections */
#define LOGGER_LOCAL   0x08 /* logs sent to local memory */
#define LOGGER_STDERR  0x10 /* logs sent to stderr */
/* clang-format on */



//  @system/core/liblog/config_write.c
LIBLOG_HIDDEN void __android_log_config_write() {

  // __android_log_transport 定义 @system/core/liblog/logger_write.c
  if (__android_log_transport & LOGGER_LOCAL) {
    //  @system/core/liblog/local_logger.c
    extern struct android_log_transport_write localLoggerWrite;

    // __android_log_add_transport 定义 @system/core/liblog/logger_write.c
    // 将 localLoggerWrite 添加到 __android_log_transport_write 链表上
    __android_log_add_transport(&__android_log_transport_write,&localLoggerWrite);
  }

  if ((__android_log_transport == LOGGER_DEFAULT) || (__android_log_transport & LOGGER_LOGD)) {

//  FAKE_LOG_DEVICE == 1 定义在 @system/core/liblog/Android.bp 中
#if (FAKE_LOG_DEVICE == 0)
    extern struct android_log_transport_write logdLoggerWrite;
    extern struct android_log_transport_write pmsgLoggerWrite;

    __android_log_add_transport(&__android_log_transport_write,  &logdLoggerWrite);
    __android_log_add_transport(&__android_log_persist_write, &pmsgLoggerWrite);
#else
    
    //  @system/core/liblog/fake_writer.c
    extern struct android_log_transport_write fakeLoggerWrite;

    __android_log_add_transport(&__android_log_transport_write, &fakeLoggerWrite);
#endif


  }

  if (__android_log_transport & LOGGER_STDERR) {

    //  @system/core/liblog/stderr_write.c:58
    extern struct android_log_transport_write stderrLoggerWrite;

    /*
     * stderr logger should be primary if we can be the only one, or if
     * already in the primary list.  Otherwise land in the persist list.
     * Remember we can be called here if we are already initialized.
     */
    if (list_empty(&__android_log_transport_write)) {
      __android_log_add_transport(&__android_log_transport_write,&stderrLoggerWrite);
    } else {
      struct android_log_transport_write* transp;
      write_transport_for_each(transp, &__android_log_transport_write) {
        if (transp == &stderrLoggerWrite) {
          return;
        }
      }
      __android_log_add_transport(&__android_log_persist_write,&stderrLoggerWrite);
    }
  }
}















