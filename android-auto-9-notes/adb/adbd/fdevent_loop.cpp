/**
 * 
*/
void fdevent_install(fdevent* fde, int fd, fd_func func, void* arg) {
    check_main_thread();
    CHECK_GE(fd, 0);
    memset(fde, 0, sizeof(fdevent));
    fde->state = FDE_ACTIVE;
    fde->fd = fd;
    fde->func = func;
    fde->arg = arg;
    if (!set_file_block_mode(fd, false)) {
        // Here is not proper to handle the error. If it fails here, some error is
        // likely to be detected by poll(), then we can let the callback function
        // to handle it.
        LOG(ERROR) << "failed to set non-blocking mode for fd " << fd;
    }
    auto pair = g_poll_node_map.emplace(fde->fd, PollNode(fde));
    CHECK(pair.second) << "install existing fd " << fd;
    D("fdevent_install %s", dump_fde(fde).c_str());
}

void fdevent_set(fdevent* fde, unsigned events) {
    check_main_thread();
    events &= FDE_EVENTMASK;
    if ((fde->state & FDE_EVENTMASK) == events) {
        return;
    }
    CHECK(fde->state & FDE_ACTIVE);
    fdevent_update(fde, events);
    D("fdevent_set: %s, events = %u", dump_fde(fde).c_str(), events);

    if (fde->state & FDE_PENDING) {
        // If we are pending, make sure we don't signal an event that is no longer wanted.
        fde->events &= events;
        if (fde->events == 0) {
            g_pending_list.remove(fde);
            fde->state &= ~FDE_PENDING;
        }
    }
}

void check_main_thread() {
    if (main_thread_valid) {
        CHECK_EQ(main_thread_id, adb_thread_id());
    }
}

static void fdevent_update(fdevent* fde, unsigned events) {
    auto it = g_poll_node_map.find(fde->fd);
    CHECK(it != g_poll_node_map.end());
    PollNode& node = it->second;
    if (events & FDE_READ) {
        node.pollfd.events |= POLLIN;
    } else {
        node.pollfd.events &= ~POLLIN;
    }

    if (events & FDE_WRITE) {
        node.pollfd.events |= POLLOUT;
    } else {
        node.pollfd.events &= ~POLLOUT;
    }
    fde->state = (fde->state & FDE_STATEMASK) | events;
}


static std::string dump_fde(const fdevent* fde) {
    std::string state;
    if (fde->state & FDE_ACTIVE) {
        state += "A";
    }
    if (fde->state & FDE_PENDING) {
        state += "P";
    }
    if (fde->state & FDE_CREATED) {
        state += "C";
    }
    if (fde->state & FDE_READ) {
        state += "R";
    }
    if (fde->state & FDE_WRITE) {
        state += "W";
    }
    if (fde->state & FDE_ERROR) {
        state += "E";
    }
    if (fde->state & FDE_DONT_CLOSE) {
        state += "D";
    }
    return android::base::StringPrintf("(fdevent %d %s)", fde->fd, state.c_str());
}







/**
 * 
 * int adbd_main(int server_port) 
 * ---> 
*/
void fdevent_loop() {
    set_main_thread();
    fdevent_run_setup();

    while (true) {
        if (terminate_loop) {
            return;
        }

        D("--- --- waiting for events");

        fdevent_process();

        while (!g_pending_list.empty()) {
            fdevent* fde = g_pending_list.front();
            g_pending_list.pop_front();
            fdevent_call_fdfunc(fde);
        }
    }
}

void set_main_thread() {
    main_thread_valid = true;
    main_thread_id = adb_thread_id();
}

static void fdevent_run_setup() {
    {
        std::lock_guard<std::mutex> lock(run_queue_mutex);
        CHECK(run_queue_notify_fd.get() == -1);
        int s[2];
        if (adb_socketpair(s) != 0) {
            PLOG(FATAL) << "failed to create run queue notify socketpair";
        }

        if (!set_file_block_mode(s[0], false) || !set_file_block_mode(s[1], false)) {
            PLOG(FATAL) << "failed to make run queue notify socket nonblocking";
        }

        run_queue_notify_fd.reset(s[0]);
        fdevent* fde = fdevent_create(s[1], fdevent_run_func, nullptr);
        CHECK(fde != nullptr);
        fdevent_add(fde, FDE_READ);
    }

    fdevent_run_flush();
}

static void fdevent_run_func(int fd, unsigned ev, void* /* userdata */) {
    CHECK_GE(fd, 0);
    CHECK(ev & FDE_READ);

    char buf[1024];

    // Empty the fd.
    if (adb_read(fd, buf, sizeof(buf)) == -1) {
        PLOG(FATAL) << "failed to empty run queue notify fd";
    }

    fdevent_run_flush();
}



static void fdevent_process() {
    std::vector<adb_pollfd> pollfds;
    for (const auto& pair : g_poll_node_map) {
        pollfds.push_back(pair.second.pollfd);
    }
    CHECK_GT(pollfds.size(), 0u);
    D("poll(), pollfds = %s", dump_pollfds(pollfds).c_str());
    int ret = adb_poll(&pollfds[0], pollfds.size(), -1);
    if (ret == -1) {
        PLOG(ERROR) << "poll(), ret = " << ret;
        return;
    }
    for (const auto& pollfd : pollfds) {
        if (pollfd.revents != 0) {
            D("for fd %d, revents = %x", pollfd.fd, pollfd.revents);
        }
        unsigned events = 0;
        if (pollfd.revents & POLLIN) {
            events |= FDE_READ;
        }
        if (pollfd.revents & POLLOUT) {
            events |= FDE_WRITE;
        }
        if (pollfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            // We fake a read, as the rest of the code assumes that errors will
            // be detected at that point.
            events |= FDE_READ | FDE_ERROR;
        }
#if defined(__linux__)
        if (pollfd.revents & POLLRDHUP) {
            events |= FDE_READ | FDE_ERROR;
        }
#endif
        if (events != 0) {
            auto it = g_poll_node_map.find(pollfd.fd);
            CHECK(it != g_poll_node_map.end());
            fdevent* fde = it->second.fde;
            CHECK_EQ(fde->fd, pollfd.fd);
            fde->events |= events;
            D("%s got events %x", dump_fde(fde).c_str(), events);
            fde->state |= FDE_PENDING;
            g_pending_list.push_back(fde);
        }
    }
}


static void fdevent_call_fdfunc(fdevent* fde) {
    unsigned events = fde->events;
    fde->events = 0;
    CHECK(fde->state & FDE_PENDING);
    fde->state &= (~FDE_PENDING);
    D("fdevent_call_fdfunc %s", dump_fde(fde).c_str());
    fde->func(fde->fd, events, fde->arg);
}




