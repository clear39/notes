//  @   system/core/init/uevent_listener.cpp
void UeventListener::RegenerateUevents(const ListenerCallback& callback) const {
    //  static const char* kRegenerationPaths[] = {"/sys/class", "/sys/block", "/sys/devices"};
    for (const auto path : kRegenerationPaths) {
        if (RegenerateUeventsForPath(path, callback) == ListenerAction::kStop) return;
    }
}


ListenerAction UeventListener::RegenerateUeventsForPath(const std::string& path,const ListenerCallback& callback) const {
    std::unique_ptr<DIR, decltype(&closedir)> d(opendir(path.c_str()), closedir);
    if (!d) return ListenerAction::kContinue;

    return RegenerateUeventsForDir(d.get(), callback);
}


// RegenerateUevents*() walks parts of the /sys tree and pokes the uevent files to cause the kernel
// to regenerate device add uevents that have already happened.  This is particularly useful when
// starting ueventd, to regenerate all of the uevents that it had previously missed.
//
// We drain any pending events from the netlink socket every time we poke another uevent file to
// make sure we don't overrun the socket's buffer.
//

ListenerAction UeventListener::RegenerateUeventsForDir(DIR* d,const ListenerCallback& callback) const {
    int dfd = dirfd(d);

    int fd = openat(dfd, "uevent", O_WRONLY);
    if (fd >= 0) {
        write(fd, "add\n", 4);
        close(fd);

        Uevent uevent;
        while (ReadUevent(&uevent)) {
            if (callback(uevent) == ListenerAction::kStop) return ListenerAction::kStop;
        }
    }

    dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (de->d_type != DT_DIR || de->d_name[0] == '.') continue;

        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
        if (fd < 0) continue;

        std::unique_ptr<DIR, decltype(&closedir)> d2(fdopendir(fd), closedir);
        if (d2 == 0) {
            close(fd);
        } else {
            if (RegenerateUeventsForDir(d2.get(), callback) == ListenerAction::kStop) {
                return ListenerAction::kStop;
            }
        }
    }

    // default is always to continue looking for uevents
    return ListenerAction::kContinue;
}


bool UeventListener::ReadUevent(Uevent* uevent) const {
    char msg[UEVENT_MSG_LEN + 2];
    int n = uevent_kernel_multicast_recv(device_fd_, msg, UEVENT_MSG_LEN);
    if (n <= 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG(ERROR) << "Error reading from Uevent Fd";
        }
        return false;
    }
    if (n >= UEVENT_MSG_LEN) {
        LOG(ERROR) << "Uevent overflowed buffer, discarding";
        // Return true here even if we discard as we may have more uevents pending and we
        // want to keep processing them.
        return true;
    }

    msg[n] = '\0';
    msg[n + 1] = '\0';

    ParseEvent(msg, uevent);

    return true;
}


static void ParseEvent(const char* msg, Uevent* uevent) {
    uevent->partition_num = -1;
    uevent->major = -1;
    uevent->minor = -1;
    uevent->action.clear();
    uevent->path.clear();
    uevent->subsystem.clear();
    uevent->firmware.clear();
    uevent->partition_name.clear();
    uevent->device_name.clear();
    // currently ignoring SEQNUM
    while (*msg) {
        if (!strncmp(msg, "ACTION=", 7)) {
            msg += 7;
            uevent->action = msg;
        } else if (!strncmp(msg, "DEVPATH=", 8)) {
            msg += 8;
            uevent->path = msg;
        } else if (!strncmp(msg, "SUBSYSTEM=", 10)) {
            msg += 10;
            uevent->subsystem = msg;
        } else if (!strncmp(msg, "FIRMWARE=", 9)) {
            msg += 9;
            uevent->firmware = msg;
        } else if (!strncmp(msg, "MAJOR=", 6)) {
            msg += 6;
            uevent->major = atoi(msg);
        } else if (!strncmp(msg, "MINOR=", 6)) {
            msg += 6;
            uevent->minor = atoi(msg);
        } else if (!strncmp(msg, "PARTN=", 6)) {
            msg += 6;
            uevent->partition_num = atoi(msg);
        } else if (!strncmp(msg, "PARTNAME=", 9)) {
            msg += 9;
            uevent->partition_name = msg;
        } else if (!strncmp(msg, "DEVNAME=", 8)) {
            msg += 8;
            uevent->device_name = msg;
        }

        // advance to after the next \0
        while (*msg++)
            ;
    }

    if (LOG_UEVENTS) {
        LOG(INFO) << "event { '" << uevent->action << "', '" << uevent->path << "', '"
                  << uevent->subsystem << "', '" << uevent->firmware << "', " << uevent->major
                  << ", " << uevent->minor << " }";
    }
}