# dumpstate -h                                 
usage: dumpstate [-h] [-b soundfile] [-e soundfile] [-o file] [-d] [-p] [-z]] [-s] [-S] [-q] [-B] [-P] [-R] [-V version]
  -h: display this help message											//展示命令帮助文档
  -b: play sound file instead of vibrate(振动), at beginning of job						//该命令开始工作时，用播放文件代替震动
  -e: play sound file instead of vibrate(振动), at end of job							//该命令工作结束时，用播放文件代替震动
  -o: write to file (instead of stdout)										//用文件代替终端输出描述符
  -d: append date to filename (requires -o)									//给输出的文件名加上（年月日时分秒）
  -p: capture screenshot to filename.png (requires -o)								//抓取屏幕，相当于截屏
  -z: generate zipped file (requires -o)									//压缩成zip文件
  -s: write output to control socket (for init)
  -S: write file location to control socket (for init; requires -o and -z)  -q: disable vibrate
  -B: send broadcast when finished (requires -o)
  -P: send broadcast when started and update system properties on progress (requires -o and -B)
  -R: take bugreport in remote mode (requires -o, -z, -d and -B, shouldn't be used with -P)
  -v: prints the dumpstate header and exit



//	frameworks/native/cmds/dumpstate/dumpstate.cpp

int main(int argc, char *argv[]) {
    int do_add_date = 0;
    int do_zip_file = 0;
    int do_vibrate = 1;
    char* use_outfile = 0;
    int use_socket = 0;
    int use_control_socket = 0;
    int do_fb = 0;
    int do_broadcast = 0;
    int is_remote_mode = 0;
    bool show_header_only = false;
    bool do_start_service = false;
    bool telephony_only = false;

    /* set as high priority, and protect from OOM killer */
    setpriority(PRIO_PROCESS, 0, -20);

    FILE* oom_adj = fopen("/proc/self/oom_score_adj", "we");
    if (oom_adj) {
        fputs("-1000", oom_adj);
        fclose(oom_adj);
    } else {
        /* fallback to kernels <= 2.6.35 */
        oom_adj = fopen("/proc/self/oom_adj", "we");
        if (oom_adj) {
            fputs("-17", oom_adj);
            fclose(oom_adj);
        }
    }

    /* parse arguments */
    int c;
    while ((c = getopt(argc, argv, "dho:svqzpPBRSV:")) != -1) {
        switch (c) {
            // clang-format off
            case 'd': do_add_date = 1;            break;
            case 'z': do_zip_file = 1;            break;
            case 'o': use_outfile = optarg;       break;
            case 's': use_socket = 1;             break;
            case 'S': use_control_socket = 1;     break;
            case 'v': show_header_only = true;    break;
            case 'q': do_vibrate = 0;             break;
            case 'p': do_fb = 1;                  break;
            case 'P': ds.update_progress_ = true; break;
            case 'R': is_remote_mode = 1;         break;
            case 'B': do_broadcast = 1;           break;
            case 'V':                             break; // compatibility no-op
            case 'h':
                ShowUsageAndExit(0);
                break;
            default:
                fprintf(stderr, "Invalid option: %c\n", c);
                ShowUsageAndExit();
                // clang-format on
        }
    }

    // TODO: use helper function to convert argv into a string
    for (int i = 0; i < argc; i++) {
        ds.args_ += argv[i];
        if (i < argc - 1) {
            ds.args_ += " ";
        }
    }

    //	frameworks/native/cmds/dumpstate/dumpstate.cpp:136:static constexpr char PROPERTY_EXTRA_OPTIONS[] = "dumpstate.options";	
    ds.extra_options_ = android::base::GetProperty(PROPERTY_EXTRA_OPTIONS, ""); //默认系统为空
    if (!ds.extra_options_.empty()) {//ds.extra_options_.empty()  == true
        // Framework uses a system property to override some command-line args.
        // Currently, it contains the type of the requested bugreport.
        if (ds.extra_options_ == "bugreportplus") {
            // Currently, the dumpstate binder is only used by Shell to update progress.
            do_start_service = true;		//标记是否启动一个 'dumpstate' 服务
            ds.update_progress_ = true;
            do_fb = 0;
        } else if (ds.extra_options_ == "bugreportremote") {
            do_vibrate = 0;
            is_remote_mode = 1;
            do_fb = 0;
        } else if (ds.extra_options_ == "bugreportwear") {
            ds.update_progress_ = true;
        } else if (ds.extra_options_ == "bugreporttelephony") { 
            telephony_only = true;
        } else {
            MYLOGE("Unknown extra option: %s\n", ds.extra_options_.c_str());
        }
        // Reset the property
        android::base::SetProperty(PROPERTY_EXTRA_OPTIONS, "");
    }




    //frameworks/native/cmds/dumpstate/dumpstate.cpp:139:static constexpr char PROPERTY_EXTRA_TITLE[] = "dumpstate.options.title";
    ds.notification_title = android::base::GetProperty(PROPERTY_EXTRA_TITLE, "");//默认系统为空
    if (!ds.notification_title.empty()) {//ds.extra_options_.empty()  == true
        // Reset the property
        android::base::SetProperty(PROPERTY_EXTRA_TITLE, "");

	//frameworks/native/cmds/dumpstate/dumpstate.cpp:140:static constexpr char PROPERTY_EXTRA_DESCRIPTION[] = "dumpstate.options.description";
        ds.notification_description = android::base::GetProperty(PROPERTY_EXTRA_DESCRIPTION, "");//默认系统为空
        if (!ds.notification_description.empty()) {//ds.extra_options_.empty()  == true
            // Reset the property
            android::base::SetProperty(PROPERTY_EXTRA_DESCRIPTION, "");
        }
        MYLOGD("notification (title:  %s, description: %s)\n",ds.notification_title.c_str(), ds.notification_description.c_str());
    }

    if ((do_zip_file || do_add_date || ds.update_progress_ || do_broadcast) && !use_outfile) {
        ExitOnInvalidArgs();
    }

    if (use_control_socket && !do_zip_file) {
        ExitOnInvalidArgs();
    }

    if (ds.update_progress_ && !do_broadcast) {
        ExitOnInvalidArgs();
    }

    if (is_remote_mode && (ds.update_progress_ || !do_broadcast || !do_zip_file || !do_add_date)) {
        ExitOnInvalidArgs();
    }

    //	frameworks/native/cmds/dumpstate/dumpstate.h:156:static std::string VERSION_DEFAULT = "default";
    //	frameworks/native/cmds/dumpstate/dumpstate.h:145:static std::string VERSION_CURRENT = "1.0";
    if (ds.version_ == VERSION_DEFAULT) {
        ds.version_ = VERSION_CURRENT;
    }

    if (ds.version_ != VERSION_CURRENT && ds.version_ != VERSION_SPLIT_ANR) {
        MYLOGE("invalid version requested ('%s'); suppported values are: ('%s', '%s', '%s')\n",ds.version_.c_str(), VERSION_DEFAULT.c_str(), VERSION_CURRENT.c_str(),VERSION_SPLIT_ANR.c_str());
        exit(1);
    }

    if (show_header_only) {
        ds.PrintHeader();//打印头信息
        exit(0);
    }

    /* redirect（重定向） output if needed */
    bool is_redirecting = !use_socket && use_outfile;

    // TODO: temporarily set progress until it's part of the Dumpstate constructor
    std::string stats_path = is_redirecting ? android::base::StringPrintf("%s/dumpstate-stats.txt", dirname(use_outfile)) : "";
    ds.progress_.reset(new Progress(stats_path));

    /* gets the sequential id */
    //	frameworks/native/cmds/dumpstate/dumpstate.cpp:137:static constexpr char PROPERTY_LAST_ID[] = "dumpstate.last_id";
    uint32_t last_id = android::base::GetIntProperty(PROPERTY_LAST_ID, 0);
    ds.id_ = ++last_id;
    android::base::SetProperty(PROPERTY_LAST_ID, std::to_string(last_id));

    MYLOGI("begin\n");

    register_sig_handler();

    if (do_start_service) {
        MYLOGI("Starting 'dumpstate' service\n");
        android::status_t ret;
        if ((ret = android::os::DumpstateService::Start()) != android::OK) {//添加一个dumpstate服务ServiceManager
            MYLOGE("Unable to start DumpstateService: %d\n", ret);
        }
    }

    if (PropertiesHelper::IsDryRun()) { //从SystemProperty获取dumpstate.dry_run值
        MYLOGI("Running on dry-run mode (to disable it, call 'setprop dumpstate.dry_run false')\n");
    }

    MYLOGI("dumpstate info: id=%d, args='%s', extra_options= %s)\n", ds.id_, ds.args_.c_str(), ds.extra_options_.c_str());

    MYLOGI("bugreport format version: %s\n", ds.version_.c_str());

    ds.do_early_screenshot_ = ds.update_progress_;

    // If we are going to use a socket, do it as early as possible to avoid timeouts from bugreport.
    if (use_socket) {
        redirect_to_socket(stdout, "dumpstate");// 将标准输出stdout重定向到dumpstate套接字描述符上
    }

    if (use_control_socket) {
        MYLOGD("Opening control socket\n");
        ds.control_socket_fd_ = open_socket("dumpstate");
        ds.update_progress_ = 1;
    }

    if (is_redirecting) {
        ds.bugreport_dir_ = dirname(use_outfile);
        std::string build_id = android::base::GetProperty("ro.build.id", "UNKNOWN_BUILD");
        std::string device_name = android::base::GetProperty("ro.product.name", "UNKNOWN_DEVICE");
        ds.base_name_ = android::base::StringPrintf("%s-%s-%s", basename(use_outfile),device_name.c_str(), build_id.c_str());
        if (do_add_date) {
            char date[80];
            strftime(date, sizeof(date), "%Y-%m-%d-%H-%M-%S", localtime(&ds.now_));
            ds.name_ = date;
        } else {
            ds.name_ = "undated";
        }

        if (telephony_only) {
            ds.base_name_ += "-telephony";
        }

        if (do_fb) {
            ds.screenshot_path_ = ds.GetPath(".png");
        }
        ds.tmp_path_ = ds.GetPath(".tmp");
        ds.log_path_ = ds.GetPath("-dumpstate_log-" + std::to_string(ds.pid_) + ".txt");

        MYLOGD(
            "Bugreport dir: %s\n"
            "Base name: %s\n"
            "Suffix: %s\n"
            "Log path: %s\n"
            "Temporary path: %s\n"
            "Screenshot path: %s\n",
            ds.bugreport_dir_.c_str(), ds.base_name_.c_str(), ds.name_.c_str(),
            ds.log_path_.c_str(), ds.tmp_path_.c_str(), ds.screenshot_path_.c_str());

        if (do_zip_file) {
            ds.path_ = ds.GetPath(".zip");
            MYLOGD("Creating initial .zip file (%s)\n", ds.path_.c_str());
            create_parent_dirs(ds.path_.c_str());
            ds.zip_file.reset(fopen(ds.path_.c_str(), "wb"));
            if (ds.zip_file == nullptr) {
                MYLOGE("fopen(%s, 'wb'): %s\n", ds.path_.c_str(), strerror(errno));
                do_zip_file = 0;
            } else {
                ds.zip_writer_.reset(new ZipWriter(ds.zip_file.get()));
            }
            ds.AddTextZipEntry("version.txt", ds.version_);
        }

        if (ds.update_progress_) {
            if (do_broadcast) {
                // clang-format off

                std::vector<std::string> am_args = {
                     "--receiver-permission", "android.permission.DUMP",
                     "--es", "android.intent.extra.NAME", ds.name_,
                     "--ei", "android.intent.extra.ID", std::to_string(ds.id_),
                     "--ei", "android.intent.extra.PID", std::to_string(ds.pid_),
                     "--ei", "android.intent.extra.MAX", std::to_string(ds.progress_->GetMax()),
                };
                // clang-format on
                SendBroadcast("com.android.internal.intent.action.BUGREPORT_STARTED", am_args);
            }
            if (use_control_socket) {
                dprintf(ds.control_socket_fd_, "BEGIN:%s\n", ds.path_.c_str());
            }
        }
    }

    /* read /proc/cmdline before dropping root */
    FILE *cmdline = fopen("/proc/cmdline", "re");
    if (cmdline) {
        fgets(cmdline_buf, sizeof(cmdline_buf), cmdline);
        fclose(cmdline);
    }

    if (do_vibrate) {
        Vibrate(150);
    }

    if (do_fb && ds.do_early_screenshot_) {
        if (ds.screenshot_path_.empty()) {
            // should not have happened
            MYLOGE("INTERNAL ERROR: skipping early screenshot because path was not set\n");
        } else {
            MYLOGI("taking early screenshot\n");
            ds.TakeScreenshot();
        }
    }

    if (do_zip_file) {//设置压缩文件权限
        if (chown(ds.path_.c_str(), AID_SHELL, AID_SHELL)) {
            MYLOGE("Unable to change ownership of zip file %s: %s\n", ds.path_.c_str(),strerror(errno));
        }
    }

    if (is_redirecting) {//设置文件重定向
        redirect_to_file(stderr, const_cast<char*>(ds.log_path_.c_str()));
        if (chown(ds.log_path_.c_str(), AID_SHELL, AID_SHELL)) {
            MYLOGE("Unable to change ownership of dumpstate log file %s: %s\n",ds.log_path_.c_str(), strerror(errno));
        }
        /* TODO: rather than generating a text file now and zipping it later,
           it would be more efficient to redirect stdout to the zip entry
           directly, but the libziparchive doesn't support that option yet. */
        redirect_to_file(stdout, const_cast<char*>(ds.tmp_path_.c_str()));
        if (chown(ds.tmp_path_.c_str(), AID_SHELL, AID_SHELL)) {
            MYLOGE("Unable to change ownership of temporary bugreport file %s: %s\n",ds.tmp_path_.c_str(), strerror(errno));
        }
    }

    // Don't buffer stdout
    setvbuf(stdout, nullptr, _IONBF, 0);

    // NOTE: there should be no stdout output until now, otherwise it would break the header.
    // In particular, DurationReport objects should be created passing 'title, NULL', so their
    // duration is logged into MYLOG instead.
    ds.PrintHeader();

    if (telephony_only) {// 需要设置setprop dumpstate.options bugreporttelephony
        DumpstateTelephonyOnly();
        ds.DumpstateBoard();
    } else {
        // Dumps systrace right away, otherwise it will be filled with unnecessary events.
        // First try to dump anrd trace if the daemon is running. Otherwise, dump
        // the raw trace.
        if (!dump_anrd_trace()) {
            dump_systrace();
        }

        // Invoking the following dumpsys calls before dump_traces() to try and
        // keep the system stats as close to its initial state as possible.
        RunDumpsys("DUMPSYS MEMINFO", {"meminfo", "-a"},CommandOptions::WithTimeout(90).DropRoot().Build());
        RunDumpsys("DUMPSYS CPUINFO", {"cpuinfo", "-a"},CommandOptions::WithTimeout(10).DropRoot().Build());

        // TODO: Drop root user and move into dumpstate() once b/28633932 is fixed.
        dump_raft();

        /* collect stack traces from Dalvik and native processes (needs root) */
        dump_traces_path = dump_traces();

        /* Run some operations that require root. */
        tombstone_data.reset(GetDumpFds(TOMBSTONE_DIR, TOMBSTONE_FILE_PREFIX, !ds.IsZipping()));
        anr_data.reset(GetDumpFds(ANR_DIR, ANR_FILE_PREFIX, !ds.IsZipping()));

        ds.AddDir(RECOVERY_DIR, true);
        ds.AddDir(RECOVERY_DATA_DIR, true);
        ds.AddDir(LOGPERSIST_DATA_DIR, false);
        if (!PropertiesHelper::IsUserBuild()) {
            ds.AddDir(PROFILE_DATA_DIR_CUR, true);
            ds.AddDir(PROFILE_DATA_DIR_REF, true);
        }
        add_mountinfo();
        DumpIpTablesAsRoot();

        // Capture any IPSec policies in play.  No keys are exposed here.
        RunCommand("IP XFRM POLICY", {"ip", "xfrm", "policy"},CommandOptions::WithTimeout(10).Build());

        // Run ss as root so we can see socket marks.
        RunCommand("DETAILED SOCKET STATE", {"ss", "-eionptu"},CommandOptions::WithTimeout(10).Build());

        if (!DropRootUser()) {
            return -1;
        }

        dumpstate();
    }

    /* close output if needed */
    if (is_redirecting) {
        fclose(stdout);
    }

    /* rename or zip the (now complete) .tmp file to its final location */
    if (use_outfile) {

        /* check if user changed the suffix using system properties */
        std::string name = android::base::GetProperty(android::base::StringPrintf("dumpstate.%d.name", ds.pid_), "");
        bool change_suffix= false;
        if (!name.empty()) {
            /* must whitelist which characters are allowed, otherwise it could cross directories */
            std::regex valid_regex("^[-_a-zA-Z0-9]+$");
            if (std::regex_match(name.c_str(), valid_regex)) {
                change_suffix = true;
            } else {
                MYLOGE("invalid suffix provided by user: %s\n", name.c_str());
            }
        }
        if (change_suffix) {
            MYLOGI("changing suffix from %s to %s\n", ds.name_.c_str(), name.c_str());
            ds.name_ = name;
            if (!ds.screenshot_path_.empty()) {
                std::string new_screenshot_path = ds.GetPath(".png");
                if (rename(ds.screenshot_path_.c_str(), new_screenshot_path.c_str())) {
                    MYLOGE("rename(%s, %s): %s\n", ds.screenshot_path_.c_str(),new_screenshot_path.c_str(), strerror(errno));
                } else {
                    ds.screenshot_path_ = new_screenshot_path;
                }
            }
        }

        bool do_text_file = true;
        if (do_zip_file) {
            if (!ds.FinishZipFile()) {
                MYLOGE("Failed to finish zip file; sending text bugreport instead\n");
                do_text_file = true;
            } else {
                do_text_file = false;
                // Since zip file is already created, it needs to be renamed.
                std::string new_path = ds.GetPath(".zip");
                if (ds.path_ != new_path) {
                    MYLOGD("Renaming zip file from %s to %s\n", ds.path_.c_str(), new_path.c_str());
                    if (rename(ds.path_.c_str(), new_path.c_str())) {
                        MYLOGE("rename(%s, %s): %s\n", ds.path_.c_str(), new_path.c_str(),strerror(errno));
                    } else {
                        ds.path_ = new_path;
                    }
                }
            }
        }
        if (do_text_file) {
            ds.path_ = ds.GetPath(".txt");
            MYLOGD("Generating .txt bugreport at %s from %s\n", ds.path_.c_str(),ds.tmp_path_.c_str());
            if (rename(ds.tmp_path_.c_str(), ds.path_.c_str())) {
                MYLOGE("rename(%s, %s): %s\n", ds.tmp_path_.c_str(), ds.path_.c_str(),strerror(errno));
                ds.path_.clear();
            }
        }
        if (use_control_socket) {
            if (do_text_file) {
                dprintf(ds.control_socket_fd_,"FAIL:could not create zip file, check %s " "for more details\n",ds.log_path_.c_str());
            } else {
                dprintf(ds.control_socket_fd_, "OK:%s\n", ds.path_.c_str());
            }
        }
    }

    /* vibrate a few but shortly times to let user know it's finished */
    for (int i = 0; i < 3; i++) {
        Vibrate(75);
        usleep((75 + 50) * 1000);
    }

    /* tell activity manager we're done */
    if (do_broadcast) {
        if (!ds.path_.empty()) {
            MYLOGI("Final bugreport path: %s\n", ds.path_.c_str());
            // clang-format off

            std::vector<std::string> am_args = {
                 "--receiver-permission", "android.permission.DUMP",
                 "--ei", "android.intent.extra.ID", std::to_string(ds.id_),
                 "--ei", "android.intent.extra.PID", std::to_string(ds.pid_),
                 "--ei", "android.intent.extra.MAX", std::to_string(ds.progress_->GetMax()),
                 "--es", "android.intent.extra.BUGREPORT", ds.path_,
                 "--es", "android.intent.extra.DUMPSTATE_LOG", ds.log_path_
            };
            // clang-format on
            if (do_fb) {
                am_args.push_back("--es");
                am_args.push_back("android.intent.extra.SCREENSHOT");
                am_args.push_back(ds.screenshot_path_);
            }
            if (!ds.notification_title.empty()) {
                am_args.push_back("--es");
                am_args.push_back("android.intent.extra.TITLE");
                am_args.push_back(ds.notification_title);
                if (!ds.notification_description.empty()) {
                    am_args.push_back("--es");
                    am_args.push_back("android.intent.extra.DESCRIPTION");
                    am_args.push_back(ds.notification_description);
                }
            }
            if (is_remote_mode) {
                am_args.push_back("--es");
                am_args.push_back("android.intent.extra.REMOTE_BUGREPORT_HASH");
                am_args.push_back(SHA256_file_hash(ds.path_));
                SendBroadcast("com.android.internal.intent.action.REMOTE_BUGREPORT_FINISHED",am_args);
            } else {
                SendBroadcast("com.android.internal.intent.action.BUGREPORT_FINISHED", am_args);
            }
        } else {
            MYLOGE("Skipping finished broadcast because bugreport could not be generated\n");
        }
    }

    MYLOGD("Final progress: %d/%d (estimated %d)\n", ds.progress_->Get(), ds.progress_->GetMax(),ds.progress_->GetInitialMax());
    ds.progress_->Save();
    MYLOGI("done (id %d)\n", ds.id_);

    if (is_redirecting) {
        fclose(stderr);
    }

    if (use_control_socket && ds.control_socket_fd_ != -1) {
        MYLOGD("Closing control socket\n");
        close(ds.control_socket_fd_);
    }

    return 0;
}


std::string GetProperty(const std::string& key, const std::string& default_value) {
  const prop_info* pi = __system_property_find(key.c_str());
  if (pi == nullptr) return default_value;

  char buf[PROP_VALUE_MAX];
  if (__system_property_read(pi, nullptr, buf) > 0) return buf;

  // If the property exists but is empty, also return the default value.
  // Since we can't remove system properties, "empty" is traditionally
  // the same as "missing" (this was true for cutils' property_get).
  return default_value;
}

static void ShowUsageAndExit(int exitCode = 1) {
    fprintf(stderr,
            "usage: dumpstate [-h] [-b soundfile] [-e soundfile] [-o file] [-d] [-p] "
            "[-z]] [-s] [-S] [-q] [-B] [-P] [-R] [-V version]\n"
            "  -h: display this help message\n"
            "  -b: play sound file instead of vibrate, at beginning of job\n"
            "  -e: play sound file instead of vibrate, at end of job\n"
            "  -o: write to file (instead of stdout)\n"
            "  -d: append date to filename (requires -o)\n"
            "  -p: capture screenshot to filename.png (requires -o)\n"
            "  -z: generate zipped file (requires -o)\n"
            "  -s: write output to control socket (for init)\n"
            "  -S: write file location to control socket (for init; requires -o and -z)"
            "  -q: disable vibrate\n"
            "  -B: send broadcast when finished (requires -o)\n"
            "  -P: send broadcast when started and update system properties on "
            "progress (requires -o and -B)\n"
            "  -R: take bugreport in remote mode (requires -o, -z, -d and -B, "
            "shouldn't be used with -P)\n"
            "  -v: prints the dumpstate header and exit\n");
    exit(exitCode);
}

static void ExitOnInvalidArgs() {
    fprintf(stderr, "invalid combination of args\n");
    ShowUsageAndExit();
}

//	frameworks/native/cmds/dumpstate/utils.cpp
Dumpstate::Dumpstate(const std::string& version)
    : pid_(getpid()), version_(version), now_(time(nullptr)) {
}

Dumpstate& Dumpstate::GetInstance() {
    static Dumpstate singleton_(android::base::GetProperty("dumpstate.version", VERSION_CURRENT));
    return singleton_;
}


//	frameworks/native/cmds/dumpstate/dumpstate.cpp
void Dumpstate::PrintHeader() const {
    std::string build, fingerprint, radio, bootloader, network;
    char date[80];

    build = android::base::GetProperty("ro.build.display.id", "(unknown)");
    fingerprint = android::base::GetProperty("ro.build.fingerprint", "(unknown)");
    radio = android::base::GetProperty("gsm.version.baseband", "(unknown)");
    bootloader = android::base::GetProperty("ro.bootloader", "(unknown)");
    network = android::base::GetProperty("gsm.operator.alpha", "(unknown)");
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now_));

    printf("========================================================\n");
    printf("== dumpstate: %s\n", date);
    printf("========================================================\n");

    printf("\n");
    printf("Build: %s\n", build.c_str());
    // NOTE: fingerprint entry format is important for other tools.
    printf("Build fingerprint: '%s'\n", fingerprint.c_str());
    printf("Bootloader: %s\n", bootloader.c_str());
    printf("Radio: %s\n", radio.c_str());
    printf("Network: %s\n", network.c_str());

    printf("Kernel: ");
    DumpFileToFd(STDOUT_FILENO, "", "/proc/version");
    printf("Command line: %s\n", strtok(cmdline_buf, "\n"));
    printf("Bugreport format version: %s\n", version_.c_str());
    printf("Dumpstate info: id=%d pid=%d dry_run=%d args=%s extra_options=%s\n", id_, pid_,PropertiesHelper::IsDryRun(), args_.c_str(), extra_options_.c_str());
    printf("\n");
}


/* redirect output to a service control socket */
//将
void redirect_to_socket(FILE *redirect, const char *service) {
    int fd = open_socket(service);
    fflush(redirect);
    dup2(fd, fileno(redirect));
    close(fd);
}

int open_socket(const char *service) {
    int s = android_get_control_socket(service);
    if (s < 0) {
        MYLOGE("android_get_control_socket(%s): %s\n", service, strerror(errno));
        exit(1);
    }
    fcntl(s, F_SETFD, FD_CLOEXEC);
    if (listen(s, 4) < 0) {
        MYLOGE("listen(control socket): %s\n", strerror(errno));
        exit(1);
    }

    struct sockaddr addr;
    socklen_t alen = sizeof(addr);
    int fd = accept(s, &addr, &alen);
    if (fd < 0) {
        MYLOGE("accept(control socket): %s\n", strerror(errno));
        exit(1);
    }

    return fd;
}


int android_get_control_socket(const char* name) {
    int fd = __android_get_control_from_env(ANDROID_SOCKET_ENV_PREFIX, name);

    if (fd < 0) return fd;

    // Compare to UNIX domain socket name, must match!
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    int ret = TEMP_FAILURE_RETRY(getsockname(fd, (struct sockaddr *)&addr, &addrlen));
    if (ret < 0) return -1;
    char *path = NULL;
    if (asprintf(&path, ANDROID_SOCKET_DIR "/%s", name) < 0) return -1;
    if (!path) return -1;
    int cmp = strcmp(addr.sun_path, path);
    free(path);
    if (cmp != 0) return -1;

    // It is what we think it is
    return fd;
}

LIBCUTILS_HIDDEN int __android_get_control_from_env(const char* prefix,const char* name) {
    if (!prefix || !name) return -1;

    char *key = NULL;
    if (asprintf(&key, "%s%s", prefix, name) < 0) return -1;
    if (!key) return -1;

    char *cp = key;
    while (*cp) {
        if (!isalnum(*cp)) *cp = '_';
        ++cp;
    }

    const char* val = getenv(key);
    free(key);
    if (!val) return -1;

    errno = 0;
    long fd = strtol(val, NULL, 10);
    if (errno) return -1;

    // validity checking
    if ((fd < 0) || (fd > INT_MAX)) return -1;

    // Since we are inheriting an fd, it could legitimately exceed _SC_OPEN_MAX

    // Still open?
#if defined(F_GETFD) // Lowest overhead
    if (TEMP_FAILURE_RETRY(fcntl(fd, F_GETFD)) < 0) return -1;
#elif defined(F_GETFL) // Alternate lowest overhead
    if (TEMP_FAILURE_RETRY(fcntl(fd, F_GETFL)) < 0) return -1;
#else // Hail Mary pass
    struct stat s;
    if (TEMP_FAILURE_RETRY(fstat(fd, &s)) < 0) return -1;
#endif

    return static_cast<int>(fd);
}








