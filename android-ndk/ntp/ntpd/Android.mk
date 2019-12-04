LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)



LOCAL_SRC_FILES:= \
	ntpd.c	\
	../libbb/udp_io.c	\
	../libbb/xfunc_die.c	\
	../libbb/config.c	\
	../libbb/xconnect.c	\
	../libbb/xfuncs_printf.c	\
	../libbb/xfuncs.c	\
	../libbb/llist.c	\
	../libbb/getopt32.c	\
	../libbb/compare_string_array.c	\
	../libbb/full_write.c	\
	../libbb/safe_write.c	\
	../libbb/safe_strncpy.c	\
	../libbb/bb_strtonum.c	\
	../libbb/xatonum.c	\
	../libbb/time.c	\
	../libbb/signals.c	\
	../libbb/vfork_daemon_rexec.c	\
	../libbb/parse_config.c	\
	../libbb/wfopen_input.c	\
	../libbb/skip_whitespace.c	\
	../libbb/wfopen.c	\


LOCAL_MODULE:= ntpd

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libcrypto

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../include	\

LOCAL_CFLAGS := 

LOCAL_CFLAGS += \
	-Wmacro-redefined	\
	-Wno-unused-parameter \
	-Wno-empty-body \
	-Wno-missing-field-initializers \
	-Wno-attributes \
	-Wno-sign-compare \
	-Wno-pointer-sign \
	-Wpointer-arith	\
	-Wno-unused-variable	\
	-Wabsolute-value	\
	-Werror


LOCAL_INIT_RC := ntpd.rc

include $(BUILD_EXECUTABLE)

