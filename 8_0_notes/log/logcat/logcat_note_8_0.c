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

/* Union, sock or fd of zero is not allowed unless static initialized */
union android_log_context {
  void* priv;
  atomic_int sock;
  atomic_int fd;
  struct listnode* node;
  atomic_uintptr_t atomic_pointer;
};

struct android_log_transport_write {
  struct listnode node;
  const char* name;                  /* human name to describe the transport */
  unsigned logMask;                  /* mask cache of available() success */
  union android_log_context context; /* Initialized by static allocation */

  int (*available)(log_id_t logId); /* Does not cause resources to be taken */
  int (*open)();   /* can be called multiple times, reusing current resources */
  void (*close)(); /* free up resources */
  /* write log to transport, returns number of bytes propagated, or -errno */
  int (*write)(log_id_t logId, struct timespec* ts, struct iovec* vec, size_t nr);
};


static int __write_to_log_init(log_id_t, struct iovec* vec, size_t nr);
static int (*write_to_log)(log_id_t, struct iovec* vec,size_t nr) = __write_to_log_init;






/* log_init_lock assumed */
static int __write_to_log_initialize() {
  struct android_log_transport_write* transport;
  struct listnode* n;
  int i = 0, ret = 0;

  __android_log_config_write();

  write_transport_for_each_safe(transport, n, &__android_log_transport_write) {
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



LIBLOG_HIDDEN void __android_log_config_write() {

  if (__android_log_transport & LOGGER_LOCAL) {
    extern struct android_log_transport_write localLoggerWrite;
    __android_log_add_transport(&__android_log_transport_write,&localLoggerWrite);
  }

  if ((__android_log_transport == LOGGER_DEFAULT) || (__android_log_transport & LOGGER_LOGD)) {
#if (FAKE_LOG_DEVICE == 0)
    extern struct android_log_transport_write logdLoggerWrite;
    extern struct android_log_transport_write pmsgLoggerWrite;

    __android_log_add_transport(&__android_log_transport_write, &logdLoggerWrite);
    __android_log_add_transport(&__android_log_persist_write, &pmsgLoggerWrite);
#else
    extern struct android_log_transport_write fakeLoggerWrite;
    __android_log_add_transport(&__android_log_transport_write,&fakeLoggerWrite);
#endif
  }

  if (__android_log_transport & LOGGER_STDERR) {
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


static void __android_log_add_transport(struct listnode* list, struct android_log_transport_write* transport) {
  size_t i;

  /* Try to keep one functioning transport for each log buffer id */
  for (i = LOG_ID_MIN; i < LOG_ID_MAX; i++) {
    struct android_log_transport_write* transp;

    if (list_empty(list)) {
      if (!transport->available || ((*transport->available)(i) >= 0)) {
        list_add_tail(list, &transport->node);
        return;
      }
    } else {
      write_transport_for_each(transp, list) {
        if (!transp->available) {
          return;
        }
        if (((*transp->available)(i) < 0) && (!transport->available || ((*transport->available)(i) >= 0))) {
          list_add_tail(list, &transport->node);
          return;
        }
      }
    }
  }
}




