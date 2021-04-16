//  @ system/core/lmkd/lmkd.c
/**
 * 
 * [init.svc.lmkd]: [running]
 * [ro.lmk.use_minfree_levels]: [true]

*/
int main(int argc __unused, char **argv __unused) {
    struct sched_param param = {
            .sched_priority = 1,
    };

    /* By default disable low level vmpressure events */
    level_oomadj[VMPRESS_LEVEL_LOW] = property_get_int32("ro.lmk.low", OOM_SCORE_ADJ_MAX + 1);
    level_oomadj[VMPRESS_LEVEL_MEDIUM] = property_get_int32("ro.lmk.medium", 800);
    level_oomadj[VMPRESS_LEVEL_CRITICAL] = property_get_int32("ro.lmk.critical", 0);
    debug_process_killing = property_get_bool("ro.lmk.debug", false);

    /* By default disable upgrade/downgrade logic */
    enable_pressure_upgrade = property_get_bool("ro.lmk.critical_upgrade", false);
    upgrade_pressure = (int64_t)property_get_int32("ro.lmk.upgrade_pressure", 100);
    downgrade_pressure = (int64_t)property_get_int32("ro.lmk.downgrade_pressure", 100);
    kill_heaviest_task = property_get_bool("ro.lmk.kill_heaviest_task", false);
    low_ram_device = property_get_bool("ro.config.low_ram", false);
    kill_timeout_ms = (unsigned long)property_get_int32("ro.lmk.kill_timeout_ms", 0);
    use_minfree_levels = property_get_bool("ro.lmk.use_minfree_levels", false);   //true

#ifdef LMKD_LOG_STATS
    statslog_init(&log_ctx, &enable_stats_log);
#endif

    // MCL_ONFAULT pins pages as they fault instead of loading
    // everything immediately all at once. (Which would be bad,
    // because as of this writing, we have a lot of mapped pages we
    // never use.) Old kernels will see MCL_ONFAULT and fail with
    // EINVAL; we ignore this failure.
    //
    // N.B. read the man page for mlockall. MCL_CURRENT | MCL_ONFAULT
    // pins ⊆ MCL_CURRENT, converging to just MCL_CURRENT as we fault
    // in pages.


    /*
       MCL_CURRENT Lock all pages which are currently mapped into the address space of the process.

       MCL_FUTURE  Lock all pages which will become mapped into the address space of the process in the future.  
                   These could be, for instance, new pages required by a growing heap and stack as well as
                   new memory-mapped files or shared memory regions.

       MCL_ONFAULT (since Linux 4.4)
                   Used together with MCL_CURRENT, MCL_FUTURE, or both.  Mark all current (with MCL_CURRENT) or future (with MCL_FUTURE) 
                   mappings to lock pages when they are  faulted  in.   When  used
                   with MCL_CURRENT, all present pages are locked, but mlockall() will not fault in non-present pages.  
                   When used with MCL_FUTURE, all future mappings will be marked to lock pages when
                   they are faulted in, but they will not be populated by the lock when the mapping is created.  
                   MCL_ONFAULT must be used with either MCL_CURRENT or MCL_FUTURE or both.

    */
    // 锁住该实时进程在物理内存上全部地址空间。这将阻止Linux将这个内存页调度到交换空间(swap space)，即使该进程已有一段时间没有访问这段空间。参见末尾参考资料。
    if (mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) && errno != EINVAL)
        ALOGW("mlockall failed: errno=%d", errno);


    

    sched_setscheduler(0, SCHED_FIFO, &param);

    if (!init())
        mainloop();

#ifdef LMKD_LOG_STATS
    statslog_destroy(&log_ctx);
#endif

    ALOGI("exiting");
    return 0;
}




static int init(void) {
    struct epoll_event epev;
    int i;
    int ret;

    page_k = sysconf(_SC_PAGESIZE);
    if (page_k == -1)
        page_k = PAGE_SIZE;
    page_k /= 1024;

    epollfd = epoll_create(MAX_EPOLL_EVENTS);
    if (epollfd == -1) {
        ALOGE("epoll_create failed (errno=%d)", errno);
        return -1;
    }

    // mark data connections as not connected
    for (int i = 0; i < MAX_DATA_CONN; i++) {
        data_sock[i].sock = -1;
    }

    ctrl_sock.sock = android_get_control_socket("lmkd");
    if (ctrl_sock.sock < 0) {
        ALOGE("get lmkd control socket failed");
        return -1;
    }

    ret = listen(ctrl_sock.sock, MAX_DATA_CONN);
    if (ret < 0) {
        ALOGE("lmkd control socket listen failed (errno=%d)", errno);
        return -1;
    }

    epev.events = EPOLLIN;
    ctrl_sock.handler_info.handler = ctrl_connect_handler;
    epev.data.ptr = (void *)&(ctrl_sock.handler_info);
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctrl_sock.sock, &epev) == -1) {
        ALOGE("epoll_ctl for lmkd control socket failed (errno=%d)", errno);
        return -1;
    }
    maxevents++;

    //  #define INKERNEL_MINFREE_PATH "/sys/module/lowmemorykiller/parameters/minfree"
    has_inkernel_module = !access(INKERNEL_MINFREE_PATH, W_OK);  //不存在
    use_inkernel_interface = has_inkernel_module; // false

    if (use_inkernel_interface) {
        ALOGI("Using in-kernel low memory killer interface");
    } else {
        //执行这里
        if (!init_mp_common(VMPRESS_LEVEL_LOW) ||
            !init_mp_common(VMPRESS_LEVEL_MEDIUM) ||
            !init_mp_common(VMPRESS_LEVEL_CRITICAL)) {
            ALOGE("Kernel does not support memory pressure events or in-kernel low memory killer");
            return -1;
        }
    }

    /*
    #define OOM_SCORE_ADJ_MIN       (-1000)
    #define OOM_SCORE_ADJ_MAX       1000
    #define ADJTOSLOT(adj) ((adj) + -OOM_SCORE_ADJ_MIN)
    */

    //  ADJTOSLOT(OOM_SCORE_ADJ_MAX) = 2000

    //  static struct adjslot_list procadjslot_list[ADJTOSLOT(OOM_SCORE_ADJ_MAX) + 1];
    for (i = 0; i <= ADJTOSLOT(OOM_SCORE_ADJ_MAX); i++) {
        procadjslot_list[i].next = &procadjslot_list[i];
        procadjslot_list[i].prev = &procadjslot_list[i];
    }

    return 0;
}


/**
 // memory pressure levels 
enum vmpressure_level {
    VMPRESS_LEVEL_LOW = 0,
    VMPRESS_LEVEL_MEDIUM,
    VMPRESS_LEVEL_CRITICAL,
    VMPRESS_LEVEL_COUNT
};


static const char *level_name[] = {
    "low",
    "medium",
    "critical"
};

*/

static bool init_mp_common(enum vmpressure_level level) {
    int mpfd;
    int evfd;
    int evctlfd;
    char buf[256];
    struct epoll_event epev;
    int ret;
    int level_idx = (int)level;
    const char *levelstr = level_name[level_idx];

    //  #define MEMCG_SYSFS_PATH "/dev/memcg/"
    //  "/dev/memcg/memory.pressure_level"
    mpfd = open(MEMCG_SYSFS_PATH "memory.pressure_level", O_RDONLY | O_CLOEXEC);  //存在
    if (mpfd < 0) {
        ALOGI("No kernel memory.pressure_level support (errno=%d)", errno);
        goto err_open_mpfd;
    }

    //  "/dev/memcg/cgroup.event_control"
    evctlfd = open(MEMCG_SYSFS_PATH "cgroup.event_control", O_WRONLY | O_CLOEXEC);
    if (evctlfd < 0) {
        ALOGI("No kernel memory cgroup event control (errno=%d)", errno);
        goto err_open_evctlfd;
    }

    evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);  //创建一个事件描述符
    if (evfd < 0) {
        ALOGE("eventfd failed for level %s; errno=%d", levelstr, errno);
        goto err_eventfd;
    }

    // 
    ret = snprintf(buf, sizeof(buf), "%d %d %s", evfd, mpfd, levelstr);
    if (ret >= (ssize_t)sizeof(buf)) {
        ALOGE("cgroup.event_control line overflow for level %s", levelstr);
        goto err;
    }

    // 
    ret = TEMP_FAILURE_RETRY(write(evctlfd, buf, strlen(buf) + 1));
    if (ret == -1) {
        ALOGE("cgroup.event_control write failed for level %s; errno=%d",
              levelstr, errno);
        goto err;
    }

    epev.events = EPOLLIN;
    /* use data to store event level */
    vmpressure_hinfo[level_idx].data = level_idx;
    vmpressure_hinfo[level_idx].handler = mp_event_common;
    epev.data.ptr = (void *)&vmpressure_hinfo[level_idx];
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, evfd, &epev);
    if (ret == -1) {
        ALOGE("epoll_ctl for level %s failed; errno=%d", levelstr, errno);
        goto err;
    }
    maxevents++;
    mpevfd[level] = evfd;
    close(evctlfd);

    return true;

err:
    close(evfd);
err_eventfd:
    close(evctlfd);
err_open_evctlfd:
    close(mpfd);
err_open_mpfd:
    return false;
}


static void mp_event_common(int data, uint32_t events __unused) {
    int ret;
    unsigned long long evcount;
    int64_t mem_usage, memsw_usage;
    int64_t mem_pressure;
    enum vmpressure_level lvl;
    union meminfo mi;
    union zoneinfo zi;
    static struct timeval last_report_tm;
    static unsigned long skip_count = 0;
    enum vmpressure_level level = (enum vmpressure_level)data;
    long other_free = 0, other_file = 0;
    int min_score_adj;
    int pages_to_free = 0;
    int minfree = 0;
    static struct reread_data mem_usage_file_data = {
        .filename = MEMCG_MEMORY_USAGE,     //  #define MEMCG_MEMORY_USAGE "/dev/memcg/memory.usage_in_bytes"
        .fd = -1,
    };
    static struct reread_data memsw_usage_file_data = {
        .filename = MEMCG_MEMORYSW_USAGE,       //  #define MEMCG_MEMORYSW_USAGE "/dev/memcg/memory.memsw.usage_in_bytes"
        .fd = -1,
    };

    /*
     * Check all event counters from low to critical
     * and upgrade to the highest priority one. By reading
     * eventfd we also reset the event counters.
     */
    for (lvl = VMPRESS_LEVEL_LOW; lvl < VMPRESS_LEVEL_COUNT; lvl++) {
        if (mpevfd[lvl] != -1 
            && TEMP_FAILURE_RETRY(read(mpevfd[lvl],&evcount, sizeof(evcount))) > 0 
            && evcount > 0 
            && lvl > level) 
        {
            level = lvl;
        }
    }

    if (kill_timeout_ms) {  // 默认为 0
        struct timeval curr_tm;
        gettimeofday(&curr_tm, NULL);
        if (get_time_diff_ms(&last_report_tm, &curr_tm) < kill_timeout_ms) {
            skip_count++;
            return;
        }
    }

    if (skip_count > 0) {  //默认为 0
        ALOGI("%lu memory pressure events were skipped after a kill!",skip_count);
        skip_count = 0;
    }

    if (meminfo_parse(&mi) < 0 || zoneinfo_parse(&zi) < 0) {
        ALOGE("Failed to get free memory!");
        return;
    }

    if (use_minfree_levels) { // 系统属性配置为true
        int i;

        other_free = mi.field.nr_free_pages - zi.field.totalreserve_pages;
        if (mi.field.nr_file_pages > (mi.field.shmem + mi.field.unevictable + mi.field.swap_cached)) {
            other_file = (mi.field.nr_file_pages - mi.field.shmem - mi.field.unevictable - mi.field.swap_cached);
        } else {
            other_file = 0;
        }

        min_score_adj = OOM_SCORE_ADJ_MAX + 1;
        for (i = 0; i < lowmem_targets_size; i++) {
            minfree = lowmem_minfree[i];
            if (other_free < minfree && other_file < minfree) {
                min_score_adj = lowmem_adj[i];
                break;
            }
        }

        if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
            if (debug_process_killing) {
                ALOGI("Ignore %s memory pressure event "
                      "(free memory=%ldkB, cache=%ldkB, limit=%ldkB)",
                      level_name[level], other_free * page_k, other_file * page_k,
                      (long)lowmem_minfree[lowmem_targets_size - 1] * page_k);
            }
            return;
        }

        /* Free up enough pages to push over the highest minfree level */
        pages_to_free = lowmem_minfree[lowmem_targets_size - 1] - ((other_free < other_file) ? other_free : other_file);
        goto do_kill;
    }

    if (level == VMPRESS_LEVEL_LOW) {
        record_low_pressure_levels(&mi);
    }

    if (level_oomadj[level] > OOM_SCORE_ADJ_MAX) {
        /* Do not monitor this pressure level */
        return;
    }

    if ((mem_usage = get_memory_usage(&mem_usage_file_data)) < 0) {
        goto do_kill;
    }
    if ((memsw_usage = get_memory_usage(&memsw_usage_file_data)) < 0) {
        goto do_kill;
    }

    // Calculate percent for swappinness.
    mem_pressure = (mem_usage * 100) / memsw_usage;

    if (enable_pressure_upgrade && level != VMPRESS_LEVEL_CRITICAL) {
        // We are swapping too much.
        if (mem_pressure < upgrade_pressure) {
            level = upgrade_level(level);
            if (debug_process_killing) {
                ALOGI("Event upgraded to %s", level_name[level]);
            }
        }
    }

    // If the pressure is larger than downgrade_pressure lmk will not
    // kill any process, since enough memory is available.
    if (mem_pressure > downgrade_pressure) {
        if (debug_process_killing) {
            ALOGI("Ignore %s memory pressure", level_name[level]);
        }
        return;
    } else if (level == VMPRESS_LEVEL_CRITICAL && mem_pressure > upgrade_pressure) {
        if (debug_process_killing) {
            ALOGI("Downgrade critical memory pressure");
        }
        // Downgrade event, since enough memory available.
        level = downgrade_level(level);
    }

do_kill:
    if (low_ram_device) {
        /* For Go devices kill only one task */
        if (find_and_kill_processes(level, level_oomadj[level], 0) == 0) {
            if (debug_process_killing) {
                ALOGI("Nothing to kill");
            }
        }
    } else {
        int pages_freed;

        if (!use_minfree_levels) {
            /* If pressure level is less than critical and enough free swap then ignore */
            if (level < VMPRESS_LEVEL_CRITICAL &&
                mi.field.free_swap > low_pressure_mem.max_nr_free_pages) {
                if (debug_process_killing) {
                    ALOGI("Ignoring pressure since %" PRId64 " swap pages are available ", mi.field.free_swap);
                }
                return;
            }
            /* Free up enough memory to downgrate the memory pressure to low level */
            if (mi.field.nr_free_pages < low_pressure_mem.max_nr_free_pages) {
                pages_to_free = low_pressure_mem.max_nr_free_pages -
                    mi.field.nr_free_pages;
            } else {
                if (debug_process_killing) {
                    ALOGI("Ignoring pressure since more memory is "
                        "available (%" PRId64 ") than watermark (%" PRId64 ")",
                        mi.field.nr_free_pages, low_pressure_mem.max_nr_free_pages);
                }
                return;
            }
            min_score_adj = level_oomadj[level];
        }

        pages_freed = find_and_kill_processes(level, min_score_adj, pages_to_free);

        if (use_minfree_levels) {
            ALOGI("Killing because cache %ldkB is below "
                  "limit %ldkB for oom_adj %d\n"
                  "   Free memory is %ldkB %s reserved",
                  other_file * page_k, minfree * page_k, min_score_adj,
                  other_free * page_k, other_free >= 0 ? "above" : "below");
        }

        if (pages_freed < pages_to_free) {
            ALOGI("Unable to free enough memory (pages to free=%d, pages freed=%d)", pages_to_free, pages_freed);
        } else {
            ALOGI("Reclaimed enough memory (pages to free=%d, pages freed=%d)", pages_to_free, pages_freed);
            gettimeofday(&last_report_tm, NULL);
        }
    }
}


static int64_t get_memory_usage(struct reread_data *file_data) {
    int ret;
    int64_t mem_usage;
    char buf[32];

    if (reread_file(file_data, buf, sizeof(buf)) < 0) {
        return -1;
    }

    if (!parse_int64(buf, &mem_usage)) {
        ALOGE("%s parse error", file_data->filename);
        return -1;
    }
    if (mem_usage == 0) {
        ALOGE("No memory!");
        return -1;
    }
    return mem_usage;
}




static void mainloop(void) {
    struct event_handler_info* handler_info;
    struct epoll_event *evt;

    while (1) {
        struct epoll_event events[maxevents];
        int nevents;
        int i;

        nevents = epoll_wait(epollfd, events, maxevents, -1);

        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("epoll_wait failed (errno=%d)", errno);
            continue;
        }

        /*
         * First pass to see if any data socket connections were dropped.
         * Dropped connection should be handled before any other events
         * to deallocate data connection and correctly handle cases when
         * connection gets dropped and reestablished in the same epoll cycle.
         * In such cases it's essential to handle connection closures first.
         */
        for (i = 0, evt = &events[0]; i < nevents; ++i, evt++) {
            if ((evt->events & EPOLLHUP) && evt->data.ptr) {
                ALOGI("lmkd data connection dropped");

                handler_info = (struct event_handler_info*)evt->data.ptr;
                ctrl_data_close(handler_info->data);
            }
        }

        /* Second pass to handle all other events */
        for (i = 0, evt = &events[0]; i < nevents; ++i, evt++) {
            if (evt->events & EPOLLERR)
                ALOGD("EPOLLERR on event #%d", i);
            if (evt->events & EPOLLHUP) {
                /* This case was handled in the first pass */
                continue;
            }
            if (evt->data.ptr) {  
                handler_info = (struct event_handler_info*)evt->data.ptr; //ctrl_sock.handler_info 
                handler_info->handler(handler_info->data, evt->events);  // ctrl_connect_handler
            }
        }
    }
}





static void ctrl_connect_handler(int data __unused, uint32_t events __unused) {
    struct epoll_event epev;
    int free_dscock_idx = get_free_dsock();

    if (free_dscock_idx < 0) {
        /*
         * Number of data connections exceeded max supported. This should not
         * happen but if it does we drop all existing connections and accept
         * the new one. This prevents inactive connections from monopolizing
         * data socket and if we drop ActivityManager connection it will
         * immediately reconnect.
         */
        for (int i = 0; i < MAX_DATA_CONN; i++) {
            ctrl_data_close(i);
        }
        free_dscock_idx = 0;
    }

    data_sock[free_dscock_idx].sock = accept(ctrl_sock.sock, NULL, NULL);
    if (data_sock[free_dscock_idx].sock < 0) {
        ALOGE("lmkd control socket accept failed; errno=%d", errno);
        return;
    }

    ALOGI("lmkd data connection established");
    /* use data to store data connection idx */
    data_sock[free_dscock_idx].handler_info.data = free_dscock_idx;
    data_sock[free_dscock_idx].handler_info.handler = ctrl_data_handler;
    epev.events = EPOLLIN;
    epev.data.ptr = (void *)&(data_sock[free_dscock_idx].handler_info);
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, data_sock[free_dscock_idx].sock, &epev) == -1) {
        ALOGE("epoll_ctl for data connection socket failed; errno=%d", errno);
        ctrl_data_close(free_dscock_idx);
        return;
    }
    maxevents++;
}




static void ctrl_data_handler(int data, uint32_t events) {
    if (events & EPOLLIN) {
        ctrl_command_handler(data);
    }
}


static void ctrl_command_handler(int dsock_idx) {
    LMKD_CTRL_PACKET packet;
    int len;
    enum lmk_cmd cmd;
    int nargs;
    int targets;

    len = ctrl_data_read(dsock_idx, (char *)packet, CTRL_PACKET_MAX_SIZE);
    if (len <= 0)
        return;

    if (len < (int)sizeof(int)) {
        ALOGE("Wrong control socket read length len=%d", len);
        return;
    }

    cmd = lmkd_pack_get_cmd(packet);
    nargs = len / sizeof(int) - 1;
    if (nargs < 0)
        goto wronglen;

    switch(cmd) {
    case LMK_TARGET:
        targets = nargs / 2;
        if (nargs & 0x1 || targets > (int)ARRAY_SIZE(lowmem_adj))
            goto wronglen;
        cmd_target(targets, packet);
        break;
    case LMK_PROCPRIO:
        if (nargs != 3)
            goto wronglen;
        cmd_procprio(packet);
        break;
    case LMK_PROCREMOVE:
        if (nargs != 1)
            goto wronglen;
        cmd_procremove(packet);
        break;
    default:
        ALOGE("Received unknown command code %d", cmd);
        return;
    }

    return;

wronglen:
    ALOGE("Wrong control socket read length cmd=%d len=%d", cmd, len);
}