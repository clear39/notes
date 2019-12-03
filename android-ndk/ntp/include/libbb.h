#ifndef LIBBB_H
#define LIBBB_H 1

#include "config.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <paths.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
/* There are two incompatible basename's, let's not use them! */
/* See the dirname/basename man page for details */
#include <libgen.h> /* dirname,basename */
#undef basename
#define basename dont_use_basename
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>


#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <sys/param.h>
#include <pwd.h>
#include <grp.h>

/* Don't do this here:
 * #include <sys/sysinfo.h>
 * Some linux/ includes pull in conflicting definition
 * of struct sysinfo (only in some toolchanins), which breaks build.
 * Include sys/sysinfo.h only in those files which need it.
 */

#if ENABLE_LOCALE_SUPPORT
# include <locale.h>
#else
# define setlocale(x,y) ((void)0)
#endif
#ifdef DMALLOC
# include <dmalloc.h>
#endif
/* Just in case libc doesn't define some of these... */
#ifndef _PATH_PASSWD
#define _PATH_PASSWD  "/etc/passwd"
#endif
#ifndef _PATH_GROUP
#define _PATH_GROUP   "/etc/group"
#endif
#ifndef _PATH_SHADOW
#define _PATH_SHADOW  "/etc/shadow"
#endif
#ifndef _PATH_GSHADOW
#define _PATH_GSHADOW "/etc/gshadow"
#endif
#if defined __FreeBSD__ || defined __OpenBSD__
# include <netinet/in.h>
# include <arpa/inet.h>
#elif defined __APPLE__
# include <netinet/in.h>
#else
# include <arpa/inet.h>
//This breaks on bionic:
//# if !defined(__socklen_t_defined) && !defined(_SOCKLEN_T_DECLARED)
///* We #define socklen_t *after* includes, otherwise we get
// * typedef redefinition errors from system headers
// * (in case "is it defined already" detection above failed)
// */
//#  define socklen_t bb_socklen_t
//   typedef unsigned socklen_t;
//# endif
//if this is still needed, add a fix along the lines of
//  ifdef SPECIFIC_BROKEN_LIBC_CHECK / typedef socklen_t / endif
//in platform.h instead!
#endif
#ifndef HAVE_CLEARENV
# define clearenv() do { if (environ) environ[0] = NULL; } while (0)
#endif
#ifndef HAVE_FDATASYNC
# define fdatasync fsync
#endif

#define TERMIOS_CLEAR_ISIG      (1 << 0)
#define TERMIOS_RAW_CRNL_INPUT  (1 << 1)
#define TERMIOS_RAW_CRNL_OUTPUT (1 << 2)
#define TERMIOS_RAW_CRNL        (TERMIOS_RAW_CRNL_INPUT|TERMIOS_RAW_CRNL_OUTPUT)
#define TERMIOS_RAW_INPUT       (1 << 3)

#define bb_dev_null "/dev/null"




//  @   ../libbb/bb_strtonum.c
unsigned bb_strtou(const char *arg, char **endp, int base) FAST_FUNC;


// ./libbb/messages.c
extern const char bb_hexdigits_upcase[] ALIGN1;

ssize_t safe_write(int fd, const void *buf, size_t count) FAST_FUNC;
// NB: will return short write on error, not -1,
// if some data was written before error occurred
ssize_t full_write(int fd, const void *buf, size_t count) FAST_FUNC;

void overlapping_strcpy(char *dst, const char *src) FAST_FUNC;


//  @   ./libbb/appletlib.c
unsigned FAST_FUNC string_array_len(char **argv);



void bb_show_usage(void);

#define  bb_error_msg  ALOGE
#define  bb_perror_msg_and_die ALOGE
#define  bb_error_msg_and_die ALOGE
#define bb_perror_msg ALOGE
#define bb_verror_msg ALOGE
#define bb_simple_perror_msg_and_die ALOGE


extern const char bb_busybox_exec_path[] ALIGN1;
extern const char bb_msg_memory_exhausted[] ALIGN1;
extern const char bb_msg_standard_output[] ALIGN1;
extern const char bb_msg_invalid_date[] ALIGN1;

void FAST_FUNC bb_die_memory_exhausted(void);

void bb_daemonize_or_rexec(int flags, char **argv) FAST_FUNC;
pid_t safe_waitpid(pid_t pid, int *wstat, int options) FAST_FUNC;
#define BB_EXECVP(prog,cmd)     execvp(prog,cmd)
pid_t spawn(char **argv) FAST_FUNC;
int FAST_FUNC fflush_all(void);


// @    ./libbb/signals.c
extern smallint bb_got_signal;
void kill_myself_with_sig(int sig) NORETURN FAST_FUNC;
void bb_signals(int sigs, void (*f)(int)) FAST_FUNC;
void record_signo(int signo); /* not FAST_FUNC! */

enum {
	LOGMODE_NONE = 0,
	LOGMODE_STDIO = (1 << 0),
	LOGMODE_SYSLOG = (1 << 1) * ENABLE_FEATURE_SYSLOG,
	LOGMODE_BOTH = LOGMODE_SYSLOG + LOGMODE_STDIO,
};

extern smallint logmode;


//  @   ./libbb/time.c
char *strftime_YYYYMMDDHHMMSS(char *buf, unsigned len, time_t *tp) FAST_FUNC;

char *is_prefixed_with(const char *string, const char *key) FAST_FUNC;

int xatoi_positive(const char *numstr) FAST_FUNC;


enum { wrote_pidfile = 0 };
#define write_pidfile_std_path_and_ext(path)  ((void)0)
#define remove_pidfile_std_path_and_ext(path) ((void)0)
#define write_pidfile(path)  ((void)0)
#define remove_pidfile(path) ((void)0)

enum {
	DAEMON_CHDIR_ROOT      = 1 << 0,
	DAEMON_DEVNULL_STDIO   = 1 << 1,
	DAEMON_CLOSE_EXTRA_FDS = 1 << 2,
	DAEMON_ONLY_SANITIZE   = 1 << 3, /* internal use */
	//DAEMON_DOUBLE_FORK     = 1 << 4, /* double fork to avoid controlling tty */
};





#define ARRAY_SIZE(x) ((unsigned)(sizeof(x) / sizeof((x)[0])))
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#define OFF_FMT "l"

char *safe_strncpy(char *dst, const char *src, size_t size) FAST_FUNC;
char *strncpy_IFNAMSIZ(char *dst, const char *src) FAST_FUNC;

extern uint8_t xfunc_error_retval;
void xfunc_die(void) NORETURN FAST_FUNC;

extern uint32_t option_mask32;
uint32_t getopt32(char **argv, const char *applet_opts, ...) FAST_FUNC;
# define No_argument "\0"
# define Required_argument "\001"
# define Optional_argument "\002"
#if ENABLE_LONG_OPTS
uint32_t getopt32long(char **argv, const char *optstring, const char *longopts, ...) FAST_FUNC;
#else
#define getopt32long(argv,optstring,longopts,...) \
	getopt32(argv,optstring,##__VA_ARGS__)
#endif



void xsetgid(gid_t gid) FAST_FUNC;
void xsetuid(uid_t uid) FAST_FUNC;
void xsetegid(gid_t egid) FAST_FUNC;
void xseteuid(uid_t euid) FAST_FUNC;


/* xvfork() can't be a _function_, return after vfork in child mangles stack
 * in the parent. It must be a macro. */
#define xvfork() \
({ \
	pid_t bb__xvfork_pid = vfork(); \
	if (bb__xvfork_pid < 0) \
		bb_perror_msg_and_die("vfork"); \
	bb__xvfork_pid; \
})


extern bool re_execed;





/* dmalloc will redefine these to it's own implementation. It is safe
 * to have the prototypes here unconditionally.  */
void *malloc_or_warn(size_t size) FAST_FUNC RETURNS_MALLOC;
void *xmalloc(size_t size) FAST_FUNC RETURNS_MALLOC;
void *xzalloc(size_t size) FAST_FUNC RETURNS_MALLOC;
void *xrealloc(void *old, size_t size) FAST_FUNC;
/* After v = xrealloc_vector(v, SHIFT, idx) it's ok to use
 * at least v[idx] and v[idx+1], for all idx values.
 * SHIFT specifies how many new elements are added (1:2, 2:4, ..., 8:256...)
 * when all elements are used up. New elements are zeroed out.
 * xrealloc_vector(v, SHIFT, idx) *MUST* be called with consecutive IDXs -
 * skipping an index is a bad bug - it may miss a realloc!
 */
#define xrealloc_vector(vector, shift, idx) \
	xrealloc_vector_helper((vector), (sizeof((vector)[0]) << 8) + (shift), (idx))
void* xrealloc_vector_helper(void *vector, unsigned sizeof_and_shift, int idx) FAST_FUNC;
char *xstrdup(const char *s) FAST_FUNC RETURNS_MALLOC;
char *xstrndup(const char *s, int n) FAST_FUNC RETURNS_MALLOC;
void *xmemdup(const void *s, int n) FAST_FUNC RETURNS_MALLOC;

int xopen(const char *pathname, int flags) FAST_FUNC;
void xchdir(const char *path) FAST_FUNC;
void xdup2(int, int) FAST_FUNC;


char *xasprintf(const char *format, ...) __attribute__ ((format(printf, 1, 2))) FAST_FUNC RETURNS_MALLOC;




/* Having next pointer as a first member allows easy creation
 * of "llist-compatible" structs, and using llist_FOO functions
 * on them.
 */
typedef struct llist_t {
	struct llist_t *link;
	char *data;
} llist_t;
void llist_add_to(llist_t **old_head, void *data) FAST_FUNC;
void llist_add_to_end(llist_t **list_head, void *data) FAST_FUNC;
void *llist_pop(llist_t **elm) FAST_FUNC;
void llist_unlink(llist_t **head, llist_t *elm) FAST_FUNC;
void llist_free(llist_t *elm, void (*freeit)(void *data)) FAST_FUNC;
llist_t *llist_rev(llist_t *list) FAST_FUNC;
llist_t *llist_find_str(llist_t *first, const char *str) FAST_FUNC;
/* BTW, surprisingly, changing API to
 *   llist_t *llist_add_to(llist_t *old_head, void *data)
 * etc does not result in smaller code... */





#define IF_NOT_FEATURE_IPV6(...) __VA_ARGS__

int xsocket(int domain, int type, int protocol) FAST_FUNC;
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen) FAST_FUNC;
void xlisten(int s, int backlog) FAST_FUNC;
void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen) FAST_FUNC;
ssize_t xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen) FAST_FUNC;

int setsockopt_int(int fd, int level, int optname, int optval) FAST_FUNC;
int setsockopt_1(int fd, int level, int optname) FAST_FUNC;
int setsockopt_SOL_SOCKET_int(int fd, int optname, int optval) FAST_FUNC;
int setsockopt_SOL_SOCKET_1(int fd, int optname) FAST_FUNC;
/* SO_REUSEADDR allows a server to rebind to an address that is already
 * "in use" by old connections to e.g. previous server instance which is
 * killed or crashed. Without it bind will fail until all such connections
 * time out. Linux does not allow multiple live binds on same ip:port
 * regardless of SO_REUSEADDR (unlike some other flavors of Unix).
 * Turn it on before you call bind(). */
void setsockopt_reuseaddr(int fd) FAST_FUNC; /* On Linux this never fails. */
int setsockopt_keepalive(int fd) FAST_FUNC;
int setsockopt_broadcast(int fd) FAST_FUNC;
int setsockopt_bindtodevice(int fd, const char *iface) FAST_FUNC;
int bb_getsockname(int sockfd, void *addr, socklen_t addrlen) FAST_FUNC;
/* NB: returns port in host byte order */
unsigned bb_lookup_port(const char *port, const char *protocol, unsigned default_port) FAST_FUNC;
#if ENABLE_FEATURE_ETC_SERVICES
# define bb_lookup_std_port(portstr, protocol, portnum) bb_lookup_port(portstr, protocol, portnum)
#else
# define bb_lookup_std_port(portstr, protocol, portnum) (portnum)
#endif
typedef struct len_and_sockaddr {
	socklen_t len;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
		struct sockaddr_in6 sin6;
#endif
	} u;
} len_and_sockaddr;
enum {
	LSA_LEN_SIZE = offsetof(len_and_sockaddr, u),
	LSA_SIZEOF_SA = sizeof(
		union {
			struct sockaddr sa;
			struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
			struct sockaddr_in6 sin6;
#endif
		}
	)
};
/* Create stream socket, and allocate suitable lsa.
 * (lsa of correct size and lsa->sa.sa_family (AF_INET/AF_INET6))
 * af == AF_UNSPEC will result in trying to create IPv6 socket,
 * and if kernel doesn't support it, fall back to IPv4.
 * This is useful if you plan to bind to resulting local lsa.
 */
int xsocket_type(len_and_sockaddr **lsap, int af, int sock_type) FAST_FUNC;
int xsocket_stream(len_and_sockaddr **lsap) FAST_FUNC;
/* Create server socket bound to bindaddr:port. bindaddr can be NULL,
 * numeric IP ("N.N.N.N") or numeric IPv6 address,
 * and can have ":PORT" suffix (for IPv6 use "[X:X:...:X]:PORT").
 * Only if there is no suffix, port argument is used */
/* NB: these set SO_REUSEADDR before bind */
int create_and_bind_stream_or_die(const char *bindaddr, int port) FAST_FUNC;
int create_and_bind_dgram_or_die(const char *bindaddr, int port) FAST_FUNC;
int create_and_bind_to_netlink(int proto, int grp, unsigned rcvbuf) FAST_FUNC;
/* Create client TCP socket connected to peer:port. Peer cannot be NULL.
 * Peer can be numeric IP ("N.N.N.N"), numeric IPv6 address or hostname,
 * and can have ":PORT" suffix (for IPv6 use "[X:X:...:X]:PORT").
 * If there is no suffix, port argument is used */
int create_and_connect_stream_or_die(const char *peer, int port) FAST_FUNC;
/* Connect to peer identified by lsa */
int xconnect_stream(const len_and_sockaddr *lsa) FAST_FUNC;
/* Get local address of bound or accepted socket */
len_and_sockaddr *get_sock_lsa(int fd) FAST_FUNC RETURNS_MALLOC;
/* Get remote address of connected or accepted socket */
len_and_sockaddr *get_peer_lsa(int fd) FAST_FUNC RETURNS_MALLOC;
/* Return malloc'ed len_and_sockaddr with socket address of host:port
 * Currently will return IPv4 or IPv6 sockaddrs only
 * (depending on host), but in theory nothing prevents e.g.
 * UNIX socket address being returned, IPX sockaddr etc...
 * On error does bb_error_msg and returns NULL */
len_and_sockaddr* host2sockaddr(const char *host, int port) FAST_FUNC RETURNS_MALLOC;
/* Version which dies on error */
len_and_sockaddr* xhost2sockaddr(const char *host, int port) FAST_FUNC RETURNS_MALLOC;
len_and_sockaddr* xdotted2sockaddr(const char *host, int port) FAST_FUNC RETURNS_MALLOC;
/* Same, useful if you want to force family (e.g. IPv6) */
#if !ENABLE_FEATURE_IPV6
#define host_and_af2sockaddr(host, port, af) host2sockaddr((host), (port))
#define xhost_and_af2sockaddr(host, port, af) xhost2sockaddr((host), (port))
#else
len_and_sockaddr* host_and_af2sockaddr(const char *host, int port, sa_family_t af) FAST_FUNC RETURNS_MALLOC;
len_and_sockaddr* xhost_and_af2sockaddr(const char *host, int port, sa_family_t af) FAST_FUNC RETURNS_MALLOC;
#endif
/* Assign sin[6]_port member if the socket is an AF_INET[6] one,
 * otherwise no-op. Useful for ftp.
 * NB: does NOT do htons() internally, just direct assignment. */
void set_nport(struct sockaddr *sa, unsigned port) FAST_FUNC;
/* Retrieve sin[6]_port or return -1 for non-INET[6] lsa's */
int get_nport(const struct sockaddr *sa) FAST_FUNC;
/* Reverse DNS. Returns NULL on failure. */
char* xmalloc_sockaddr2host(const struct sockaddr *sa) FAST_FUNC RETURNS_MALLOC;
/* This one doesn't append :PORTNUM */
char* xmalloc_sockaddr2host_noport(const struct sockaddr *sa) FAST_FUNC RETURNS_MALLOC;
/* This one also doesn't fall back to dotted IP (returns NULL) */
char* xmalloc_sockaddr2hostonly_noport(const struct sockaddr *sa) FAST_FUNC RETURNS_MALLOC;
/* inet_[ap]ton on steroids */
char* xmalloc_sockaddr2dotted(const struct sockaddr *sa) FAST_FUNC RETURNS_MALLOC;
char* xmalloc_sockaddr2dotted_noport(const struct sockaddr *sa) FAST_FUNC RETURNS_MALLOC;
// "old" (ipv4 only) API
// users: traceroute.c hostname.c - use _list_ of all IPs
struct hostent *xgethostbyname(const char *name) FAST_FUNC;
// Also mount.c and inetd.c are using gethostbyname(),
// + inet_common.c has additional IPv4-only stuff


struct tls_aes {
	uint32_t key[60];
	unsigned rounds;
};
#define TLS_MAX_MAC_SIZE 32
#define TLS_MAX_KEY_SIZE 32
#define TLS_MAX_IV_SIZE   4
struct tls_handshake_data; /* opaque */
typedef struct tls_state {
	unsigned flags;

	int ofd;
	int ifd;

	unsigned min_encrypted_len_on_read;
	uint16_t cipher_id;
	unsigned MAC_size;
	unsigned key_size;
	unsigned IV_size;

	uint8_t *outbuf;
	int     outbuf_size;

	int     inbuf_size;
	int     ofs_to_buffered;
	int     buffered_size;
	uint8_t *inbuf;

	struct tls_handshake_data *hsd;

	// RFC 5246
	// sequence number
	//   Each connection state contains a sequence number, which is
	//   maintained separately for read and write states.  The sequence
	//   number MUST be set to zero whenever a connection state is made the
	//   active state.  Sequence numbers are of type uint64 and may not
	//   exceed 2^64-1.
	/*uint64_t read_seq64_be;*/
	uint64_t write_seq64_be;

	/*uint8_t *server_write_MAC_key;*/
	uint8_t *client_write_key;
	uint8_t *server_write_key;
	uint8_t *client_write_IV;
	uint8_t *server_write_IV;
	uint8_t client_write_MAC_key[TLS_MAX_MAC_SIZE];
	uint8_t server_write_MAC_k__[TLS_MAX_MAC_SIZE];
	uint8_t client_write_k__[TLS_MAX_KEY_SIZE];
	uint8_t server_write_k__[TLS_MAX_KEY_SIZE];
	uint8_t client_write_I_[TLS_MAX_IV_SIZE];
	uint8_t server_write_I_[TLS_MAX_IV_SIZE];

	struct tls_aes aes_encrypt;
	struct tls_aes aes_decrypt;
	uint8_t H[16]; //used by AES_GCM
} tls_state_t;

static inline tls_state_t *new_tls_state(void)
{
	tls_state_t *tls = xzalloc(sizeof(*tls));
	return tls;
}
void tls_handshake(tls_state_t *tls, const char *sni) FAST_FUNC;
#define TLSLOOP_EXIT_ON_LOCAL_EOF (1 << 0)
void tls_run_copy_loop(tls_state_t *tls, unsigned flags) FAST_FUNC;


void socket_want_pktinfo(int fd) FAST_FUNC;
ssize_t send_to_from(int fd, void *buf, size_t len, int flags,
		const struct sockaddr *to,
		const struct sockaddr *from,
		socklen_t tolen) FAST_FUNC;
ssize_t recv_from_to(int fd, void *buf, size_t len, int flags,
		struct sockaddr *from,
		struct sockaddr *to,
		socklen_t sa_size) FAST_FUNC;

uint16_t inet_cksum(uint16_t *addr, int len) FAST_FUNC;
int parse_pasv_epsv(char *buf) FAST_FUNC;





#endif