//	@external/strace/strace.c
int main(int argc, char *argv[])
{
	init(argc, argv);

	exit_code = !nprocs;

	int status;
	siginfo_t si;
	while (dispatch_event(next_event(&status, &si), &status, &si));
	terminate();
}



static void ATTRIBUTE_NOINLINE
init(int argc, char *argv[])
{
	int c, i;
	int optF = 0;

	if (!program_invocation_name || !*program_invocation_name) {
		static char name[] = "strace";
		program_invocation_name = (argv[0] && *argv[0]) ? argv[0] : name;
	}

	strace_tracer_pid = getpid();

	os_release = get_os_release();

	shared_log = stderr;
	set_sortby(DEFAULT_SORTBY);
	set_personality(DEFAULT_PERSONALITY);
	qualify("trace=all");
	qualify("abbrev=all");
	qualify("verbose=all");
#if DEFAULT_QUAL_FLAGS != (QUAL_TRACE | QUAL_ABBREV | QUAL_VERBOSE)
# error Bug in DEFAULT_QUAL_FLAGS
#endif
	qualify("signal=all");
	while ((c = getopt(argc, argv,
		"+b:cCdfFhiqrtTvVwxyz"
#ifdef USE_LIBUNWIND
		"k"
#endif
		"D"
		"a:e:o:O:p:s:S:u:E:P:I:")) != EOF) {
		switch (c) {
		case 'b':
			if (strcmp(optarg, "execve") != 0)
				error_msg_and_die("Syscall '%s' for -b isn't supported",optarg);
			detach_on_execve = 1;
			break;
		case 'c':
			if (cflag == CFLAG_BOTH) {
				error_msg_and_help("-c and -C are mutually exclusive");
			}
			cflag = CFLAG_ONLY_STATS;
			break;
		case 'C':
			if (cflag == CFLAG_ONLY_STATS) {
				error_msg_and_help("-c and -C are mutually exclusive");
			}
			cflag = CFLAG_BOTH;
			break;
		case 'd':
			debug_flag = 1;
			break;
		case 'D':
			daemonized_tracer = 1;
			break;
		case 'F':
			optF = 1;
			break;
		case 'f':
			followfork++;
			break;
		case 'h':
			usage();
			break;
		case 'i':
			iflag = 1;
			break;
		case 'q':
			qflag++;
			break;
		case 'r':
			rflag = 1;
			break;
		case 't':
			tflag++;
			break;
		case 'T':
			Tflag = 1;
			break;
		case 'w':
			count_wallclock = 1;
			break;
		case 'x':
			xflag++;
			break;
		case 'y':
			show_fd_path++;
			break;
		case 'v':
			qualify("abbrev=none");
			break;
		case 'V':
			print_version();
			exit(0);
			break;
		case 'z':
			not_failing_only = 1;
			break;
		case 'a':
			acolumn = string_to_uint(optarg);
			if (acolumn < 0)
				error_opt_arg(c, optarg);
			break;
		case 'e':
			qualify(optarg);
			break;
		case 'o':
			outfname = xstrdup(optarg);
			break;
		case 'O':
			i = string_to_uint(optarg);
			if (i < 0)
				error_opt_arg(c, optarg);
			set_overhead(i);
			break;
		case 'p':
			process_opt_p_list(optarg);
			break;
		case 'P':
			pathtrace_select(optarg);
			break;
		case 's':
			i = string_to_uint(optarg);
			if (i < 0 || (unsigned int) i > -1U / 4)
				error_opt_arg(c, optarg);
			max_strlen = i;
			break;
		case 'S':
			set_sortby(optarg);
			break;
		case 'u':
			username = xstrdup(optarg);
			break;
#ifdef USE_LIBUNWIND
		case 'k':
			stack_trace_enabled = true;
			break;
#endif
		case 'E':
			if (putenv(optarg) < 0)
				perror_msg_and_die("putenv");
			break;
		case 'I':
			opt_intr = string_to_uint_upto(optarg, NUM_INTR_OPTS - 1);
			if (opt_intr <= 0)
				error_opt_arg(c, optarg);
			break;
		default:
			error_msg_and_help(NULL);
			break;
		}
	}

	argv += optind;
	argc -= optind;

	if (argc < 0 || (!argv[0] && !nprocs)) {
		error_msg_and_help("must have PROG [ARGS] or -p PID");
	}

	if (!argv[0] && daemonized_tracer) {
		error_msg_and_help("PROG [ARGS] must be specified with -D");
	}

	if (!followfork)
		followfork = optF;

	if (followfork >= 2 && cflag) {
		error_msg_and_help("(-c or -C) and -ff are mutually exclusive");
	}

	if (count_wallclock && !cflag) {
		error_msg_and_help("-w must be given with (-c or -C)");
	}

	if (cflag == CFLAG_ONLY_STATS) {
		if (iflag)
			error_msg("-%c has no effect with -c", 'i');
#ifdef USE_LIBUNWIND
		if (stack_trace_enabled)
			error_msg("-%c has no effect with -c", 'k');
#endif
		if (rflag)
			error_msg("-%c has no effect with -c", 'r');
		if (tflag)
			error_msg("-%c has no effect with -c", 't');
		if (Tflag)
			error_msg("-%c has no effect with -c", 'T');
		if (show_fd_path)
			error_msg("-%c has no effect with -c", 'y');
	}

	if (rflag) {
		if (tflag > 1)
			error_msg("-tt has no effect with -r");
		tflag = 1;
	}

	acolumn_spaces = xmalloc(acolumn + 1);
	memset(acolumn_spaces, ' ', acolumn);
	acolumn_spaces[acolumn] = '\0';

	sigprocmask(SIG_SETMASK, NULL, &start_set);
	memcpy(&blocked_set, &start_set, sizeof(blocked_set));

	set_sigaction(SIGCHLD, SIG_DFL, &params_for_tracee.child_sa);

#ifdef USE_LIBUNWIND
	if (stack_trace_enabled) {
		unsigned int tcbi;

		unwind_init();
		for (tcbi = 0; tcbi < tcbtabsize; ++tcbi) {
			unwind_tcb_init(tcbtab[tcbi]);
		}
	}
#endif

	/* See if they want to run as another user. */
	if (username != NULL) {
		struct passwd *pent;

		if (getuid() != 0 || geteuid() != 0) {
			error_msg_and_die("You must be root to use the -u option");
		}
		pent = getpwnam(username);
		if (pent == NULL) {
			error_msg_and_die("Cannot find user '%s'", username);
		}
		run_uid = pent->pw_uid;
		run_gid = pent->pw_gid;
	} else {
		run_uid = getuid();
		run_gid = getgid();
	}

	if (followfork)
		ptrace_setoptions |= PTRACE_O_TRACECLONE |
				     PTRACE_O_TRACEFORK |
				     PTRACE_O_TRACEVFORK;
	if (debug_flag)
		error_msg("ptrace_setoptions = %#x", ptrace_setoptions);
	test_ptrace_seize();

	/*
	 * Is something weird with our stdin and/or stdout -
	 * for example, may they be not open? In this case,
	 * ensure that none of the future opens uses them.
	 *
	 * This was seen in the wild when /proc/sys/kernel/core_pattern
	 * was set to "|/bin/strace -o/tmp/LOG PROG":
	 * kernel runs coredump helper with fd#0 open but fd#1 closed (!),
	 * therefore LOG gets opened to fd#1, and fd#1 is closed by
	 * "don't hold up stdin/out open" code soon after.
	 */
	ensure_standard_fds_opened();

	/* Check if they want to redirect the output. */
	if (outfname) {
		/* See if they want to pipe the output. */
		if (outfname[0] == '|' || outfname[0] == '!') {
			/*
			 * We can't do the <outfname>.PID funny business
			 * when using popen, so prohibit it.
			 */
			if (followfork >= 2)
				error_msg_and_help("piping the output and -ff are mutually exclusive");
			shared_log = strace_popen(outfname + 1);
		} else if (followfork < 2)
			shared_log = strace_fopen(outfname);
	} else {
		/* -ff without -o FILE is the same as single -f */
		if (followfork >= 2)
			followfork = 1;
	}

	if (!outfname || outfname[0] == '|' || outfname[0] == '!') {
		setvbuf(shared_log, NULL, _IOLBF, 0);
	}

	/*
	 * argv[0]	-pPID	-oFILE	Default interactive setting
	 * yes		*	0	INTR_WHILE_WAIT
	 * no		1	0	INTR_WHILE_WAIT
	 * yes		*	1	INTR_NEVER
	 * no		1	1	INTR_WHILE_WAIT
	 */

	if (outfname && argv[0]) {
		if (!opt_intr)
			opt_intr = INTR_NEVER;
		if (!qflag)
			qflag = 1;
	}
	if (!opt_intr)
		opt_intr = INTR_WHILE_WAIT;

	/*
	 * startup_child() must be called before the signal handlers get
	 * installed below as they are inherited into the spawned process.
	 * Also we do not need to be protected by them as during interruption
	 * in the startup_child() mode we kill the spawned process anyway.
	 */
	if (argv[0]) {
		startup_child(argv);
	}

	set_sigaction(SIGTTOU, SIG_IGN, NULL);
	set_sigaction(SIGTTIN, SIG_IGN, NULL);
	if (opt_intr != INTR_ANYWHERE) {
		if (opt_intr == INTR_BLOCK_TSTP_TOO)
			set_sigaction(SIGTSTP, SIG_IGN, NULL);
		/*
		 * In interactive mode (if no -o OUTFILE, or -p PID is used),
		 * fatal signals are blocked while syscall stop is processed,
		 * and acted on in between, when waiting for new syscall stops.
		 * In non-interactive mode, signals are ignored.
		 */
		set_sigaction(SIGHUP, interactive ? interrupt : SIG_IGN, NULL);
		set_sigaction(SIGINT, interactive ? interrupt : SIG_IGN, NULL);
		set_sigaction(SIGQUIT, interactive ? interrupt : SIG_IGN, NULL);
		set_sigaction(SIGPIPE, interactive ? interrupt : SIG_IGN, NULL);
		set_sigaction(SIGTERM, interactive ? interrupt : SIG_IGN, NULL);
	}

	if (nprocs != 0 || daemonized_tracer)
		startup_attach();

	/* Do we want pids printed in our -o OUTFILE?
	 * -ff: no (every pid has its own file); or
	 * -f: yes (there can be more pids in the future); or
	 * -p PID1,PID2: yes (there are already more than one pid)
	 */
	print_pid_pfx = (outfname && followfork < 2 && (followfork == 1 || nprocs > 1));
}