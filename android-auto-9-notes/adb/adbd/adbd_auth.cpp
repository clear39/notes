//  system/core/adb/adbd_auth.cpp

/**
 * int adbd_main(int server_port) {
 * --> 
*/
void adbd_cloexec_auth_socket() {
    /**
     * 
    */
    int fd = android_get_control_socket("adbd");
    if (fd == -1) {
        PLOG(ERROR) << "Failed to get adbd socket";
        return;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
}

/**
 * int adbd_main(int server_port) {
 * --> 
*/
void adbd_auth_init(void) {
    int fd = android_get_control_socket("adbd");
    if (fd == -1) {
        PLOG(ERROR) << "Failed to get adbd socket";
        return;
    }

    if (listen(fd, 4) == -1) {
        PLOG(ERROR) << "Failed to listen on '" << fd << "'";
        return;
    }

    fdevent_install(&listener_fde, fd, adbd_auth_listener, NULL);
    fdevent_add(&listener_fde, FDE_READ);
}

//  @   system/core/libcutils/sockets_unix.cpp
int android_get_control_socket(const char* name /* = adbd*/) {
    /**
     * system/core/libcutils/include/cutils/sockets.h:45:
     * #define ANDROID_SOCKET_ENV_PREFIX	"ANDROID_SOCKET_"
     * 
     *
    */
    int fd = __android_get_control_from_env(ANDROID_SOCKET_ENV_PREFIX, name);

    if (fd < 0) return fd;

    // Compare to UNIX domain socket name, must match!
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    int ret = TEMP_FAILURE_RETRY(getsockname(fd, (struct sockaddr *)&addr, &addrlen));
    if (ret < 0) return -1;
    char *path = NULL;
    /**
     * system/core/libcutils/include/cutils/sockets.h:46:
     * #define ANDROID_SOCKET_DIR		"/dev/socket"
    */
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
    /**
     *  key = ANDROID_SOCKET_adbd
    */
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




static void adbd_auth_listener(int fd, unsigned events, void* data) {
    int s = adb_socket_accept(fd, nullptr, nullptr);
    if (s < 0) {
        PLOG(ERROR) << "Failed to accept";
        return;
    }

    if (framework_fd >= 0) {
        LOG(WARNING) << "adb received framework auth socket connection again";
        framework_disconnected();
    }

    framework_fd = s;
    fdevent_install(&framework_fde, framework_fd, adbd_auth_event, nullptr);
    fdevent_add(&framework_fde, FDE_READ);

    if (needs_retry) {
        needs_retry = false;
        /**
         * static atransport* usb_transport;
        */
        send_auth_request(usb_transport);
    }
}

static void adbd_auth_event(int fd, unsigned events, void*) {
    if (events & FDE_READ) {
        char response[2];
        int ret = unix_read(fd, response, sizeof(response));
        if (ret <= 0) {
            framework_disconnected();
        } else if (ret == 2 && response[0] == 'O' && response[1] == 'K') {
            if (usb_transport) {
                adbd_auth_verified(usb_transport);
            }
        }
    }
}


void adbd_auth_verified(atransport* t) {
    LOG(INFO) << "adb client authorized";
    handle_online(t);
    send_connect(t);
}



void adbd_auth_confirm_key(const char* key, size_t len, atransport* t) {
    if (!usb_transport) {
        usb_transport = t;
        t->AddDisconnect(&usb_disconnect);
    }

    if (framework_fd < 0) {
        LOG(ERROR) << "Client not connected";
        needs_retry = true;
        return;
    }

    if (key[len - 1] != '\0') {
        LOG(ERROR) << "Key must be a null-terminated string";
        return;
    }

    char msg[MAX_PAYLOAD_V1];
    int msg_len = snprintf(msg, sizeof(msg), "PK%s", key);
    if (msg_len >= static_cast<int>(sizeof(msg))) {
        LOG(ERROR) << "Key too long (" << msg_len << ")";
        return;
    }
    LOG(DEBUG) << "Sending '" << msg << "'";

    if (unix_write(framework_fd, msg, msg_len) == -1) {
        PLOG(ERROR) << "Failed to write PK";
        return;
    }
}