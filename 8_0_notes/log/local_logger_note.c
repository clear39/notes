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


//  @system/core/liblog/local_logger.c
LIBLOG_HIDDEN struct android_log_transport_write localLoggerWrite = {
  .node = { &localLoggerWrite.node, &localLoggerWrite.node },//初始化列表
  .context.priv = NULL,
  .name = "local",
  .available = writeToLocalAvailable,
  .open = writeToLocalInit,
  .close = writeToLocalReset,
  .write = writeToLocalWrite,
};


//	#define BLOCK_LOG_BUFFERS(id)  (((id) == LOG_ID_SECURITY) || ((id) == LOG_ID_KERNEL))
static int writeToLocalAvailable(log_id_t logId) {
#if !defined(__MINGW32__)
  uid_t uid;
#endif

  if ((logId >= NUMBER_OF_LOG_BUFFERS) || BLOCK_LOG_BUFFERS(logId)) {
    return -EINVAL;
  }

/* Android hard coded permitted, system goes to logd */
#if !defined(__MINGW32__)
  if (__android_log_transport == LOGGER_DEFAULT) {
    uid = __android_log_uid();
    if ((uid < AID_APP) && (getpwuid(uid) != NULL)) {
      return -EPERM;
    }
  }
#endif

  /* ToDo: Ask package manager for LOGD permissions */
  /* Assume we do _not_ have permissions to go to LOGD, so must go local */
  return 0;
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

//	#define NUMBER_OF_LOG_BUFFERS ((LOG_ID_SECURITY == (LOG_ID_MAX - 2)) ? LOG_ID_SECURITY : LOG_ID_KERNEL)
//  #define NUMBER_OF_LOG_BUFFERS (5==(7-2)?5:6)  //5
static struct LogBuffer {
  struct listnode head;
  pthread_rwlock_t listLock;
  char* serviceName; /* Also indicates ready by having a value */
  /* Order and proximity important for memset */
  size_t number[NUMBER_OF_LOG_BUFFERS];         /* clear memset          */
  size_t size[NUMBER_OF_LOG_BUFFERS];           /* clear memset          */
  size_t totalSize[NUMBER_OF_LOG_BUFFERS];      /* init memset           */
  size_t maxSize[NUMBER_OF_LOG_BUFFERS];        /* init MAX_SIZE_DEFAULT */
  struct listnode* last[NUMBER_OF_LOG_BUFFERS]; /* init &head            */
} logbuf = {
  .head = { &logbuf.head, &logbuf.head }, .listLock = PTHREAD_RWLOCK_INITIALIZER,
};


/*
 * return zero if permitted to log directly to logd,
 * return 1 if binder server started and
 * return negative error number if failed to start binder server.
 */
static int writeToLocalInit() {
  pthread_attr_t attr;
  struct LogBuffer* log;

  if (writeToLocalAvailable(LOG_ID_MAIN) < 0) {
    return -EPERM;
  }

  log = &logbuf;
  if (!log->serviceName) {
    LogBufferInit(log);
  }

  if (!log->serviceName) {
    LogBufferFree(log);
    return -ENOMEM;
  }

  return EPERM; /* successful local-only logging */
}

static void LogBufferInit(struct LogBuffer* log) {
  size_t i;

  pthread_rwlock_wrlock(&log->listLock);
  list_init(&log->head);
  memset(log->number, 0,sizeof(log->number) + sizeof(log->size) + sizeof(log->totalSize));
  for (i = 0; i < NUMBER_OF_LOG_BUFFERS; ++i) {
    log->maxSize[i] = MAX_SIZE_DEFAULT;	//	static const size_t MAX_SIZE_DEFAULT = 32768;
    log->last[i] = &log->head;//这里进行初始化
  }
  //	static const char baseServiceName[] = "android.logd";
#ifdef __BIONIC__
  asprintf(&log->serviceName, "%s@%d:%d", baseServiceName, __android_log_uid(),getpid());
#else
  char buffer[sizeof(baseServiceName) + 1 + 5 + 1 + 5 + 8];
  snprintf(buffer, sizeof(buffer), "%s@%d:%d", baseServiceName,__android_log_uid(), getpid());
  log->serviceName = strdup(buffer);
#endif
  pthread_rwlock_unlock(&log->listLock);
}


//	write
static int writeToLocalWrite(log_id_t logId, struct timespec* ts,struct iovec* vec, size_t nr) {
  size_t len, i;
  struct LogBufferElement* element;

  if ((logId >= NUMBER_OF_LOG_BUFFERS) || BLOCK_LOG_BUFFERS(logId)) {
    return -EINVAL;
  }

  len = 0;
  for (i = 0; i < nr; ++i) {
    len += vec[i].iov_len;
  }

  if (len > LOGGER_ENTRY_MAX_PAYLOAD) {
    len = LOGGER_ENTRY_MAX_PAYLOAD;
  }
  element = (struct LogBufferElement*)calloc(1, sizeof(struct LogBufferElement) + len + 1);
  if (!element) {
    return errno ? -errno : -ENOMEM;
  }
  element->timestamp.tv_sec = ts->tv_sec;
  element->timestamp.tv_nsec = ts->tv_nsec;
#ifdef __BIONIC__
  element->tid = gettid();
#else
  element->tid = getpid();
#endif
  element->logId = logId;
  element->len = len;

  char* cp = element->msg;
  for (i = 0; i < nr; ++i) {
    size_t iov_len = vec[i].iov_len;
    if (iov_len > len) {
      iov_len = len;
    }
    memcpy(cp, vec[i].iov_base, iov_len);
    len -= iov_len;
    if (len == 0) {
      break;
    }
    cp += iov_len;
  }

  // 	logbuf 全局变量，在 open 接口初始化
  return LogBufferLog(&logbuf, element);
}


struct LogBufferElement {
  struct listnode node;
  log_id_t logId;
  pid_t tid;
  log_time timestamp;
  unsigned short len;
  char msg[];
};



static int LogBufferLog(struct LogBuffer* log,struct LogBufferElement* element) {
  log_id_t logId = element->logId;

  pthread_rwlock_wrlock(&log->listLock);
  log->number[logId]++;
  log->size[logId] += element->len;
  log->totalSize[logId] += element->len;
  /* prune entry(s) until enough space is available */
  if (log->last[logId] == &log->head) {//如果log->last[logId] 等于 log->head，则移动为最后节点
    log->last[logId] = list_tail(&log->head);
  }
  while (log->size[logId] > log->maxSize[logId]) {
    struct listnode* node = log->last[logId];
    struct LogBufferElement* e;
    struct android_log_logger_list* logger_list;

    e = node_to_item(node, struct LogBufferElement, node);
    log->number[logId]--;
    log->size[logId] -= e->len;
    logger_list_rdlock();
    logger_list_for_each(logger_list) {
      struct android_log_transport_context* transp;

      transport_context_for_each(transp, logger_list) {
        if ((transp->transport == &localLoggerRead) && (transp->context.node == node)) {
          if (node == &log->head) {
            transp->context.node = &log->head;
          } else {
            transp->context.node = node->next;
          }
        }
      }
    }
    logger_list_unlock();
    if (node != &log->head) {
      log->last[logId] = node->prev;
    }
    list_remove(node);
    free(e);
  }
  /* add entry to list */
  list_add_head(&log->head, &element->node);
  /* ToDo: wake up all readers */
  pthread_rwlock_unlock(&log->listLock);

  return element->len;
}

