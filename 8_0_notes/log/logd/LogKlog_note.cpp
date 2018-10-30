//	@system/core/logd/LogKlog.cpp
//	 kl = new LogKlog(logBuf, reader, fdDmesg = "/dev/kmsg", fdPmesg = "/proc/kmsg", al != nullptr);
LogKlog::LogKlog(LogBuffer* buf, LogReader* reader, int fdWrite == fdDmesg , int fdRead == fdPmesg,bool auditd)
    : SocketListener(fdRead, false),
      logbuf(buf),
      reader(reader),
      signature(CLOCK_MONOTONIC),
      initialized(false),
      enableLogging(true),
      auditd(auditd) {
    static const char klogd_message[] = "%s%s%" PRIu64 "\n";
    char buffer[strlen(priority_message) + strlen(klogdStr) + strlen(klogd_message) + 20];
    snprintf(buffer, sizeof(buffer), klogd_message, priority_message, klogdStr, signature.nsec());
    write(fdWrite, buffer, strlen(buffer));
}

bool isMonotonic() {
    return logbuf->isMonotonic();
}

// Passed the entire SYSLOG_ACTION_READ_ALL buffer and interpret a
// compensated start time.
void LogKlog::synchronize(const char* buf, ssize_t len) {
    const char* cp = android::strnstr(buf, len, suspendStr);
    if (!cp) {
        cp = android::strnstr(buf, len, resumeStr);
        if (!cp) return;
    } else {
        const char* rp = android::strnstr(buf, len, resumeStr);
        if (rp && (rp < cp)) cp = rp;
    }

    do {
        --cp;
    } while ((cp > buf) && (*cp != '\n'));
    if (*cp == '\n') {
        ++cp;
    }
    parseKernelPrio(cp, len - (cp - buf));

    log_time now;
    sniffTime(now, cp, len - (cp - buf), true);

    const char* suspended = android::strnstr(buf, len, suspendedStr);
    if (!suspended || (suspended > cp)) {
        return;
    }
    cp = suspended;

    do {
        --cp;
    } while ((cp > buf) && (*cp != '\n'));
    if (*cp == '\n') {
        ++cp;
    }
    parseKernelPrio(cp, len - (cp - buf));

    sniffTime(now, cp, len - (cp - buf), true);
}


//
// log a message into the kernel log buffer
//
// Filter rules to parse <PRI> <TIME> <tag> and <message> in order for
// them to appear correct in the logcat output:
//
// LOG_KERN (0):
// <PRI>[<TIME>] <tag> ":" <message>
// <PRI>[<TIME>] <tag> <tag> ":" <message>
// <PRI>[<TIME>] <tag> <tag>_work ":" <message>
// <PRI>[<TIME>] <tag> '<tag>.<num>' ":" <message>
// <PRI>[<TIME>] <tag> '<tag><num>' ":" <message>
// <PRI>[<TIME>] <tag>_host '<tag>.<num>' ":" <message>
// (unimplemented) <PRI>[<TIME>] <tag> '<num>.<tag>' ":" <message>
// <PRI>[<TIME>] "[INFO]"<tag> : <message>
// <PRI>[<TIME>] "------------[ cut here ]------------"   (?)
// <PRI>[<TIME>] "---[ end trace 3225a3070ca3e4ac ]---"   (?)
// LOG_USER, LOG_MAIL, LOG_DAEMON, LOG_AUTH, LOG_SYSLOG, LOG_LPR, LOG_NEWS
// LOG_UUCP, LOG_CRON, LOG_AUTHPRIV, LOG_FTP:
// <PRI+TAG>[<TIME>] (see sys/syslog.h)
// Observe:
//  Minimum tag length = 3   NB: drops things like r5:c00bbadf, but allow PM:
//  Maximum tag words = 2
//  Maximum tag length = 16  NB: we are thinking of how ugly logcat can get.
//  Not a Tag if there is no message content.
//  leading additional spaces means no tag, inherit last tag.
//  Not a Tag if <tag>: is "ERROR:", "WARNING:", "INFO:" or "CPU:"
// Drop:
//  empty messages
//  messages with ' audit(' in them if auditd is running
//  logd.klogd:
// return -1 if message logd.klogd: <signature>
//
int LogKlog::log(const char* buf, ssize_t len) {
    if (auditd && android::strnstr(buf, len, auditStr)) {
        return 0;
    }

    const char* p = buf;
    int pri = parseKernelPrio(p, len);

    log_time now;
    sniffTime(now, p, len - (p - buf), false);

    // sniff for start marker
    const char* start = android::strnstr(p, len - (p - buf), klogdStr);
    if (start) {
        uint64_t sig = strtoll(start + strlen(klogdStr), nullptr, 10);
        if (sig == signature.nsec()) {
            if (initialized) {
                enableLogging = true;
            } else {
                enableLogging = false;
            }
            return -1;
        }
        return 0;
    }

    if (!enableLogging) {
        return 0;
    }

    // Parse pid, tid and uid
    const pid_t pid = sniffPid(p, len - (p - buf));
    const pid_t tid = pid;
    uid_t uid = AID_ROOT;
    if (pid) {
        logbuf->wrlock();
        uid = logbuf->pidToUid(pid);
        logbuf->unlock();
    }

    // Parse (rules at top) to pull out a tag from the incoming kernel message.
    // Some may view the following as an ugly heuristic, the desire is to
    // beautify the kernel logs into an Android Logging format; the goal is
    // admirable but costly.
    while ((p < &buf[len]) && (isspace(*p) || !*p)) {
        ++p;
    }
    if (p >= &buf[len]) {  // timestamp, no content
        return 0;
    }
    start = p;
    const char* tag = "";
    const char* etag = tag;
    ssize_t taglen = len - (p - buf);
    const char* bt = p;

    static const char infoBrace[] = "[INFO]";
    static const ssize_t infoBraceLen = strlen(infoBrace);
    if ((taglen >= infoBraceLen) &&
        !fastcmp<strncmp>(p, infoBrace, infoBraceLen)) {
        // <PRI>[<TIME>] "[INFO]"<tag> ":" message
        bt = p + infoBraceLen;
        taglen -= infoBraceLen;
    }

    const char* et;
    for (et = bt; (taglen > 0) && *et && (*et != ':') && !isspace(*et);
         ++et, --taglen) {
        // skip ':' within [ ... ]
        if (*et == '[') {
            while ((taglen > 0) && *et && *et != ']') {
                ++et;
                --taglen;
            }
            if (taglen <= 0) {
                break;
            }
        }
    }
    const char* cp;
    for (cp = et; (taglen > 0) && isspace(*cp); ++cp, --taglen) {
    }

    // Validate tag
    ssize_t size = et - bt;
    if ((taglen > 0) && (size > 0)) {
        if (*cp == ':') {
            // ToDo: handle case insensitive colon separated logging stutter:
            //       <tag> : <tag>: ...

            // One Word
            tag = bt;
            etag = et;
            p = cp + 1;
        } else if ((taglen > size) && (tolower(*bt) == tolower(*cp))) {
            // clean up any tag stutter
            if (!fastcmp<strncasecmp>(bt + 1, cp + 1, size - 1)) {  // no match
                // <PRI>[<TIME>] <tag> <tag> : message
                // <PRI>[<TIME>] <tag> <tag>: message
                // <PRI>[<TIME>] <tag> '<tag>.<num>' : message
                // <PRI>[<TIME>] <tag> '<tag><num>' : message
                // <PRI>[<TIME>] <tag> '<tag><stuff>' : message
                const char* b = cp;
                cp += size;
                taglen -= size;
                while ((--taglen > 0) && !isspace(*++cp) && (*cp != ':')) {
                }
                const char* e;
                for (e = cp; (taglen > 0) && isspace(*cp); ++cp, --taglen) {
                }
                if ((taglen > 0) && (*cp == ':')) {
                    tag = b;
                    etag = e;
                    p = cp + 1;
                }
            } else {
                // what about <PRI>[<TIME>] <tag>_host '<tag><stuff>' : message
                static const char host[] = "_host";
                static const ssize_t hostlen = strlen(host);
                if ((size > hostlen) &&
                    !fastcmp<strncmp>(bt + size - hostlen, host, hostlen) &&
                    !fastcmp<strncmp>(bt + 1, cp + 1, size - hostlen - 1)) {
                    const char* b = cp;
                    cp += size - hostlen;
                    taglen -= size - hostlen;
                    if (*cp == '.') {
                        while ((--taglen > 0) && !isspace(*++cp) &&
                               (*cp != ':')) {
                        }
                        const char* e;
                        for (e = cp; (taglen > 0) && isspace(*cp);
                             ++cp, --taglen) {
                        }
                        if ((taglen > 0) && (*cp == ':')) {
                            tag = b;
                            etag = e;
                            p = cp + 1;
                        }
                    }
                } else {
                    goto twoWord;
                }
            }
        } else {
        // <PRI>[<TIME>] <tag> <stuff>' : message
        twoWord:
            while ((--taglen > 0) && !isspace(*++cp) && (*cp != ':')) {
            }
            const char* e;
            for (e = cp; (taglen > 0) && isspace(*cp); ++cp, --taglen) {
            }
            // Two words
            if ((taglen > 0) && (*cp == ':')) {
                tag = bt;
                etag = e;
                p = cp + 1;
            }
        }
    }  // else no tag

    static const char cpu[] = "CPU";
    static const ssize_t cpuLen = strlen(cpu);
    static const char warning[] = "WARNING";
    static const ssize_t warningLen = strlen(warning);
    static const char error[] = "ERROR";
    static const ssize_t errorLen = strlen(error);
    static const char info[] = "INFO";
    static const ssize_t infoLen = strlen(info);

    size = etag - tag;
    if ((size <= 1) ||
        // register names like x9
        ((size == 2) && (isdigit(tag[0]) || isdigit(tag[1]))) ||
        // register names like x18 but not driver names like en0
        ((size == 3) && (isdigit(tag[1]) && isdigit(tag[2]))) ||
        // blacklist
        ((size == cpuLen) && !fastcmp<strncmp>(tag, cpu, cpuLen)) ||
        ((size == warningLen) &&
         !fastcmp<strncasecmp>(tag, warning, warningLen)) ||
        ((size == errorLen) && !fastcmp<strncasecmp>(tag, error, errorLen)) ||
        ((size == infoLen) && !fastcmp<strncasecmp>(tag, info, infoLen))) {
        p = start;
        etag = tag = "";
    }

    // Suppress additional stutter in tag:
    //   eg: [143:healthd]healthd -> [143:healthd]
    taglen = etag - tag;
    // Mediatek-special printk induced stutter
    const char* mp = strnrchr(tag, taglen, ']');
    if (mp && (++mp < etag)) {
        ssize_t s = etag - mp;
        if (((s + s) < taglen) && !fastcmp<memcmp>(mp, mp - 1 - s, s)) {
            taglen = mp - tag;
        }
    }
    // Deal with sloppy and simplistic harmless p = cp + 1 etc above.
    if (len < (p - buf)) {
        p = &buf[len];
    }
    // skip leading space
    while ((p < &buf[len]) && (isspace(*p) || !*p)) {
        ++p;
    }
    // truncate trailing space or nuls
    ssize_t b = len - (p - buf);
    while ((b > 0) && (isspace(p[b - 1]) || !p[b - 1])) {
        --b;
    }
    // trick ... allow tag with empty content to be logged. log() drops empty
    if ((b <= 0) && (taglen > 0)) {
        p = " ";
        b = 1;
    }
    // paranoid sanity check, can not happen ...
    if (b > LOGGER_ENTRY_MAX_PAYLOAD) {
        b = LOGGER_ENTRY_MAX_PAYLOAD;
    }
    if (taglen > LOGGER_ENTRY_MAX_PAYLOAD) {
        taglen = LOGGER_ENTRY_MAX_PAYLOAD;
    }
    // calculate buffer copy requirements
    ssize_t n = 1 + taglen + 1 + b + 1;
    // paranoid sanity check, first two just can not happen ...
    if ((taglen > n) || (b > n) || (n > (ssize_t)USHRT_MAX) || (n <= 0)) {
        return -EINVAL;
    }

    // Careful.
    // We are using the stack to house the log buffer for speed reasons.
    // If we malloc'd this buffer, we could get away without n's USHRT_MAX
    // test above, but we would then required a max(n, USHRT_MAX) as
    // truncating length argument to logbuf->log() below. Gain is protection
    // of stack sanity and speedup, loss is truncated long-line content.
    char newstr[n];
    char* np = newstr;

    // Convert priority into single-byte Android logger priority
    *np = convertKernelPrioToAndroidPrio(pri);
    ++np;

    // Copy parsed tag following priority
    memcpy(np, tag, taglen);
    np += taglen;
    *np = '\0';
    ++np;

    // Copy main message to the remainder
    memcpy(np, p, b);
    np[b] = '\0';

    if (!isMonotonic()) {
        // Watch out for singular race conditions with timezone causing near
        // integer quarter-hour jumps in the time and compensate accordingly.
        // Entries will be temporal within near_seconds * 2. b/21868540
        static uint32_t vote_time[3];
        vote_time[2] = vote_time[1];
        vote_time[1] = vote_time[0];
        vote_time[0] = now.tv_sec;

        if (vote_time[1] && vote_time[2]) {
            static const unsigned near_seconds = 10;
            static const unsigned timezones_seconds = 900;
            int diff0 = (vote_time[0] - vote_time[1]) / near_seconds;
            unsigned abs0 = (diff0 < 0) ? -diff0 : diff0;
            int diff1 = (vote_time[1] - vote_time[2]) / near_seconds;
            unsigned abs1 = (diff1 < 0) ? -diff1 : diff1;
            if ((abs1 <= 1) &&  // last two were in agreement on timezone
                ((abs0 + 1) % (timezones_seconds / near_seconds)) <= 2) {
                abs0 = (abs0 + 1) / (timezones_seconds / near_seconds) *
                       timezones_seconds;
                now.tv_sec -= (diff0 < 0) ? -abs0 : abs0;
            }
        }
    }

    // Log message
    int rc = logbuf->log(LOG_ID_KERNEL, now, uid, pid, tid, newstr,
                         (unsigned short)n);

    // notify readers
    if (!rc) {
        reader->notifyNewLog();
    }

    return rc;
}