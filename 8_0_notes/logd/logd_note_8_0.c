//logd进程

//system/core/logd


service logd-reinit /system/bin/logd --reinit
    oneshot
    disabled
    user logd
    group logd
    writepid /dev/cpuset/system-background/tasks



// Foreground waits for exit of the main persistent threads
// that are started here. The threads are created to manage
// UNIX domain client sockets for writing, reading and
// controlling the user space logger, and for any additional
// logging plugins like auditd and restart control. Additional
// transitory per-client threads are created for each reader.
int main(int argc, char* argv[]) {
    // logd is written under the assumption(假设) that the timezone is UTC.
    // If TZ is not set, persist.sys.timezone is looked up in some time utility
    // libc functions, including mktime. It confuses(迷惑) the logd time handling,
    // so here explicitly(明确地) set TZ to UTC, which overrides the property.
    setenv("TZ", "UTC", 1);
    // issue reinit command. KISS argument parsing.
    if ((argc > 1) && argv[1] && !strcmp(argv[1], "--reinit")) {
        return issueReinit();//往logd套接字中写入reinit字符串，然后通过poll等待读取字符串是否为success
    }

    static const char dev_kmsg[] = "/dev/kmsg";
    fdDmesg = android_get_control_file(dev_kmsg);// fdDmesg == -1
    if (fdDmesg < 0) {
        fdDmesg = TEMP_FAILURE_RETRY(open(dev_kmsg, O_WRONLY | O_CLOEXEC));
    }

    int fdPmesg = -1;
    bool klogd = __android_logger_property_get_bool("logd.kernel", BOOL_DEFAULT_TRUE | BOOL_DEFAULT_FLAG_PERSIST | BOOL_DEFAULT_FLAG_ENG | BOOL_DEFAULT_FLAG_SVELTE);
    if (klogd) {
        static const char proc_kmsg[] = "/proc/kmsg";
        fdPmesg = android_get_control_file(proc_kmsg);
        if (fdPmesg < 0) {
            fdPmesg = TEMP_FAILURE_RETRY(open(proc_kmsg, O_RDONLY | O_NDELAY | O_CLOEXEC));
        }
        if (fdPmesg < 0) android::prdebug("Failed to open %s\n", proc_kmsg);
    }

    // Reinit Thread
    sem_init(&reinit, 0, 0);
    sem_init(&uidName, 0, 0);
    sem_init(&sem_name, 0, 1);
    pthread_attr_t attr;
    if (!pthread_attr_init(&attr)) {
        struct sched_param param;

        memset(&param, 0, sizeof(param));
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setschedpolicy(&attr, SCHED_BATCH);
        if (!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
            pthread_t thread;
            reinit_running = true;
            if (pthread_create(&thread, &attr, reinit_thread_start, nullptr)) {
                reinit_running = false;
            }
        }
        pthread_attr_destroy(&attr);
    }

    bool auditd = __android_logger_property_get_bool("ro.logd.auditd", BOOL_DEFAULT_TRUE);
    if (drop_privs(klogd, auditd) != 0) {
        return -1;
    }

    // Serves the purpose of managing the last logs times read on a
    // socket connection, and as a reader lock on a range of log
    // entries.

    LastLogTimes* times = new LastLogTimes();

    // LogBuffer is the object which is responsible for holding all
    // log entries.

    logBuf = new LogBuffer(times);

    signal(SIGHUP, reinit_signal_handler);

    if (__android_logger_property_get_bool("logd.statistics", BOOL_DEFAULT_TRUE | BOOL_DEFAULT_FLAG_PERSIST | BOOL_DEFAULT_FLAG_ENG |BOOL_DEFAULT_FLAG_SVELTE)) {
        logBuf->enableStatistics();
    }

    // LogReader listens on /dev/socket/logdr. When a client
    // connects, log entries in the LogBuffer are written to the client.

    LogReader* reader = new LogReader(logBuf);
    if (reader->startListener()) {
        exit(1);
    }

    // LogListener listens on /dev/socket/logdw for client
    // initiated log messages. New log entries are added to LogBuffer
    // and LogReader is notified to send updates to connected clients.

    LogListener* swl = new LogListener(logBuf, reader);
    // Backlog and /proc/sys/net/unix/max_dgram_qlen set to large value
    if (swl->startListener(600)) {
        exit(1);
    }

    // Command listener listens on /dev/socket/logd for incoming logd
    // administrative commands.

    CommandListener* cl = new CommandListener(logBuf, reader, swl);
    if (cl->startListener()) {
        exit(1);
    }

    // LogAudit listens on NETLINK_AUDIT socket for selinux
    // initiated log messages. New log entries are added to LogBuffer
    // and LogReader is notified to send updates to connected clients.

    LogAudit* al = nullptr;
    if (auditd) {
        al = new LogAudit(logBuf, reader, __android_logger_property_get_bool("ro.logd.auditd.dmesg", BOOL_DEFAULT_TRUE)? fdDmesg : -1);
    }

    LogKlog* kl = nullptr;
    if (klogd) {
        kl = new LogKlog(logBuf, reader, fdDmesg, fdPmesg, al != nullptr);
    }

    readDmesg(al, kl);

    // failure is an option ... messages are in dmesg (required by standard)

    if (kl && kl->startListener()) {
        delete kl;
    }

    if (al && al->startListener()) {
        delete al;
    }

    TEMP_FAILURE_RETRY(pause());

    exit(0);
}

static int issueReinit() {
    cap_t caps = cap_init();
    (void)cap_clear(caps);
    (void)cap_set_proc(caps);
    (void)cap_free(caps);

    int sock = TEMP_FAILURE_RETRY(socket_local_client("logd", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM));
    if (sock < 0) return -errno;

    static const char reinitStr[] = "reinit";
    ssize_t ret = TEMP_FAILURE_RETRY(write(sock, reinitStr, sizeof(reinitStr)));
    if (ret < 0) return -errno;

    struct pollfd p;
    memset(&p, 0, sizeof(p));
    p.fd = sock;
    p.events = POLLIN;
    ret = TEMP_FAILURE_RETRY(poll(&p, 1, 1000));
    if (ret < 0) return -errno;
    if ((ret == 0) || !(p.revents & POLLIN)) return -ETIME;

    static const char success[] = "success";
    char buffer[sizeof(success) - 1];
    memset(buffer, 0, sizeof(buffer));
    ret = TEMP_FAILURE_RETRY(read(sock, buffer, sizeof(buffer)));
    if (ret < 0) return -errno;

    return strncmp(buffer, success, sizeof(success) - 1) != 0;
}


int android_get_control_file(const char* path) {
    //system/core/libcutils/include/cutils/android_get_control_file.h:20:#define ANDROID_FILE_ENV_PREFIX "ANDROID_FILE_"
    int fd = __android_get_control_from_env(ANDROID_FILE_ENV_PREFIX, path);

#if defined(__linux__)
    // Find file path from /proc and make sure it is correct
    char *proc = NULL;
    if (asprintf(&proc, "/proc/self/fd/%d", fd) < 0) return -1;
    if (!proc) return -1;

    size_t len = strlen(path);
    // readlink() does not guarantee a nul byte, len+2 so we catch truncation.
    char *buf = static_cast<char *>(calloc(1, len + 2));
    if (!buf) {
        free(proc);
        return -1;
    }
    ssize_t ret = TEMP_FAILURE_RETRY(readlink(proc, buf, len + 1));
    free(proc);
    int cmp = (len != static_cast<size_t>(ret)) || strcmp(buf, path);
    free(buf);
    if (ret < 0) return -1;
    if (cmp != 0) return -1;
    // It is what we think it is
#endif

    return fd;
}


LIBCUTILS_HIDDEN int __android_get_control_from_env(const char* prefix,const char* name) {
    if (!prefix || !name) return -1;

    char *key = NULL;
    if (asprintf(&key, "%s%s", prefix, name) < 0) return -1; //ANDROID_FILE_/dev/kmsg
    if (!key) return -1;

    char *cp = key;
    while (*cp) {
        if (!isalnum(*cp)) *cp = '_';//isalnum判断是否为字母和数字，如果不是替换成下划线；这里得到ANDROID_FILE__dev_kmsg
        ++cp;
    }

    const char* val = getenv(key);// key == ANDROID_FILE__dev_kmsg
    free(key);
    if (!val) return -1;

    errno = 0;
    long fd = strtol(val, NULL, 10);
    if (errno) return -1;

    // validity checking
    if ((fd < 0) || (fd > INT_MAX)) return -1;

    // Since we are inheriting an fd, it could legitimately exceed _SC_OPEN_MAX

    // Still open?
#if defined(F_GETFD) // Lowest overhead
    if (TEMP_FAILURE_RETRY(fcntl(fd, F_GETFD)) < 0) return -1;
#elif defined(F_GETFL) // Alternate lowest overhead
    if (TEMP_FAILURE_RETRY(fcntl(fd, F_GETFL)) < 0) return -1;
#else // Hail Mary pass
    struct stat s;
    if (TEMP_FAILURE_RETRY(fstat(fd, &s)) < 0) return -1;
#endif

    return static_cast<int>(fd);
}





//__android_logger_property_get_bool("logd.kernel", BOOL_DEFAULT_TRUE | BOOL_DEFAULT_FLAG_PERSIST | BOOL_DEFAULT_FLAG_ENG | BOOL_DEFAULT_FLAG_SVELTE);
/* get boolean with the logger twist that supports eng adjustments */
LIBLOG_ABI_PRIVATE bool __android_logger_property_get_bool(const char* key,int flag) {
  struct cache_property property = { { NULL, -1 }, { 0 } };
  if (flag & BOOL_DEFAULT_FLAG_PERSIST) { //为true 
    char newkey[strlen("persist.") + strlen(key) + 1];
    snprintf(newkey, sizeof(newkey), "ro.%s", key);// newkey == ro.logd.kernel
    refresh_cache_property(&property, newkey);
    property.cache.pinfo = NULL;
    property.cache.serial = -1;
    snprintf(newkey, sizeof(newkey), "persist.%s", key);// newkey == persist.logd.kernel
    refresh_cache_property(&property, newkey);
    property.cache.pinfo = NULL;
    property.cache.serial = -1;
  }

  refresh_cache_property(&property, key);

  if (check_flag(property.property, "true")) {
    return true;
  }
  if (check_flag(property.property, "false")) {
    return false;
  }
  if (property.property[0]) {
    flag &= ~(BOOL_DEFAULT_FLAG_ENG | BOOL_DEFAULT_FLAG_SVELTE);
  }
  if (check_flag(property.property, "eng")) {
    flag |= BOOL_DEFAULT_FLAG_ENG;
  }
  /* this is really a "not" flag */
  if (check_flag(property.property, "svelte")) {
    flag |= BOOL_DEFAULT_FLAG_SVELTE;
  }

  /* Sanity Check */
  if (flag & (BOOL_DEFAULT_FLAG_SVELTE | BOOL_DEFAULT_FLAG_ENG)) {
    flag &= ~BOOL_DEFAULT_FLAG_TRUE_FALSE;
    flag |= BOOL_DEFAULT_TRUE;
  }

  if ((flag & BOOL_DEFAULT_FLAG_SVELTE) &&  __android_logger_property_get_bool("ro.config.low_ram",BOOL_DEFAULT_FALSE)) {
    return false;
  }
  if ((flag & BOOL_DEFAULT_FLAG_ENG) && !__android_log_is_debuggable()) {
    return false;
  }

  return (flag & BOOL_DEFAULT_FLAG_TRUE_FALSE) != BOOL_DEFAULT_FALSE;
}

//	bionic/libc/bionic/system_properties.cpp
struct prop_info {
  atomic_uint_least32_t serial;
  // we need to keep this buffer around because the property
  // value can be modified whereas name is constant.
  char value[PROP_VALUE_MAX];
  char name[0];

  prop_info(const char* name, uint32_t namelen, const char* value, uint32_t valuelen) {
    memcpy(this->name, name, namelen);
    this->name[namelen] = '\0';
    atomic_init(&this->serial, valuelen << 24);
    memcpy(this->value, value, valuelen);
    this->value[valuelen] = '\0';
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(prop_info);
};

struct cache {
  const prop_info* pinfo;
  uint32_t serial;
};

/* cache structure */
struct cache_property {
  struct cache cache;
  char property[PROP_VALUE_MAX];
};

static void refresh_cache_property(struct cache_property* cache,const char* key) {
  if (!cache->cache.pinfo) {
    cache->cache.pinfo = __system_property_find(key);
    if (!cache->cache.pinfo) {
      return;
    }
  }
  cache->cache.serial = __system_property_serial(cache->cache.pinfo);
  __system_property_read(cache->cache.pinfo, 0, cache->property);
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


class context_node {
 public:
  context_node(context_node* next, const char* context, prop_area* pa)
      : next(next), context_(strdup(context)), pa_(pa), no_access_(false) {
    lock_.init(false);
  }

  ~context_node() {
    unmap();
    free(context_);
  }

  bool open(bool access_rw, bool* fsetxattr_failed);
  bool check_access_and_open();
  void reset_access();

  const char* context() const {
    return context_;
  }
  prop_area* pa() {
    return pa_;
  }

  context_node* next;

 private:
  bool check_access();
  void unmap();

  Lock lock_;
  char* context_;
  prop_area* pa_;
  bool no_access_;
};

struct prefix_node {
  prefix_node(struct prefix_node* next, const char* prefix, context_node* context)
      : prefix(strdup(prefix)), prefix_len(strlen(prefix)), context(context), next(next) {
  }
  ~prefix_node() {
    free(prefix);
  }
  char* prefix;
  const size_t prefix_len;
  context_node* context;
  struct prefix_node* next;
};


static prop_area* get_prop_area_for_name(const char* name) {
  //	bionic/libc/bionic/system_properties.cpp:790:static prefix_node* prefixes = nullptr;
  auto entry = list_find(prefixes, [name](prefix_node* l) {// c++ Lambda 表达式
    return l->prefix[0] == '*' || !strncmp(l->prefix, name, l->prefix_len);
  });

  if (!entry) {
    return nullptr;
  }

  auto cnode = entry->context;
  if (!cnode->pa()) {
    /*
     * We explicitly do not check no_access_ in this case because unlike the
     * case of foreach(), we want to generate an selinux audit for each
     * non-permitted property access in this function.
     */
    cnode->open(false, nullptr);
  }
  return cnode->pa();
}

template <typename List, typename Func>
static List* list_find(List* list, Func func) {
  while (list) {
    if (func(list)) {
      return list;
    }
    list = list->next;
  }
  return nullptr;
}



// Wait for non-locked serial, and retrieve it with acquire semantics.
uint32_t __system_property_serial(const prop_info* pi) {
  uint32_t serial = load_const_atomic(&pi->serial, memory_order_acquire);
  while (SERIAL_DIRTY(serial)) {
    __futex_wait(const_cast<_Atomic(uint_least32_t)*>(&pi->serial), serial, nullptr);
    serial = load_const_atomic(&pi->serial, memory_order_acquire);
  }
  return serial;
}




















