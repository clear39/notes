/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2007 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"


time_t FAST_FUNC validate_tm_time(const char *date_str, struct tm *ptm)
{
	time_t t = mktime(ptm);
	if (t == (time_t) -1L) {
		bb_error_msg_and_die(bb_msg_invalid_date, date_str);
	}
	return t;
}

static char* strftime_fmt(char *buf, unsigned len, time_t *tp, const char *fmt)
{
	time_t t;
	if (!tp) {
		tp = &t;
		time(tp);
	}
	/* Returns pointer to NUL */
	return buf + strftime(buf, len, fmt, localtime(tp));
}

char* FAST_FUNC strftime_HHMMSS(char *buf, unsigned len, time_t *tp)
{
	return strftime_fmt(buf, len, tp, "%H:%M:%S");
}

char* FAST_FUNC strftime_YYYYMMDDHHMMSS(char *buf, unsigned len, time_t *tp)
{
	return strftime_fmt(buf, len, tp, "%Y-%m-%d %H:%M:%S");
}

#if ENABLE_MONOTONIC_SYSCALL

#include <sys/syscall.h>
/* Old glibc (< 2.3.4) does not provide this constant. We use syscall
 * directly so this definition is safe. */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* libc has incredibly messy way of doing this,
 * typically requiring -lrt. We just skip all this mess */
static void get_mono(struct timespec *ts)
{
	if (syscall(__NR_clock_gettime, CLOCK_MONOTONIC, ts))
		bb_error_msg_and_die("clock_gettime(MONOTONIC) failed");
}
unsigned long long FAST_FUNC monotonic_ns(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
unsigned long long FAST_FUNC monotonic_us(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000000ULL + ts.tv_nsec/1000;
}
unsigned long long FAST_FUNC monotonic_ms(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000ULL + ts.tv_nsec/1000000;
}
unsigned FAST_FUNC monotonic_sec(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec;
}

#else

unsigned long long FAST_FUNC monotonic_ns(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
}
unsigned long long FAST_FUNC monotonic_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}
unsigned long long FAST_FUNC monotonic_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}
unsigned FAST_FUNC monotonic_sec(void)
{
	return time(NULL);
}

#endif
