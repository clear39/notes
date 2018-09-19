private static final String TAG = "MediaRouterService";
private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG); 


public final class Log {
	public static native boolean isLoggable(String tag, int level);
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
   *    log.tag.<tag>					// log.tag.MediaRouterService
   *    persist.log.tag.<tag>				// persist.log.tag.MediaRouterService
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

      kp = key + base_offset;	// kp == "log.tag.MediaRouterService"
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





//	bionic/libc/bionic/system_properties.cpp
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









