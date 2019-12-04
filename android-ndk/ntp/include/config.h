#ifndef CONFIG_H_H_
#define CONFIG_H_H_ 

#define ENABLE_FEATURE_NTP_AUTH 0

#define ENABLE_FEATURE_NTPD_SERVER 1

#define ALWAYS_INLINE inline

#define NOINLINE

#define FAST_FUNC

#define ENABLE_FEATURE_CLEAN_UP 0

#define ENABLE_FEATURE_IPV6 0
#define IF_FEATURE_IPV6(...)

#define ENABLE_FEATURE_UNIX_LOCAL 0

#define ENABLE_FEATURE_SYSLOG 0

#define ENABLE_DEBUG 0

#define ENABLE_FEATURE_NTPD_CONF 1

#define ENABLE_FEATURE_CROND_D 1

#define RETURNS_MALLOC __attribute__ ((malloc))
#define NORETURN __attribute__ ((__noreturn__))
#define UNUSED_PARAM __attribute__ ((__unused__))
# define ALIGN1 __attribute__((aligned(1)))
# define FIX_ALIASING __attribute__((__may_alias__))


#define ENABLE_LONG_OPTS 1

typedef int smallint;

#define LOG_TAG "NTPSever"
#define LOG_NDEBUG 0

#include <utils/Log.h>

#endif





