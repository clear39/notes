 //      DumpstateTelephonyOnly()
{

	// This method collects dumpsys for telephony debugging only
	static void DumpstateTelephonyOnly() {
	    //构造函数保存启始时间，析构中 打印执行了时间间隔
	    DurationReporter duration_reporter("DUMPSTATE");//用于计算执行一下命令所花费时间

	    //通过iptables 输出 网络防火墙管理数据
	    DumpIpTablesAsRoot();//	frameworks/native/cmds/dumpstate/dumpstate.cpp:846

	    if (!DropRootUser()) {//设置syslog权限
		return;
	    }

	    do_dmesg();//读取内核日志
	    DoLogcat();
	    DumpPacketStats();
	    DoKmsg();
	    DumpIpAddrAndRules();
	    dump_route_tables();

	    RunDumpsys("NETWORK DIAGNOSTICS", {"connectivity", "--diag"},CommandOptions::WithTimeout(10).Build());

	    RunCommand("SYSTEM PROPERTIES", {"getprop"});

	    printf("========================================================\n");
	    printf("== Android Framework Services\n");
	    printf("========================================================\n");

	    RunDumpsys("DUMPSYS", {"connectivity"}, CommandOptions::WithTimeout(90).Build(), 10);
	    RunDumpsys("DUMPSYS", {"carrier_config"}, CommandOptions::WithTimeout(90).Build(), 10);

	    printf("========================================================\n");
	    printf("== Running Application Services\n");
	    printf("========================================================\n");

	    //相当于执行dumpsys activity service TelephonyDebugService
	    RunDumpsys("TELEPHONY SERVICES", {"activity", "service", "TelephonyDebugService"});

	    printf("========================================================\n");
	    printf("== dumpstate: done (id %d)\n", ds.id_);
	    printf("========================================================\n");
	}

	static void DumpIpTablesAsRoot() {
	    RunCommand("IPTABLES", {"iptables", "-L", "-nvx"}); //	frameworks/native/cmds/dumpstate/utils.cpp:74
	    RunCommand("IP6TABLES", {"ip6tables", "-L", "-nvx"});
	    RunCommand("IPTABLES NAT", {"iptables", "-t", "nat", "-L", "-nvx"});
	    /* no ip6 nat */
	    RunCommand("IPTABLES MANGLE", {"iptables", "-t", "mangle", "-L", "-nvx"});
	    RunCommand("IP6TABLES MANGLE", {"ip6tables", "-t", "mangle", "-L", "-nvx"});
	    RunCommand("IPTABLES RAW", {"iptables", "-t", "raw", "-L", "-nvx"});
	    RunCommand("IP6TABLES RAW", {"ip6tables", "-t", "raw", "-L", "-nvx"});
	}

	//	frameworks/native/cmds/dumpstate/utils.cpp:74
	static Dumpstate& ds = Dumpstate::GetInstance();
	static int RunCommand(const std::string& title, const std::vector<std::string>& full_command,const CommandOptions& options = CommandOptions::DEFAULT) {
	    return ds.RunCommand(title, full_command, options);
	}

	int Dumpstate::RunCommand(const std::string& title, const std::vector<std::string>& full_command,const CommandOptions& options) {
	    DurationReporter duration_reporter(title);//打印执行时间戳

	    int status = RunCommandToFd(STDOUT_FILENO, title, full_command, options);//frameworks/native/cmds/dumpstate/DumpstateUtil.cpp:194:

	    /* TODO: for now we're simplifying the progress calculation by using the
	     * timeout as the weight. It's a good approximation for most cases, except when calling dumpsys,
	     * where its weight should be much higher proportionally to its timeout.
	     * Ideally, it should use a options.EstimatedDuration() instead...*/
	    UpdateProgress(options.Timeout());

	    return status;
	}

	int RunCommandToFd(int fd, const std::string& title, const std::vector<std::string>& full_command,const CommandOptions& options) {
	    if (full_command.empty()) {
		MYLOGE("No arguments on RunCommandToFd(%s)\n", title.c_str());
		return -1;
	    }

	    int size = full_command.size() + 1;  // null terminated
	    int starting_index = 0;
	    if (options.PrivilegeMode() == SU_ROOT) {
		starting_index = 2;  // "su" "root"
		size += starting_index;
	    }

	    std::vector<const char*> args;
	    args.resize(size);

	    std::string command_string;
	    if (options.PrivilegeMode() == SU_ROOT) {
		// frameworks/native/cmds/dumpstate/DumpstateUtil.cpp:43:static constexpr const char* kSuPath = "/system/xbin/su";
		args[0] = kSuPath;
		command_string += kSuPath;
		args[1] = "root";
		command_string += " root ";// /system/xbin/su root
	    }
	    for (size_t i = 0; i < full_command.size(); i++) {
		args[i + starting_index] = full_command[i].data();
		command_string += args[i + starting_index];
		if (i != full_command.size() - 1) {
		    command_string += " ";
		}
	    }
	    args[size - 1] = nullptr;

	    const char* command = command_string.c_str();

	    //PropertiesHelper::IsUserBuild() => 判断为user版本
	    // 如果软件为user版本，不能 以root权限运行命令
	    if (options.PrivilegeMode() == SU_ROOT && PropertiesHelper::IsUserBuild()) {
		dprintf(fd, "Skipping '%s' on user build.\n", command);
		return 0;
	    }

	    if (!title.empty()) {
		//打印执行标题，以及执行的命令
		dprintf(fd, "------ %s (%s) ------\n", title.c_str(), command);
		fsync(fd);
	    }

	    const std::string& logging_message = options.LoggingMessage();
	    if (!logging_message.empty()) {
		MYLOGI(logging_message.c_str(), command_string.c_str());
	    }

	    bool silent = (options.OutputMode() == REDIRECT_TO_STDERR);
	    bool redirecting_to_fd = STDOUT_FILENO != fd;

	    if (PropertiesHelper::IsDryRun() && !options.Always()) {
		if (!title.empty()) {
		    dprintf(fd, "\t(skipped on dry run)\n");
		} else if (redirecting_to_fd) {
		    // There is no title, but we should still print a dry-run message
		    dprintf(fd, "%s: skipped on dry run\n", command_string.c_str());
		}
		fsync(fd);
		return 0;
	    }

	    const char* path = args[0];

	    uint64_t start = Nanotime();
	    pid_t pid = fork();

	    /* handle error case */
	    if (pid < 0) {
		if (!silent) dprintf(fd, "*** fork: %s\n", strerror(errno));
		MYLOGE("*** fork: %s\n", strerror(errno));
		return pid;
	    }

	    /* handle child case */
	    if (pid == 0) {
		if (options.PrivilegeMode() == DROP_ROOT && !DropRootUser()) {
		    if (!silent) {
		        dprintf(fd, "*** failed to drop root before running %s: %s\n", command,strerror(errno));
		    }
		    MYLOGE("*** could not drop root before running %s: %s\n", command, strerror(errno));
		    return -1;
		}

		if (silent) {
		    // Redirects stdout to stderr
		    TEMP_FAILURE_RETRY(dup2(STDERR_FILENO, STDOUT_FILENO));
		} else if (redirecting_to_fd) {
		    // Redirect stdout to fd
		    TEMP_FAILURE_RETRY(dup2(fd, STDOUT_FILENO));
		    close(fd);
		}

		/* make sure the child dies when dumpstate dies */
		prctl(PR_SET_PDEATHSIG, SIGKILL);

		/* just ignore SIGPIPE, will go down with parent's */
		struct sigaction sigact;
		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &sigact, NULL);

		execvp(path, (char**)args.data());
		// execvp's result will be handled after waitpid_with_timeout() below, but
		// if it failed, it's safer to exit dumpstate.
		MYLOGD("execvp on command '%s' failed (error: %s)\n", command, strerror(errno));
		// Must call _exit (instead of exit), otherwise it will corrupt the zip
		// file.
		_exit(EXIT_FAILURE);
	    }

	    /* handle parent case */
	    int status;
	    bool ret = waitpid_with_timeout(pid, options.Timeout(), &status);
	    fsync(fd);

	    uint64_t elapsed = Nanotime() - start;
	    if (!ret) {
		if (errno == ETIMEDOUT) {
		    if (!silent)
		        dprintf(fd, "*** command '%s' timed out after %.3fs (killing pid %d)\n", command,static_cast<float>(elapsed) / NANOS_PER_SEC, pid);
		    MYLOGE("*** command '%s' timed out after %.3fs (killing pid %d)\n", command,static_cast<float>(elapsed) / NANOS_PER_SEC, pid);
		} else {
		    if (!silent)
		        dprintf(fd, "*** command '%s': Error after %.4fs (killing pid %d)\n", command,static_cast<float>(elapsed) / NANOS_PER_SEC, pid);
		    MYLOGE("command '%s': Error after %.4fs (killing pid %d)\n", command,static_cast<float>(elapsed) / NANOS_PER_SEC, pid);
		}
		kill(pid, SIGTERM);
		if (!waitpid_with_timeout(pid, 5, nullptr)) {
		    kill(pid, SIGKILL);
		    if (!waitpid_with_timeout(pid, 5, nullptr)) {
		        if (!silent)
		            dprintf(fd, "could not kill command '%s' (pid %d) even with SIGKILL.\n",command, pid);
		        MYLOGE("could not kill command '%s' (pid %d) even with SIGKILL.\n", command, pid);
		    }
		}
		return -1;
	    }

	    if (WIFSIGNALED(status)) {
		if (!silent)
		    dprintf(fd, "*** command '%s' failed: killed by signal %d\n", command, WTERMSIG(status));
		MYLOGE("*** command '%s' failed: killed by signal %d\n", command, WTERMSIG(status));
	    } else if (WIFEXITED(status) && WEXITSTATUS(status) > 0) {
		status = WEXITSTATUS(status);
		if (!silent) dprintf(fd, "*** command '%s' failed: exit code %d\n", command, status);
		MYLOGE("*** command '%s' failed: exit code %d\n", command, status);
	    }

	    return status;
	}



	// Switches to non-root user and group.
	bool DropRootUser() {
	    if (getgid() == AID_SHELL && getuid() == AID_SHELL) {
		MYLOGD("drop_root_user(): already running as Shell\n");
		return true;
	    }
	    /* ensure we will keep capabilities when we drop root */
	    if (prctl(PR_SET_KEEPCAPS, 1) < 0) {
		MYLOGE("prctl(PR_SET_KEEPCAPS) failed: %s\n", strerror(errno));
		return false;
	    }

	    gid_t groups[] = {AID_LOG,AID_SDCARD_R,AID_SDCARD_RW, AID_MOUNT,AID_INET, AID_NET_BW_STATS, AID_READPROC,  AID_BLUETOOTH};
	    if (setgroups(sizeof(groups) / sizeof(groups[0]), groups) != 0) {
		MYLOGE("Unable to setgroups, aborting: %s\n", strerror(errno));
		return false;
	    }
	    if (setgid(AID_SHELL) != 0) {
		MYLOGE("Unable to setgid, aborting: %s\n", strerror(errno));
		return false;
	    }
	    if (setuid(AID_SHELL) != 0) {
		MYLOGE("Unable to setuid, aborting: %s\n", strerror(errno));
		return false;
	    }

	    struct __user_cap_header_struct capheader;
	    struct __user_cap_data_struct capdata[2];
	    memset(&capheader, 0, sizeof(capheader));
	    memset(&capdata, 0, sizeof(capdata));
	    capheader.version = _LINUX_CAPABILITY_VERSION_3;
	    capheader.pid = 0;

	    capdata[CAP_TO_INDEX(CAP_SYSLOG)].permitted = CAP_TO_MASK(CAP_SYSLOG);
	    capdata[CAP_TO_INDEX(CAP_SYSLOG)].effective = CAP_TO_MASK(CAP_SYSLOG);
	    capdata[0].inheritable = 0;
	    capdata[1].inheritable = 0;

	    if (capset(&capheader, &capdata[0]) < 0) {
		MYLOGE("capset failed: %s\n", strerror(errno));
		return false;
	    }

	    return true;
	}

	void do_dmesg() {
	    const char *title = "KERNEL LOG (dmesg)"; //toybox 的自命令
	    DurationReporter duration_reporter(title);
	    printf("------ %s ------\n", title);

	    if (PropertiesHelper::IsDryRun()) return;

	    /* Get size of kernel buffer */
	    int size = klogctl(KLOG_SIZE_BUFFER, NULL, 0);
	    if (size <= 0) {
		printf("Unexpected klogctl return value: %d\n\n", size);
		return;
	    }
	    char *buf = (char *) malloc(size + 1);
	    if (buf == NULL) {
		printf("memory allocation failed\n\n");
		return;
	    }
	    int retval = klogctl(KLOG_READ_ALL, buf, size);
	    if (retval < 0) {
		printf("klogctl failure\n\n");
		free(buf);
		return;
	    }
	    buf[retval] = '\0';
	    printf("%s\n\n", buf);
	    free(buf);
	    return;
	}

	static void DoLogcat() {
	    unsigned long timeout;
	    // DumpFile("EVENT LOG TAGS", "/etc/event-log-tags");
	    // calculate timeout
	    timeout = logcat_timeout("main") + logcat_timeout("system") + logcat_timeout("crash");
	    if (timeout < 20000) {
		timeout = 20000;
	    }
	    RunCommand("SYSTEM LOG",{"logcat", "-v", "threadtime", "-v", "printable", "-v", "uid","-d", "*:v"},CommandOptions::WithTimeout(timeout / 1000).Build());
	    timeout = logcat_timeout("events");
	    if (timeout < 20000) {
		timeout = 20000;
	    }
	    RunCommand("EVENT LOG",{"logcat", "-b", "events", "-v", "threadtime", "-v", "printable", "-v", "uid","-d", "*:v"},CommandOptions::WithTimeout(timeout / 1000).Build());
	    timeout = logcat_timeout("radio");
	    if (timeout < 20000) {
		timeout = 20000;
	    }
	    RunCommand("RADIO LOG",{"logcat", "-b", "radio", "-v", "threadtime", "-v", "printable", "-v", "uid","-d", "*:v"},CommandOptions::WithTimeout(timeout / 1000).Build());

	    RunCommand("LOG STATISTICS", {"logcat", "-b", "all", "-S"});

	    /* kernels must set CONFIG_PSTORE_PMSG, slice up pstore with device tree */
	    RunCommand("LAST LOGCAT",{"logcat", "-L", "-b", "all", "-v", "threadtime", "-v", "printable", "-v", "uid","-d", "*:v"});
	}



}
 //      ds.DumpstateBoard();
