
//  @   system/core/adb/commandline.cpp
static int adb_query_command(const std::string& command) {
    std::string result;
    std::string error;
    if (!adb_query(command, &result, &error)) {
        fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    printf("%s\n", result.c_str());
    return 0;
}

/**
 * command 为 root 或者 unroot
*/
static bool adb_root(const char* command) {
    std::string error;

    unique_fd fd(adb_connect(android::base::StringPrintf("%s:", command), &error));
    if (fd < 0) {
        fprintf(stderr, "adb: unable to connect for %s: %s\n", command, error.c_str());
        return false;
    }

    // Figure out whether we actually did anything.
    char buf[256];
    char* cur = buf;
    ssize_t bytes_left = sizeof(buf);
    while (bytes_left > 0) {
        ssize_t bytes_read = adb_read(fd, cur, bytes_left);
        if (bytes_read == 0) {
            break;
        } else if (bytes_read < 0) {
            fprintf(stderr, "adb: error while reading for %s: %s\n", command, strerror(errno));
            return false;
        }
        cur += bytes_read;
        bytes_left -= bytes_read;
    }

    if (bytes_left == 0) {
        fprintf(stderr, "adb: unexpected output length for %s\n", command);
        return false;
    }

    fflush(stdout);
    WriteFdExactly(STDOUT_FILENO, buf, sizeof(buf) - bytes_left);
    if (cur != buf && strstr(buf, "restarting") == nullptr) {
        return true;
    }

    // Give adbd some time to kill itself and come back up.
    // We can't use wait-for-device because devices (e.g. adb over network) might not come back.
    std::this_thread::sleep_for(3s);
    return true;
}