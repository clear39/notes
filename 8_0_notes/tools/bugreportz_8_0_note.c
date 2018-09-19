
//	frameworks/native/cmds/bugreportz/main.cpp
int main(int argc, char* argv[]) {
    bool show_progress = false;
    if (argc > 1) {
        /* parse arguments */
        int c;
        while ((c = getopt(argc, argv, "hpv")) != -1) {
            switch (c) {
                case 'h':
                    show_usage();
                    return EXIT_SUCCESS;
                case 'p':
                    show_progress = true;
                    break;
                case 'v':
                    show_version();
                    return EXIT_SUCCESS;
                default:
                    show_usage();
                    return EXIT_FAILURE;
            }
        }
    }

    // TODO: code below was copy-and-pasted from bugreport.cpp (except by the
    // timeout value);
    // should be reused instead.

    // Start the dumpstatez service.
    property_set("ctl.start", "dumpstatez");

    // Socket will not be available until service starts.
    int s;
    for (int i = 0; i < 20; i++) {
        s = socket_local_client("dumpstate", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
        if (s >= 0) break;
        // Try again in 1 second.
        sleep(1);
    }

    if (s == -1) {
        printf("FAIL:Failed to connect to dumpstatez service: %s\n", strerror(errno));
        return EXIT_SUCCESS;
    }

    // Set a timeout so that if nothing is read in 10 minutes, we'll stop
    // reading and quit. No timeout in dumpstate is longer than 60 seconds,
    // so this gives lots of leeway in case of unforeseen time outs.
    struct timeval tv;
    tv.tv_sec = 10 * 60;
    tv.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        fprintf(stderr, "WARNING: Cannot set socket timeout: %s\n", strerror(errno));
    }

    bugreportz(s, show_progress);
}


//	property_set("ctl.start", "dumpstatez");
//	frameworks/native/cmds/dumpstate/dumpstate.rc
# dumpstatez generates a zipped bugreport but also uses a socket to print the file location once it is finished.
service dumpstatez /system/bin/dumpstate -S -d -z -o /data/user_de/0/com.android.shell/files/bugreports/bugreport
    socket dumpstate stream 0660 shell log
    class main
    disabled
    oneshot





int bugreportz(int s, bool show_progress) {
    std::string line;
    while (1) {
        char buffer[65536];
        ssize_t bytes_read = TEMP_FAILURE_RETRY(read(s, buffer, sizeof(buffer)));
        if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            // EAGAIN really means time out, so change the errno.
            if (errno == EAGAIN) {
                errno = ETIMEDOUT;
            }
            printf("FAIL:Bugreport read terminated abnormally (%s)\n", strerror(errno));
            break;
        }

        // Writes line by line.
        for (int i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            line.append(1, c);
            if (c == '\n') {
                write_line(line, show_progress);
                line.clear();
            }
        }
    }
    // Process final line, in case it didn't finish with newline
    write_line(line, show_progress);

    if (close(s) == -1) {
        fprintf(stderr, "WARNING: error closing socket: %s\n", strerror(errno));
    }
    return EXIT_SUCCESS;
}

static constexpr char BEGIN_PREFIX[] = "BEGIN:";
static constexpr char PROGRESS_PREFIX[] = "PROGRESS:";
static void write_line(const std::string& line, bool show_progress) {
    if (line.empty()) return;

    // When not invoked with the -p option, it must skip BEGIN and PROGRESS lines otherwise it
    // will break adb (which is expecting either OK or FAIL).
    if (!show_progress && (android::base::StartsWith(line, PROGRESS_PREFIX) || android::base::StartsWith(line, BEGIN_PREFIX)))
        return;

    android::base::WriteStringToFd(line, STDOUT_FILENO);
}

//	system/core/base/file.cpp
bool WriteStringToFd(const std::string& content, int fd) {
  const char* p = content.data();
  size_t left = content.size();
  while (left > 0) {
    ssize_t n = TEMP_FAILURE_RETRY(write(fd, p, left));
    if (n == -1) {
      return false;
    }
    p += n;
    left -= n;
  }
  return true;
}


//	system/core/base/strings.cpp
bool StartsWith(const std::string& s, const char* prefix) {
  return strncmp(s.c_str(), prefix, strlen(prefix)) == 0;
}







