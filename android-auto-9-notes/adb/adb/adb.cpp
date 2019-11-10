

//  @  system/core/adb/client/main.cpp 
int main(int argc, char** argv) {
    adb_trace_init(argv);
    return adb_commandline(argc - 1, const_cast<const char**>(argv + 1));
}

//  @   system/core/adb/commandline.cpp
int adb_commandline(int argc, const char** argv) {
    int no_daemon = 0;
    int is_daemon = 0;
    int is_server = 0;
    int r;
    /**
     * 
    */
    TransportType transport_type = kTransportAny;
    int ack_reply_fd = -1;

#if !defined(_WIN32)
    // We'd rather have EPIPE than SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
#endif

    const char* server_host_str = nullptr;
    const char* server_port_str = nullptr;
    const char* server_socket_str = nullptr;

    // We need to check for -d and -e before we look at $ANDROID_SERIAL.
    const char* serial = nullptr;
    TransportId transport_id = 0;

    while (argc > 0) {
        if (!strcmp(argv[0],"server")) {
            is_server = 1;
        } else if (!strcmp(argv[0],"nodaemon")) {
            no_daemon = 1;
        } else if (!strcmp(argv[0], "fork-server")) {
            /* this is a special flag used only when the ADB client launches the ADB Server */
            is_daemon = 1;
        } else if (!strcmp(argv[0], "--reply-fd")) {
            if (argc < 2) return syntax_error("--reply-fd requires an argument");
            const char* reply_fd_str = argv[1];
            argc--;
            argv++;
            ack_reply_fd = strtol(reply_fd_str, nullptr, 10);
            if (!_is_valid_ack_reply_fd(ack_reply_fd)) {
                fprintf(stderr, "adb: invalid reply fd \"%s\"\n", reply_fd_str);
                return 1;
            }
        } else if (!strncmp(argv[0], "-s", 2)) {
            if (isdigit(argv[0][2])) {
                serial = argv[0] + 2;
            } else {
                if (argc < 2 || argv[0][2] != '\0') return syntax_error("-s requires an argument");
                serial = argv[1];
                argc--;
                argv++;
            }
        } else if (!strncmp(argv[0], "-t", 2)) {
            const char* id;
            if (isdigit(argv[0][2])) {
                id = argv[0] + 2;
            } else {
                id = argv[1];
                argc--;
                argv++;
            }
            transport_id = strtoll(id, const_cast<char**>(&id), 10);
            if (*id != '\0') {
                return syntax_error("invalid transport id");
            }
        } else if (!strcmp(argv[0],"-d")) {
            transport_type = kTransportUsb;
        } else if (!strcmp(argv[0],"-e")) {
            transport_type = kTransportLocal;
        } else if (!strcmp(argv[0],"-a")) {
            gListenAll = 1;
        } else if (!strncmp(argv[0], "-H", 2)) {
            if (argv[0][2] == '\0') {
                if (argc < 2) return syntax_error("-H requires an argument");
                server_host_str = argv[1];
                argc--;
                argv++;
            } else {
                server_host_str = argv[0] + 2;
            }
        } else if (!strncmp(argv[0], "-P", 2)) {
            if (argv[0][2] == '\0') {
                if (argc < 2) return syntax_error("-P requires an argument");
                server_port_str = argv[1];
                argc--;
                argv++;
            } else {
                server_port_str = argv[0] + 2;
            }
        } else if (!strcmp(argv[0], "-L")) {
            if (argc < 2) return syntax_error("-L requires an argument");
            server_socket_str = argv[1];
            argc--;
            argv++;
        } else {
            /* out of recognized modifiers and flags */
            break;
        }
        argc--;
        argv++;
    }

    if ((server_host_str || server_port_str) && server_socket_str) {
        return syntax_error("-L is incompatible with -H or -P");
    }

    // If -L, -H, or -P are specified, ignore environment variables.
    // Otherwise, prefer ADB_SERVER_SOCKET over ANDROID_ADB_SERVER_ADDRESS/PORT.
    if (!server_host_str && !server_port_str && !server_socket_str) {
        server_socket_str = getenv("ADB_SERVER_SOCKET");
    }

    if (!server_socket_str) {
        // tcp:1234 and tcp:localhost:1234 are different with -a, so don't default to localhost
        server_host_str = server_host_str ? server_host_str : getenv("ANDROID_ADB_SERVER_ADDRESS");

        int server_port = DEFAULT_ADB_PORT;
        server_port_str = server_port_str ? server_port_str : getenv("ANDROID_ADB_SERVER_PORT");
        if (server_port_str && strlen(server_port_str) > 0) {
            if (!android::base::ParseInt(server_port_str, &server_port, 1, 65535)) {
                fprintf(stderr, "adb: Env var ANDROID_ADB_SERVER_PORT must be a positive" " number less than 65535. Got \"%s\"\n",server_port_str);
                exit(1);
            }
        }

        int rc;
        char* temp;
        if (server_host_str) {
            rc = asprintf(&temp, "tcp:%s:%d", server_host_str, server_port);
        } else {
            rc = asprintf(&temp, "tcp:%d", server_port);
        }
        if (rc < 0) {
            fatal("failed to allocate server socket specification");
        }
        server_socket_str = temp;
    }

    adb_set_socket_spec(server_socket_str);

    // If none of -d, -e, or -s were specified, try $ANDROID_SERIAL.
    if (transport_type == kTransportAny && serial == nullptr) {
        serial = getenv("ANDROID_SERIAL");
    }

    adb_set_transport(transport_type, serial, transport_id);

    if (is_server) {
        if (no_daemon || is_daemon) {
            if (is_daemon && (ack_reply_fd == -1)) {
                fprintf(stderr, "reply fd for adb server to client communication not specified.\n");
                return 1;
            }
            r = adb_server_main(is_daemon, server_socket_str, ack_reply_fd);
        } else {
            r = launch_server(server_socket_str);
        }
        if (r) {
            fprintf(stderr,"* could not start server *\n");
        }
        return r;
    }

    if (argc == 0) {
        help();
        return 1;
    }

    /* handle wait-for-* prefix */
    if (!strncmp(argv[0], "wait-for-", strlen("wait-for-"))) {
        const char* service = argv[0];

        if (!wait_for_device(service)) {
            return 1;
        }

        // Allow a command to be run after wait-for-device,
        // e.g. 'adb wait-for-device shell'.
        if (argc == 1) {
            return 0;
        }

        /* Fall through */
        argc--;
        argv++;
    }

    /* adb_connect() commands */
    if (!strcmp(argv[0], "devices")) {
        const char *listopt;
        if (argc < 2) {
            listopt = "";
        } else if (argc == 2 && !strcmp(argv[1], "-l")) {
            listopt = argv[1];
        } else {
            return syntax_error("adb devices [-l]");
        }
        /**
         * "host:devices"   "host:devices-l"
        */
        std::string query = android::base::StringPrintf("host:%s%s", argv[0], listopt);
        printf("List of devices attached\n");
        return adb_query_command(query);
    }
    else if (!strcmp(argv[0], "connect")) {
        if (argc != 2) return syntax_error("adb connect <host>[:<port>]");

        std::string query = android::base::StringPrintf("host:connect:%s", argv[1]);
        return adb_query_command(query);
    }
    else if (!strcmp(argv[0], "disconnect")) {
        if (argc > 2) return syntax_error("adb disconnect [<host>[:<port>]]");

        std::string query = android::base::StringPrintf("host:disconnect:%s",
                                                        (argc == 2) ? argv[1] : "");
        return adb_query_command(query);
    }
    else if (!strcmp(argv[0], "emu")) {
        return adb_send_emulator_command(argc, argv, serial);
    }
    else if (!strcmp(argv[0], "shell")) {
        return adb_shell(argc, argv);
    }
    else if (!strcmp(argv[0], "exec-in") || !strcmp(argv[0], "exec-out")) {
        int exec_in = !strcmp(argv[0], "exec-in");

        if (argc < 2) return syntax_error("adb %s command", argv[0]);

        std::string cmd = "exec:";
        cmd += argv[1];
        argc -= 2;
        argv += 2;
        while (argc-- > 0) {
            cmd += " " + escape_arg(*argv++);
        }

        std::string error;
        int fd = adb_connect(cmd, &error);
        if (fd < 0) {
            fprintf(stderr, "error: %s\n", error.c_str());
            return -1;
        }

        if (exec_in) {
            copy_to_file(STDIN_FILENO, fd);
        } else {
            copy_to_file(fd, STDOUT_FILENO);
        }

        adb_close(fd);
        return 0;
    }
    else if (!strcmp(argv[0], "kill-server")) {
        return adb_kill_server() ? 0 : 1;
    }
    else if (!strcmp(argv[0], "sideload")) {
        if (argc != 2) return syntax_error("sideload requires an argument");
        if (adb_sideload_host(argv[1])) {
            return 1;
        } else {
            return 0;
        }
    } else if (!strcmp(argv[0], "tcpip")) {
        if (argc != 2) return syntax_error("tcpip requires an argument");
        int port;
        if (!android::base::ParseInt(argv[1], &port, 1, 65535)) {
            return syntax_error("tcpip: invalid port: %s", argv[1]);
        }
        return adb_connect_command(android::base::StringPrintf("tcpip:%d", port));
    }
    else if (!strcmp(argv[0], "remount") ||
             !strcmp(argv[0], "reboot") ||
             !strcmp(argv[0], "reboot-bootloader") ||
             !strcmp(argv[0], "usb") ||
             !strcmp(argv[0], "disable-verity") ||
             !strcmp(argv[0], "enable-verity")) {
        std::string command;
        if (!strcmp(argv[0], "reboot-bootloader")) {
            command = "reboot:bootloader";
        } else if (argc > 1) {
            command = android::base::StringPrintf("%s:%s", argv[0], argv[1]);
        } else {
            command = android::base::StringPrintf("%s:", argv[0]);
        }
        return adb_connect_command(command);
    } else if (!strcmp(argv[0], "root") || !strcmp(argv[0], "unroot")) {
        return adb_root(argv[0]) ? 0 : 1;
    } else if (!strcmp(argv[0], "bugreport")) {
        Bugreport bugreport;
        return bugreport.DoIt(argc, argv);
    } else if (!strcmp(argv[0], "forward") || !strcmp(argv[0], "reverse")) {
        bool reverse = !strcmp(argv[0], "reverse");
        ++argv;
        --argc;
        if (argc < 1) return syntax_error("%s requires an argument", argv[0]);

        // Determine the <host-prefix> for this command.
        std::string host_prefix;
        if (reverse) {
            host_prefix = "reverse";
        } else {
            if (serial) {
                host_prefix = android::base::StringPrintf("host-serial:%s", serial);
            } else if (transport_type == kTransportUsb) {
                host_prefix = "host-usb";
            } else if (transport_type == kTransportLocal) {
                host_prefix = "host-local";
            } else {
                host_prefix = "host";
            }
        }

        std::string cmd, error;
        if (strcmp(argv[0], "--list") == 0) {
            if (argc != 1) return syntax_error("--list doesn't take any arguments");
            return adb_query_command(host_prefix + ":list-forward");
        } else if (strcmp(argv[0], "--remove-all") == 0) {
            if (argc != 1) return syntax_error("--remove-all doesn't take any arguments");
            cmd = host_prefix + ":killforward-all";
        } else if (strcmp(argv[0], "--remove") == 0) {
            // forward --remove <local>
            if (argc != 2) return syntax_error("--remove requires an argument");
            cmd = host_prefix + ":killforward:" + argv[1];
        } else if (strcmp(argv[0], "--no-rebind") == 0) {
            // forward --no-rebind <local> <remote>
            if (argc != 3) return syntax_error("--no-rebind takes two arguments");
            if (forward_targets_are_valid(argv[1], argv[2], &error)) {
                cmd = host_prefix + ":forward:norebind:" + argv[1] + ";" + argv[2];
            }
        } else {
            // forward <local> <remote>
            if (argc != 2) return syntax_error("forward takes two arguments");
            if (forward_targets_are_valid(argv[0], argv[1], &error)) {
                cmd = host_prefix + ":forward:" + argv[0] + ";" + argv[1];
            }
        }

        if (!error.empty()) {
            fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }

        int fd = adb_connect(cmd, &error);
        if (fd < 0 || !adb_status(fd, &error)) {
            adb_close(fd);
            fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }

        // Server or device may optionally return a resolved TCP port number.
        std::string resolved_port;
        if (ReadProtocolString(fd, &resolved_port, &error) && !resolved_port.empty()) {
            printf("%s\n", resolved_port.c_str());
        }

        ReadOrderlyShutdown(fd);
        return 0;
    }
    /* do_sync_*() commands */
    else if (!strcmp(argv[0], "ls")) {
        if (argc != 2) return syntax_error("ls requires an argument");
        return do_sync_ls(argv[1]) ? 0 : 1;
    }
    else if (!strcmp(argv[0], "push")) {
        bool copy_attrs = false;
        bool sync = false;
        std::vector<const char*> srcs;
        const char* dst = nullptr;

        parse_push_pull_args(&argv[1], argc - 1, &srcs, &dst, &copy_attrs, &sync);
        if (srcs.empty() || !dst) return syntax_error("push requires an argument");
        return do_sync_push(srcs, dst, sync) ? 0 : 1;
    }
    else if (!strcmp(argv[0], "pull")) {
        bool copy_attrs = false;
        std::vector<const char*> srcs;
        const char* dst = ".";

        parse_push_pull_args(&argv[1], argc - 1, &srcs, &dst, &copy_attrs, nullptr);
        if (srcs.empty()) return syntax_error("pull requires an argument");
        return do_sync_pull(srcs, dst, copy_attrs) ? 0 : 1;
    }
    else if (!strcmp(argv[0], "install")) {
        if (argc < 2) return syntax_error("install requires an argument");
        if (_use_legacy_install()) {
            return install_app_legacy(argc, argv);
        }
        return install_app(argc, argv);
    }
    else if (!strcmp(argv[0], "install-multiple")) {
        if (argc < 2) return syntax_error("install-multiple requires an argument");
        return install_multiple_app(argc, argv);
    }
    else if (!strcmp(argv[0], "uninstall")) {
        if (argc < 2) return syntax_error("uninstall requires an argument");
        if (_use_legacy_install()) {
            return uninstall_app_legacy(argc, argv);
        }
        return uninstall_app(argc, argv);
    }
    else if (!strcmp(argv[0], "sync")) {
        std::string src;
        bool list_only = false;
        if (argc < 2) {
            // No partition specified: sync all of them.
        } else if (argc >= 2 && strcmp(argv[1], "-l") == 0) {
            list_only = true;
            if (argc == 3) src = argv[2];
        } else if (argc == 2) {
            src = argv[1];
        } else {
            return syntax_error("adb sync [-l] [PARTITION]");
        }

        if (src.empty()) src = "all";
        std::vector<std::string> partitions{"data", "odm", "oem", "product", "system", "vendor"};
        bool found = false;
        for (const auto& partition : partitions) {
            if (src == "all" || src == partition) {
                std::string src_dir{product_file(partition)};
                if (!directory_exists(src_dir)) continue;
                found = true;
                if (!do_sync_sync(src_dir, "/" + partition, list_only)) return 1;
            }
        }
        return found ? 0 : syntax_error("don't know how to sync %s partition", src.c_str());
    }
    /* passthrough commands */
    else if (!strcmp(argv[0],"get-state") ||
        !strcmp(argv[0],"get-serialno") ||
        !strcmp(argv[0],"get-devpath"))
    {
        return adb_query_command(format_host_command(argv[0]));
    }
    /* other commands */
    else if (!strcmp(argv[0],"logcat") || !strcmp(argv[0],"lolcat") || !strcmp(argv[0],"longcat")) {
        return logcat(argc, argv);
    }
    else if (!strcmp(argv[0],"ppp")) {
        return ppp(argc, argv);
    }
    else if (!strcmp(argv[0], "start-server")) {
        std::string error;
        const int result = adb_connect("host:start-server", &error);
        if (result < 0) {
            fprintf(stderr, "error: %s\n", error.c_str());
        }
        return result;
    }
    else if (!strcmp(argv[0], "backup")) {
        return backup(argc, argv);
    }
    else if (!strcmp(argv[0], "restore")) {
        return restore(argc, argv);
    }
    else if (!strcmp(argv[0], "keygen")) {
        if (argc != 2) return syntax_error("keygen requires an argument");
        // Always print key generation information for keygen command.
        adb_trace_enable(AUTH);
        return adb_auth_keygen(argv[1]);
    }
    else if (!strcmp(argv[0], "jdwp")) {
        return adb_connect_command("jdwp");
    }
    else if (!strcmp(argv[0], "track-jdwp")) {
        return adb_connect_command("track-jdwp");
    }
    else if (!strcmp(argv[0], "track-devices")) {
        return adb_connect_command("host:track-devices");
    }


    /* "adb /?" is a common idiom under Windows */
    else if (!strcmp(argv[0], "--help") || !strcmp(argv[0], "help") || !strcmp(argv[0], "/?")) {
        help();
        return 0;
    }
    else if (!strcmp(argv[0], "--version") || !strcmp(argv[0], "version")) {
        fprintf(stdout, "%s", adb_version().c_str());
        return 0;
    } else if (!strcmp(argv[0], "features")) {
        // Only list the features common to both the adb client and the device.
        FeatureSet features;
        std::string error;
        if (!adb_get_feature_set(&features, &error)) {
            fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }

        for (const std::string& name : features) {
            if (CanUseFeature(features, name)) {
                printf("%s\n", name.c_str());
            }
        }
        return 0;
    } else if (!strcmp(argv[0], "host-features")) {
        return adb_query_command("host:host-features");
    } else if (!strcmp(argv[0], "reconnect")) {
        if (argc == 1) {
            return adb_query_command(format_host_command(argv[0]));
        } else if (argc == 2) {
            if (!strcmp(argv[1], "device")) {
                std::string err;
                adb_connect("reconnect", &err);
                return 0;
            } else if (!strcmp(argv[1], "offline")) {
                std::string err;
                return adb_query_command("host:reconnect-offline");
            } else {
                return syntax_error("adb reconnect [device|offline]");
            }
        }
    }

    syntax_error("unknown command %s", argv[0]);
    return 1;
}


