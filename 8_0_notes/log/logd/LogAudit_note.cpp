// LogAudit listens on NETLINK_AUDIT socket for selinux
// initiated log messages. New log entries are added to LogBuffer
// and LogReader is notified to send updates to connected clients.




//@	system/core/logd/LogAudit.cpp
//通过NETLINK机制，连接内核NETLINK_AUDIT socket
LogAudit::LogAudit(LogBuffer* buf, LogReader* reader, int fdDmesg)
    : SocketListener(mSock = getLogSocket(), false),
      logbuf(buf),
      reader(reader),
      fdDmesg(fdDmesg),
      main(__android_logger_property_get_bool("ro.logd.auditd.main",BOOL_DEFAULT_TRUE)), //ro.logd.auditd.main 属性值为空
      events(__android_logger_property_get_bool("ro.logd.auditd.events",BOOL_DEFAULT_TRUE)),//ro.logd.auditd.main 属性值为空
      initialized(false),	//为bool类型
      tooFast(false) {	//为bool类型
    static const char auditd_message[] = { KMSG_PRIORITY(LOG_INFO),
                                           'l',
                                           'o',
                                           'g',
                                           'd',
                                           '.',
                                           'a',
                                           'u',
                                           'd',
                                           'i',
                                           't',
                                           'd',
                                           ':',
                                           ' ',
                                           's',
                                           't',
                                           'a',
                                           'r',
                                           't',
                                           '\n' };
    write(fdDmesg, auditd_message, sizeof(auditd_message));	//	fdDmesg = "/dev/kmsg"
}


int LogAudit::getLogSocket() {
    int fd = audit_open();
    if (fd < 0) {
        return fd;
    }
    if (audit_setup(fd, getpid()) < 0) {
        audit_close(fd);
        fd = -1;
    }
    //	#define AUDIT_RATE_LIMIT_DEFAULT 20        /* acceptable burst rate      */
    (void)audit_rate_limit(fd, AUDIT_RATE_LIMIT_DEFAULT);
    return fd;
}

//	@system/core/logd/libaudit.c
int audit_open() {
    return socket(PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_AUDIT);
}

//	@system/core/logd/libaudit.c
int audit_setup(int fd, pid_t pid) {
    int rc;
    struct audit_message rep;
    struct audit_status status;

    memset(&status, 0, sizeof(status));

    /*
     * In order to set the auditd PID we send an audit message over the netlink
     * socket with the pid field of the status struct set to our current pid,
     * and the the mask set to AUDIT_STATUS_PID
     */
    status.pid = pid;
    status.mask = AUDIT_STATUS_PID;

    /* Let the kernel know this pid will be registering for audit events */
    rc = audit_send(fd, AUDIT_SET, &status, sizeof(status));
    if (rc < 0) {
        return rc;
    }

    /*
     * In a request where we need to wait for a response, wait for the message
     * and discard it. This message confirms and sync's us with the kernel.
     * This daemon is now registered as the audit logger.
     *
     * TODO
     * If the daemon dies and restarts the message didn't come back,
     * so I went to non-blocking and it seemed to fix the bug.
     * Need to investigate further.
     */
    audit_get_reply(fd, &rep, GET_REPLY_NONBLOCKING, 0);

    return 0;
}


static int audit_send(int fd, int type, const void* data, size_t size) {
    int rc;
    static int16_t sequence = 0;
    struct audit_message req;
    struct sockaddr_nl addr;

    memset(&req, 0, sizeof(req));
    memset(&addr, 0, sizeof(addr));

    /* We always send netlink messaged */
    addr.nl_family = AF_NETLINK;

    /* Set up the netlink headers */
    req.nlh.nlmsg_type = type;
    req.nlh.nlmsg_len = NLMSG_SPACE(size);
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

    /*
     * Check for a valid fd, even though sendto would catch this, its easier
     * to always blindly increment the sequence number
     */
    if (fd < 0) {
        return -EBADF;
    }

    /* Ensure the message is not too big */
    if (NLMSG_SPACE(size) > MAX_AUDIT_MESSAGE_LENGTH) {
        return -EINVAL;
    }

    /* Only memcpy in the data if it was specified */
    if (size && data) {
        memcpy(NLMSG_DATA(&req.nlh), data, size);
    }

    /*
     * Only increment the sequence number on a guarantee
     * you will send it to the kernel.
     *
     * Also, the sequence is defined as a u32 in the kernel
     * struct. Using an int here might not work on 32/64 bit splits. A
     * signed 64 bit value can overflow a u32..but a u32
     * might not fit in the response, so we need to use s32.
     * Which is still kind of hackish since int could be 16 bits
     * in size. The only safe type to use here is a signed 16
     * bit value.
     */
    req.nlh.nlmsg_seq = ++sequence;

    /* While failing and its due to interrupts */

    rc = TEMP_FAILURE_RETRY(sendto(fd, &req, req.nlh.nlmsg_len, 0,(struct sockaddr*)&addr, sizeof(addr)));

    /* Not all the bytes were sent */
    if (rc < 0) {
        rc = -errno;
        goto out;
    } else if ((uint32_t)rc != req.nlh.nlmsg_len) {
        rc = -EPROTO;
        goto out;
    }

    /* We sent all the bytes, get the ack */
    rc = get_ack(fd);

    /* If the ack failed, return the error, else return the sequence number */
    rc = (rc == 0) ? (int)sequence : rc;

out:
    /* Don't let sequence roll to negative */
    if (sequence < 0) {
        sequence = 0;
    }

    return rc;
}


int audit_get_reply(int fd, struct audit_message* rep, reply_t block, int peek) {
    ssize_t len;
    int flags;
    int rc = 0;

    struct sockaddr_nl nladdr;
    socklen_t nladdrlen = sizeof(nladdr);

    if (fd < 0) {
        return -EBADF;
    }

    /* Set up the flags for recv from */
    flags = (block == GET_REPLY_NONBLOCKING) ? MSG_DONTWAIT : 0;
    flags |= peek;

    /*
     * Get the data from the netlink socket but on error we need to be carefull,
     * the interface shows that EINTR can never be returned, other errors,
     * however, can be returned.
     */
    len = TEMP_FAILURE_RETRY(recvfrom(fd, rep, sizeof(*rep), flags,(struct sockaddr*)&nladdr, &nladdrlen));

    /*
     * EAGAIN should be re-tried until success or another error manifests.
     */
    if (len < 0) {
        rc = -errno;
        if (block == GET_REPLY_NONBLOCKING && rc == -EAGAIN) {
            /* If request is non blocking and errno is EAGAIN, just return 0 */
            return 0;
        }
        return rc;
    }

    if (nladdrlen != sizeof(nladdr)) {
        return -EPROTO;
    }

    /* Make sure the netlink message was not spoof'd */
    if (nladdr.nl_pid) {
        return -EINVAL;
    }

    /* Check if the reply from the kernel was ok */
    if (!NLMSG_OK(&rep->nlh, (size_t)len)) {
        rc = (len == sizeof(*rep)) ? -EFBIG : -EBADE;
    }

    return rc;
}


int audit_rate_limit(int fd, unsigned rate_limit) {
    int rc;
    struct audit_message rep;
    struct audit_status status;

    memset(&status, 0, sizeof(status));

    status.mask = AUDIT_STATUS_RATE_LIMIT;
    status.rate_limit = rate_limit; /* audit entries per second */

    rc = audit_send(fd, AUDIT_SET, &status, sizeof(status));
    if (rc < 0) {
        return rc;
    }

    audit_get_reply(fd, &rep, GET_REPLY_NONBLOCKING, 0);

    return 0;
}




int LogAudit::log(char* buf, size_t len) {
    char* audit = strstr(buf, " audit(");
    if (!audit || (audit >= &buf[len])) {
        return 0;
    }

    *audit = '\0';

    int rc;
    char* type = strstr(buf, "type=");
    if (type && (type < &buf[len])) {
        rc = logPrint("%s %s", type, audit + 1);
    } else {
        rc = logPrint("%s", audit + 1);
    }
    *audit = ' ';
    return rc;
}


int LogAudit::logPrint(const char* fmt, ...) {
    if (fmt == NULL) {
        return -EINVAL;
    }

    va_list args;

    char* str = NULL;
    va_start(args, fmt);
    int rc = vasprintf(&str, fmt, args);
    va_end(args);

    if (rc < 0) {
        return rc;
    }

    char* cp;
    // Work around kernels missing
    // https://github.com/torvalds/linux/commit/b8f89caafeb55fba75b74bea25adc4e4cd91be67
    // Such kernels improperly add newlines inside audit messages.
    while ((cp = strchr(str, '\n'))) {
        *cp = ' ';
    }

    while ((cp = strstr(str, "  "))) {
        memmove(cp, cp + 1, strlen(cp + 1) + 1);
    }

    bool info = strstr(str, " permissive=1") || strstr(str, " policy loaded ");
    if ((fdDmesg >= 0) && initialized) {
        struct iovec iov[3];
        static const char log_info[] = { KMSG_PRIORITY(LOG_INFO) };
        static const char log_warning[] = { KMSG_PRIORITY(LOG_WARNING) };
        static const char newline[] = "\n";

        // Dedupe messages, checking for identical messages starting with avc:
        static unsigned count;
        static char* last_str;
        static bool last_info;

        if (last_str != NULL) {
            static const char avc[] = "): avc: ";
            char* avcl = strstr(last_str, avc);
            bool skip = false;

            if (avcl) {
                char* avcr = strstr(str, avc);

                skip = avcr && !fastcmp<strcmp>(avcl + strlen(avc), avcr + strlen(avc));
                if (skip) {
                    ++count;
                    free(last_str);
                    last_str = strdup(str);
                    last_info = info;
                }
            }
            if (!skip) {
                static const char resume[] = " duplicate messages suppressed\n";

                iov[0].iov_base = last_info ? const_cast<char*>(log_info)
                                            : const_cast<char*>(log_warning);
                iov[0].iov_len =
                    last_info ? sizeof(log_info) : sizeof(log_warning);
                iov[1].iov_base = last_str;
                iov[1].iov_len = strlen(last_str);
                if (count > 1) {
                    iov[2].iov_base = const_cast<char*>(resume);
                    iov[2].iov_len = strlen(resume);
                } else {
                    iov[2].iov_base = const_cast<char*>(newline);
                    iov[2].iov_len = strlen(newline);
                }

                writev(fdDmesg, iov, arraysize(iov));
                free(last_str);
                last_str = NULL;
            }
        }
        if (last_str == NULL) {
            count = 0;
            last_str = strdup(str);
            last_info = info;
        }
        if (count == 0) {
            iov[0].iov_base = info ? const_cast<char*>(log_info) : const_cast<char*>(log_warning);
            iov[0].iov_len = info ? sizeof(log_info) : sizeof(log_warning);
            iov[1].iov_base = str;
            iov[1].iov_len = strlen(str);
            iov[2].iov_base = const_cast<char*>(newline);
            iov[2].iov_len = strlen(newline);

            writev(fdDmesg, iov, arraysize(iov));
        }
    }

    if (!main && !events) {
        free(str);
        return 0;
    }

    pid_t pid = getpid();
    pid_t tid = gettid();
    uid_t uid = AID_LOGD;
    log_time now;

    static const char audit_str[] = " audit(";
    char* timeptr = strstr(str, audit_str);
    if (timeptr && ((cp = now.strptime(timeptr + sizeof(audit_str) - 1, "%s.%q"))) && (*cp == ':')) {
        memcpy(timeptr + sizeof(audit_str) - 1, "0.0", 3);
        memmove(timeptr + sizeof(audit_str) - 1 + 3, cp, strlen(cp) + 1);
        if (!isMonotonic()) {
            if (android::isMonotonic(now)) {
                LogKlog::convertMonotonicToReal(now);
            }
        } else {
            if (!android::isMonotonic(now)) {
                LogKlog::convertRealToMonotonic(now);
            }
        }
    } else if (isMonotonic()) {
        now = log_time(CLOCK_MONOTONIC);
    } else {
        now = log_time(CLOCK_REALTIME);
    }

    static const char pid_str[] = " pid=";
    char* pidptr = strstr(str, pid_str);
    if (pidptr && isdigit(pidptr[sizeof(pid_str) - 1])) {
        cp = pidptr + sizeof(pid_str) - 1;
        pid = 0;
        while (isdigit(*cp)) {
            pid = (pid * 10) + (*cp - '0');
            ++cp;
        }
        tid = pid;
        logbuf->wrlock();
        uid = logbuf->pidToUid(pid);
        logbuf->unlock();
        memmove(pidptr, cp, strlen(cp) + 1);
    }

    // log to events

    size_t l = strnlen(str, LOGGER_ENTRY_MAX_PAYLOAD);
    size_t n = l + sizeof(android_log_event_string_t);

    bool notify = false;

    if (events) {  // begin scope for event buffer
        uint32_t buffer[(n + sizeof(uint32_t) - 1) / sizeof(uint32_t)];

        android_log_event_string_t* event =
            reinterpret_cast<android_log_event_string_t*>(buffer);
        event->header.tag = htole32(AUDITD_LOG_TAG);
        event->type = EVENT_TYPE_STRING;
        event->length = htole32(l);
        memcpy(event->data, str, l);

        rc = logbuf->log(LOG_ID_EVENTS, now, uid, pid, tid,
                         reinterpret_cast<char*>(event),
                         (n <= USHRT_MAX) ? (unsigned short)n : USHRT_MAX);
        if (rc >= 0) {
            notify = true;
        }
        // end scope for event buffer
    }

    // log to main

    static const char comm_str[] = " comm=\"";
    const char* comm = strstr(str, comm_str);
    const char* estr = str + strlen(str);
    const char* commfree = NULL;
    if (comm) {
        estr = comm;
        comm += sizeof(comm_str) - 1;
    } else if (pid == getpid()) {
        pid = tid;
        comm = "auditd";
    } else {
        logbuf->wrlock();
        comm = commfree = logbuf->pidToName(pid);
        logbuf->unlock();
        if (!comm) {
            comm = "unknown";
        }
    }

    const char* ecomm = strchr(comm, '"');
    if (ecomm) {
        ++ecomm;
        l = ecomm - comm;
    } else {
        l = strlen(comm) + 1;
        ecomm = "";
    }
    size_t b = estr - str;
    if (b > LOGGER_ENTRY_MAX_PAYLOAD) {
        b = LOGGER_ENTRY_MAX_PAYLOAD;
    }
    size_t e = strnlen(ecomm, LOGGER_ENTRY_MAX_PAYLOAD - b);
    n = b + e + l + 2;

    if (main) {  // begin scope for main buffer
        char newstr[n];

        *newstr = info ? ANDROID_LOG_INFO : ANDROID_LOG_WARN;
        strlcpy(newstr + 1, comm, l);
        strncpy(newstr + 1 + l, str, b);
        strncpy(newstr + 1 + l + b, ecomm, e);

        rc = logbuf->log(LOG_ID_MAIN, now, uid, pid, tid, newstr,
                         (n <= USHRT_MAX) ? (unsigned short)n : USHRT_MAX);

        if (rc >= 0) {
            notify = true;
        }
        // end scope for main buffer
    }

    free(const_cast<char*>(commfree));
    free(str);

    if (notify) {
        reader->notifyNewLog();
        if (rc < 0) {
            rc = n;
        }
    }

    return rc;
}