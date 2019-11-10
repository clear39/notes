//  @   system/core/adb/adb_client.cpp
bool adb_query(const std::string& service, std::string* result, std::string* error) {
    D("adb_query: %s", service.c_str()); //
    int fd = adb_connect(service, error);
    if (fd < 0) {
        return false;
    }

    result->clear();
    if (!ReadProtocolString(fd, result, error)) {
        adb_close(fd);
        return false;
    }

    ReadOrderlyShutdown(fd);
    adb_close(fd);
    return true;
}




int adb_connect(const std::string& service, std::string* error) {
    // first query the adb server's version
    int fd = _adb_connect("host:version", error);

    D("adb_connect: service %s", service.c_str());
    if (fd == -2 && !is_local_socket_spec(__adb_server_socket_spec)) {
        fprintf(stderr, "* cannot start server on remote host\n");
        // error is the original network connection error
        return fd;
    } else if (fd == -2) {
        fprintf(stderr, "* daemon not running; starting now at %s\n", __adb_server_socket_spec);
    start_server:
        if (launch_server(__adb_server_socket_spec)) {
            fprintf(stderr, "* failed to start daemon\n");
            // launch_server() has already printed detailed error info, so just
            // return a generic error string about the overall adb_connect()
            // that the caller requested.
            *error = "cannot connect to daemon";
            return -1;
        } else {
            fprintf(stderr, "* daemon started successfully\n");
        }
        // The server will wait until it detects all of its connected devices before acking.
        // Fall through to _adb_connect.
    } else {
        // If a server is already running, check its version matches.
        int version = ADB_SERVER_VERSION - 1;

        // If we have a file descriptor, then parse version result.
        if (fd >= 0) {
            std::string version_string;
            if (!ReadProtocolString(fd, &version_string, error)) {
                adb_close(fd);
                return -1;
            }

            ReadOrderlyShutdown(fd);
            adb_close(fd);

            if (sscanf(&version_string[0], "%04x", &version) != 1) {
                *error = android::base::StringPrintf("cannot parse version string: %s",version_string.c_str());
                return -1;
            }
        } else {
            // If fd is -1 check for "unknown host service" which would
            // indicate a version of adb that does not support the
            // version command, in which case we should fall-through to kill it.
            if (*error != "unknown host service") {
                return fd;
            }
        }

        if (version != ADB_SERVER_VERSION) {
            fprintf(stderr, "adb server version (%d) doesn't match this client (%d); killing...\n",version, ADB_SERVER_VERSION);
            adb_kill_server();
            goto start_server;
        }
    }

    // if the command is start-server, we are done.
    if (service == "host:start-server") {
        return 0;
    }

    fd = _adb_connect(service, error);
    if (fd == -1) {
        D("_adb_connect error: %s", error->c_str());
    } else if(fd == -2) {
        fprintf(stderr, "* daemon still not running\n");
    }
    D("adb_connect: return fd %d", fd);

    return fd;
}




static int _adb_connect(const std::string& service, std::string* error) {
    D("_adb_connect: %s", service.c_str());// host:version
    /**
     * system/core/adb/adb.h:34:constexpr size_t MAX_PAYLOAD = 1024 * 1024;
    */
    if (service.empty() || service.size() > MAX_PAYLOAD) {
        *error = android::base::StringPrintf("bad service name length (%zd)",service.size());
        return -1;
    }

    std::string reason;
    /**
     * @    system/core/adb/socket_spec.cpp
    */
    int fd = socket_spec_connect(__adb_server_socket_spec, &reason);
    if (fd < 0) {
        *error = android::base::StringPrintf("cannot connect to daemon at %s: %s", __adb_server_socket_spec, reason.c_str());
        return -2;
    }

    if (memcmp(&service[0], "host", 4) != 0 && switch_socket_transport(fd, error)) {
        return -1;
    }

    if (!SendProtocolString(fd, service)) {
        *error = perror_str("write failure during connection");
        adb_close(fd);
        return -1;
    }

    if (!adb_status(fd, error)) {
        adb_close(fd);
        return -1;
    }

    D("_adb_connect: return fd %d", fd);
    return fd;
}
