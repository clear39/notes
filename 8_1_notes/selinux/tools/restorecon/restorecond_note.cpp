//	@external/selinux/restorecond.c
int main(int argc, char **argv)
{
	int opt;
	struct sigaction sa;

	/* If we are not running SELinux then just exit */
	if (is_selinux_enabled() != 1)
		return 0;

	/* Set all options to zero/NULL except for ignore_noent & digest. */
	memset(&r_opts, 0, sizeof(r_opts));
	r_opts.ignore_noent = SELINUX_RESTORECON_IGNORE_NOENTRY;
	r_opts.ignore_digest = SELINUX_RESTORECON_IGNORE_DIGEST;

	/* As r_opts.selabel_opt_digest = NULL, no digest will be requested. */
	restore_init(&r_opts);

	/* Register sighandlers */
	sa.sa_flags = 0;
	sa.sa_handler = term_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);

	atexit( done );
	while ((opt = getopt(argc, argv, "hdf:uv")) > 0) {
		switch (opt) {
		case 'd':
			debug_mode = 1;
			break;
		case 'f':
			watch_file = optarg;
			break;
		case 'u':
			run_as_user = 1;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case 'v':
			r_opts.verbose = SELINUX_RESTORECON_VERBOSE;
			break;
		case '?':
			usage(argv[0]);
			exit(-1);
		}
	}

	master_fd = inotify_init();
	if (master_fd < 0)
		exitApp("inotify_init");

	uid_t uid = getuid();
	struct passwd *pwd = getpwuid(uid);
	if (!pwd)
		exitApp("getpwuid");

	homedir = pwd->pw_dir;
	if (uid != 0) {
		if (run_as_user)
			return server(master_fd, user_watch_file);
		if (start() != 0)
			return server(master_fd, user_watch_file);
		return 0;
	}

	watch_file = server_watch_file;
	read_config(master_fd, watch_file);

	if (!debug_mode)
		daemon(0, 0);

	write_pid_file();

	while (watch(master_fd, watch_file) == 0) {
	};

	watch_list_free(master_fd);
	close(master_fd);

	if (pidfile)
		unlink(pidfile);

	return 0;
}