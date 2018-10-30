//  @system/core/liblog/include/log/log_main.h:70
/*
 * Log macro that allows you to specify a number for the priority.
 */
#ifndef LOG_PRI
#define LOG_PRI(priority, tag, ...) android_printLog(priority, tag, __VA_ARGS__)
#endif

//  @system/core/liblog/include/log/log_main.h:306
#ifndef ALOG
#define ALOG(priority, tag, ...) LOG_PRI(ANDROID_##priority, tag, __VA_ARGS__)
#endif



//  @system/core/liblog/include/log/log_main.h:61
#define android_printLog(prio, tag, ...)  _android_log_print(prio, tag, __VA_ARGS__)


//  @system/core/liblog/logger_write.c:480
LIBLOG_ABI_PUBLIC int __android_log_print(int prio, const char* tag,const char* fmt, ...) {
  va_list ap;
  char buf[LOG_BUF_SIZE];

  va_start(ap, fmt);
  vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
  va_end(ap);

  return __android_log_write(prio, tag, buf);
}


LIBLOG_ABI_PUBLIC int __android_log_write(int prio, const char* tag,const char* msg) {
  return __android_log_buf_write(LOG_ID_MAIN, prio, tag, msg);
}