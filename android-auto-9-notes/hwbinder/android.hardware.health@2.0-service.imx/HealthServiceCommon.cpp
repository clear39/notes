//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/health/2.0/utils/libhealthservice/HealthServiceCommon.cpp

int health_service_main(const char* instance /*=""*/) {
    gInstanceName = instance;
    if (gInstanceName.empty()) {
        gInstanceName = "default";
    }
    healthd_mode_ops = &healthd_mode_service_2_0_ops;
    LOG(INFO) << LOG_TAG << gInstanceName << ": Hal starting main loop...";
    //  @hardware/interfaces/health/2.0/default/healthd_common.cpp
    return healthd_main();
}


static struct healthd_mode_ops healthd_mode_service_2_0_ops = {
    .init = healthd_mode_service_2_0_init,
    .preparetowait = healthd_mode_service_2_0_preparetowait,
    .heartbeat = healthd_mode_service_2_0_heartbeat,                //空实现
    .battery_update = healthd_mode_service_2_0_battery_update,
};


//  @hardware/interfaces/health/2.0/default/healthd_common.cpp
int healthd_main() {
    int ret;

    klog_set_level(KLOG_LEVEL);

    if (!healthd_mode_ops) {
        KLOG_ERROR("healthd ops not set, exiting\n");
        exit(1);
    }

    ret = healthd_init();
    if (ret) {
        KLOG_ERROR("Initialization failed, exiting\n");
        exit(2);
    }

    healthd_mainloop();
    KLOG_ERROR("Main loop terminated, exiting\n");
    return 3;
}


static int healthd_init() {
    epollfd = epoll_create(MAX_EPOLL_EVENTS);
    if (epollfd == -1) {
        KLOG_ERROR(LOG_TAG, "epoll_create failed; errno=%d\n", errno);
        return -1;
    }
    /***
     * healthd_config 定义在/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/health/2.0/default/healthd_common.cpp
     * 并且被初始化为默认值
     * init ---> 
     */ 
    healthd_mode_ops->init(&healthd_config);

    /**
     * 这里　监听　时间事件　和　uevent事件,最终调用到　healthd_battery_update();
     * 最终调用到　bool BatteryMonitor::update(void)　接口
     */ 
    wakealarm_init();
    uevent_init();

    return 0;
}

//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/health/2.0/utils/libhealthservice/HealthServiceCommon.cpp
void healthd_mode_service_2_0_init(struct healthd_config* config) {
    LOG(INFO) << LOG_TAG << gInstanceName << " Hal is starting up...";

    gBinderFd = setupTransportPolling(); // binder 文件描述符

    if (gBinderFd >= 0) {
        if (healthd_register_event(gBinderFd, binder_event))
            LOG(ERROR) << LOG_TAG << gInstanceName << ": Register for binder events failed";
    }

    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/health/2.0/default/Health.cpp
    /**
     * 构造　一个　Health　
     */
    android::sp<IHealth> service = Health::initInstance(config);
    CHECK_EQ(service->registerAsService(gInstanceName), android::OK)　<< LOG_TAG << gInstanceName << ": Failed to register HAL";

    LOG(INFO) << LOG_TAG << gInstanceName << ": Hal init done";
}

static void binder_event(uint32_t /*epevents*/) {
    if (gBinderFd >= 0) handleTransportPoll(gBinderFd);
}


sp<IHealth> Health::initInstance(struct healthd_config* c) {
    if (instance_ == nullptr) {
        instance_ = new Health(c);
    }
    return instance_;
}



///////////////////////////////

static void wakealarm_init(void) {
    wakealarm_fd = timerfd_create(CLOCK_BOOTTIME_ALARM, TFD_NONBLOCK);
    if (wakealarm_fd == -1) {
        KLOG_ERROR(LOG_TAG, "wakealarm_init: timerfd_create failed\n");
        return;
    }

    if (healthd_register_event(wakealarm_fd, wakealarm_event, EVENT_WAKEUP_FD))
        KLOG_ERROR(LOG_TAG, "Registration of wakealarm event failed\n");

    //设置时间间隔
    wakealarm_set_interval(healthd_config.periodic_chores_interval_fast);
}

static void wakealarm_event(uint32_t /*epevents*/) {
    unsigned long long wakeups;

    if (read(wakealarm_fd, &wakeups, sizeof(wakeups)) == -1) {
        KLOG_ERROR(LOG_TAG, "wakealarm_event: read wakealarm fd failed\n");
        return;
    }

    periodic_chores();
}

static void periodic_chores() {
    healthd_battery_update();
}


static void uevent_init(void) {
    uevent_fd = uevent_open_socket(64 * 1024, true);

    if (uevent_fd < 0) {
        KLOG_ERROR(LOG_TAG, "uevent_init: uevent_open_socket failed\n");
        return;
    }

    fcntl(uevent_fd, F_SETFL, O_NONBLOCK);
    if (healthd_register_event(uevent_fd, uevent_event, EVENT_WAKEUP_FD))
        KLOG_ERROR(LOG_TAG, "register for uevent events failed\n");
}

#define UEVENT_MSG_LEN 2048
static void uevent_event(uint32_t /*epevents*/) {
    char msg[UEVENT_MSG_LEN + 2];
    char* cp;
    int n;

    n = uevent_kernel_multicast_recv(uevent_fd, msg, UEVENT_MSG_LEN);
    if (n <= 0) return;
    if (n >= UEVENT_MSG_LEN) /* overflow -- discard */
        return;

    msg[n] = '\0';
    msg[n + 1] = '\0';
    cp = msg;

    while (*cp) {
        if (!strcmp(cp, "SUBSYSTEM=" POWER_SUPPLY_SUBSYSTEM)) {  // #define POWER_SUPPLY_SUBSYSTEM "power_supply"
            healthd_battery_update();
            break;
        }

        /* advance to after the next \0 */
        while (*cp++)
            ;
    }
}


//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/health/2.0/default/healthd_common.cpp
int healthd_register_event(int fd, void (*handler)(uint32_t), EventWakeup wakeup) {
    struct epoll_event ev;

    ev.events = EPOLLIN;

    if (wakeup == EVENT_WAKEUP_FD) ev.events |= EPOLLWAKEUP;

    ev.data.ptr = (void*)handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        KLOG_ERROR(LOG_TAG, "epoll_ctl failed; errno=%d\n", errno);
        return -1;
    }

    eventct++;
    return 0;
}






static void healthd_mainloop(void) {
    int nevents = 0;
    while (1) {
        struct epoll_event events[eventct];
        int timeout = awake_poll_interval;
        int mode_timeout;

        /* Don't wait for first timer timeout to run periodic chores */
        if (!nevents) periodic_chores();

        healthd_mode_ops->heartbeat();　　// 空实现

        /*
        int healthd_mode_service_2_0_preparetowait(void) {
            IPCThreadState::self()->flushCommands();
            return -1;
        }
        */
        mode_timeout = healthd_mode_ops->preparetowait();
        if (timeout < 0 || (mode_timeout > 0 && mode_timeout < timeout)) timeout = mode_timeout;
        nevents = epoll_wait(epollfd, events, eventct, timeout);
        if (nevents == -1) {
            if (errno == EINTR) continue;
            KLOG_ERROR(LOG_TAG, "healthd_mainloop: epoll_wait failed\n");
            break;
        }

        for (int n = 0; n < nevents; ++n) {
            if (events[n].data.ptr) (*(void (*)(int))events[n].data.ptr)(events[n].events);
        }
    }

    return;
}