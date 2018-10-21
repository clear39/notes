//	@system/core/liblog/pmsg_writer.c
LIBLOG_HIDDEN struct android_log_transport_write pmsgLoggerWrite = {
  .node = { &pmsgLoggerWrite.node, &pmsgLoggerWrite.node },
  .context.fd = -1,
  .name = "pmsg",
  .available = pmsgAvailable,
  .open = pmsgOpen,
  .close = pmsgClose,
  .write = pmsgWrite,
};



static int pmsgOpen() {
  int fd = atomic_load(&pmsgLoggerWrite.context.fd);
  if (fd < 0) {
    int i;

    fd = TEMP_FAILURE_RETRY(open("/dev/pmsg0", O_WRONLY | O_CLOEXEC));
    i = atomic_exchange(&pmsgLoggerWrite.context.fd, fd);
    if ((i >= 0) && (i != fd)) {
      close(i);
    }
  }

  return fd;
}


static int pmsgWrite(log_id_t logId, struct timespec* ts, struct iovec* vec,size_t nr) {
  static const unsigned headerLength = 2;
  struct iovec newVec[nr + headerLength];
  android_log_header_t header;
  android_pmsg_log_header_t pmsgHeader;
  size_t i, payloadSize;
  ssize_t ret;

  if ((logId == LOG_ID_EVENTS) && !__android_log_is_debuggable()) {
    if (vec[0].iov_len < 4) {
      return -EINVAL;
    }

    if (SNET_EVENT_LOG_TAG != get4LE(vec[0].iov_base)) {
      return -EPERM;
    }
  }

  if (atomic_load(&pmsgLoggerWrite.context.fd) < 0) {
    return -EBADF;
  }

  /*
   *  struct {
   *      // what we provide to pstore
   *      android_pmsg_log_header_t pmsgHeader;
   *      // what we provide to file
   *      android_log_header_t header;
   *      // caller provides
   *      union {
   *          struct {
   *              char     prio;
   *              char     payload[];
   *          } string;
   *          struct {
   *              uint32_t tag
   *              char     payload[];
   *          } binary;
   *      };
   *  };
   */

  pmsgHeader.magic = LOGGER_MAGIC;
  pmsgHeader.len = sizeof(pmsgHeader) + sizeof(header);
  pmsgHeader.uid = __android_log_uid();
  pmsgHeader.pid = getpid();

  header.id = logId;
  header.tid = gettid();
  header.realtime.tv_sec = ts->tv_sec;
  header.realtime.tv_nsec = ts->tv_nsec;

  newVec[0].iov_base = (unsigned char*)&pmsgHeader;
  newVec[0].iov_len = sizeof(pmsgHeader);
  newVec[1].iov_base = (unsigned char*)&header;
  newVec[1].iov_len = sizeof(header);

  for (payloadSize = 0, i = headerLength; i < nr + headerLength; i++) {
    newVec[i].iov_base = vec[i - headerLength].iov_base;
    payloadSize += newVec[i].iov_len = vec[i - headerLength].iov_len;

    if (payloadSize > LOGGER_ENTRY_MAX_PAYLOAD) {
      newVec[i].iov_len -= payloadSize - LOGGER_ENTRY_MAX_PAYLOAD;
      if (newVec[i].iov_len) {
        ++i;
      }
      payloadSize = LOGGER_ENTRY_MAX_PAYLOAD;
      break;
    }
  }
  pmsgHeader.len += payloadSize;

  ret = TEMP_FAILURE_RETRY( writev(atomic_load(&pmsgLoggerWrite.context.fd), newVec, i));
  if (ret < 0) {
    ret = errno ? -errno : -ENOTCONN;
  }

  if (ret > (ssize_t)(sizeof(header) + sizeof(pmsgHeader))) {
    ret -= sizeof(header) - sizeof(pmsgHeader);
  }

  return ret;
}