#include "libbb.h"


const char bb_busybox_exec_path[] ALIGN1 = "/proc/self/exe";

const char bb_msg_memory_exhausted[] ALIGN1 = "out of memory";

const char bb_msg_standard_output[] ALIGN1 = "standard output";

const char bb_msg_invalid_date[] ALIGN1 = "invalid date '%s'";

const char bb_msg_standard_input[] ALIGN1 = "standard input";

smallint logmode = LOGMODE_STDIO;

uint8_t xfunc_error_retval = 2;

const char bb_hexdigits_upcase[] ALIGN1 = "0123456789ABCDEF";
bool re_execed = true;

unsigned FAST_FUNC string_array_len(char **argv)
{
        char **start = argv;

        while (*argv)
                argv++;

        return argv - start;
}


void bb_show_usage(void) {

}

char* FAST_FUNC strchrnul(const char *s, int c)
{
	while (*s != '\0' && *s != c)
		s++;
	return (char*)s;
}