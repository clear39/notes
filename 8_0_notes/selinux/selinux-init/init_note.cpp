int main(int argc, char** argv) {
    。。。。。。

    bool is_first_stage = (getenv("INIT_SECOND_STAGE") == nullptr); //系统第一次启动为true

    if (is_first_stage) {
 	    。。。。。。
        mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL);

	    。。。。。。

        // Set up SELinux, loading the SELinux policy.
        selinux_initialize(true);  //分析一  // 将 /sepolicy 文件内容写入到 /sys/fs/selinux/load 中

        // We're in the kernel domain, so re-exec init to transition to the init domain now
        // that the SELinux policy has been loaded.
	    // external/selinux/libselinux/src/android/android_platform.c
        if (selinux_android_restorecon("/init", 0) == -1) {//分析二
            PLOG(ERROR) << "restorecon failed";
            security_failure();
        }

        setenv("INIT_SECOND_STAGE", "true", 1);

        char* path = argv[0];
        char* args[] = { path, nullptr };
        execv(path, args);

        。。。。。。
        security_failure();
    }

    。。。。。。

    // Now set up SELinux for second stage.
    selinux_initialize(false);  //分析三
    selinux_restore_context();  //分析四

    。。。。。。

    return 0;
}


/* Callback facilities */
union selinux_callback {
    /* log the printf-style format and arguments,
       with the type code indicating the type of message */
    int 
#ifdef __GNUC__
__attribute__ ((format(printf, 2, 3)))
#endif
    (*func_log) (int type, const char *fmt, ...);
    /* store a string representation of auditdata (corresponding
       to the given security class) into msgbuf. */
    int (*func_audit) (void *auditdata, security_class_t cls,char *msgbuf, size_t msgbufsize);
    /* validate the supplied context, modifying if necessary */
    int (*func_validate) (char **ctx);
    /* netlink callback for setenforce message */
    int (*func_setenforce) (int enforcing);
    /* netlink callback for policyload message */
    int (*func_policyload) (int seqno);
};


//  external/selinux/libselinux/src/callbacks.c
/* callback setting function */
void selinux_set_callback(int type, union selinux_callback cb)
{
    switch (type) {
    case SELINUX_CB_LOG:
        selinux_log = cb.func_log;
        break;
    case SELINUX_CB_AUDIT:
        selinux_audit = cb.func_audit; //selinux_audit默认是空实现
        break;
    case SELINUX_CB_VALIDATE:
        selinux_validate = cb.func_validate;//selinux_validate默认是空实现
        break;
    case SELINUX_CB_SETENFORCE:
        selinux_netlink_setenforce = cb.func_setenforce;//selinux_netlink_setenforce默认是空实现
        break;
    case SELINUX_CB_POLICYLOAD:
        selinux_netlink_policyload = cb.func_policyload;//selinux_netlink_policyload默认是空实现
        break;
    }
}


//分析一
static void selinux_initialize(bool in_kernel_domain == true) {
    Timer t;


    //  external/selinux/libselinux/include/selinux/selinux.h
    selinux_callback cb;

    //  external/selinux/libselinux/src/callbacks.c
    //  设置不同类型日志打印函数
    cb.func_log = selinux_klog_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);
    cb.func_audit = audit_callback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);//设置selinux记录回到函数

    if (in_kernel_domain) {//true
        LOG(INFO) << "Loading SELinux policy";
        //  system/core/init/init.cpp  // 将 /sepolicy 文件内容写入到 /sys/fs/selinux/load 中
        if (!selinux_load_policy()) {
            panic();
        }

        // external/selinux/libselinux/src/getenforce.c   //    读取 /sys/fs/selinux/enforce
        bool kernel_enforcing = (security_getenforce() == 1); // 这里 security_getenforce() 返回值为0

        bool is_enforcing = selinux_is_enforcing(); // 这里 is_enforcing 为 false
        
        if (kernel_enforcing != is_enforcing) { // kernel_enforcing == false;   is_enforcing == false
            //   external/selinux/libselinux/src/setenforce.c
            if (security_setenforce(is_enforcing)) {
                PLOG(ERROR) << "security_setenforce(%s) failed" << (is_enforcing ? "true" : "false");
                security_failure();
            }
        }

        std::string err;
        if (!WriteFile("/sys/fs/selinux/checkreqprot", "0", &err)) {
            LOG(ERROR) << err;
            security_failure();
        }

        // init's first stage can't set properties, so pass the time to the second stage.
        setenv("INIT_SELINUX_TOOK", std::to_string(t.duration_ms()).c_str(), 1);
    } else {
        selinux_init_all_handles();
    }
}



/*
 * Loads SELinux policy into the kernel.
 *
 * Returns true upon success, false otherwise (failure cause is logged).
 */
//  system/core/init/init.cpp
static bool selinux_load_policy() {
    //这里执行selinux_load_monolithic_policy()，selinux_load_split_policy()以后在分析
    return selinux_is_split_policy_device() ? selinux_load_split_policy() : selinux_load_monolithic_policy();
}


///system/etc/selinux/plat_sepolicy.cil不存在，则返回false
static constexpr const char plat_policy_cil_file[] = "/system/etc/selinux/plat_sepolicy.cil";
static bool selinux_is_split_policy_device() { return access(plat_policy_cil_file, R_OK) != -1; }


/*
 * Loads SELinux policy from a monolithic（单片式） file.
 *
 * Returns true upon success, false otherwise (failure cause is logged).
 */
// 将 /sepolicy 文件内容写入到 /sys/fs/selinux/load 中
static bool selinux_load_monolithic_policy() {
    LOG(VERBOSE) << "Loading SELinux policy from monolithic file";
    // external/selinux/libselinux/src/android/android_platform.c
    if (selinux_android_load_policy() < 0) { //将 "/sepolicy" 文件数据写入 /sys/fs/selinux/load 中
        PLOG(ERROR) << "Failed to load monolithic SELinux policy";
        return false;
    }
    return true;
}


//这个函数的目的是 将 "/sepolicy" 文件数据写入 /sys/fs/selinux/load 中
int selinux_android_load_policy()
{
    int fd = -1;
        //  static const char *const sepolicy_file = "/sepolicy";
    fd = open(sepolicy_file, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        selinux_log(SELINUX_ERROR, "SELinux:  Could not open %s:  %s\n",sepolicy_file, strerror(errno));
        return -1;
    }
    int ret = selinux_android_load_policy_from_fd(fd, sepolicy_file);
    close(fd);
    return ret;
}


int selinux_android_load_policy_from_fd(int fd, const char *description = "/sepolicy")
{
    int rc;
    struct stat sb;
    void *map = NULL;
    static int load_successful = 0;

    /*
     * Since updating policy at runtime has been abolished
     * we just check whether a policy has been loaded before
     * and return if this is the case.
     * There is no point in reloading policy.
     */
    if (load_successful){
      selinux_log(SELINUX_WARNING, "SELinux: Attempted reload of SELinux policy!/n");
      return 0;
    }

    // libselinux/src/policy.h:24:#define SELINUXMNT "/sys/fs/selinux"
    set_selinuxmnt(SELINUXMNT);//设置挂在selinux路径

    if (fstat(fd, &sb) < 0) {
        selinux_log(SELINUX_ERROR, "SELinux:  Could not stat %s:  %s\n",description, strerror(errno));
        return -1;
    }
    map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        selinux_log(SELINUX_ERROR, "SELinux:  Could not map %s:  %s\n",description, strerror(errno));
        return -1;
    }

    rc = security_load_policy(map, sb.st_size);
    if (rc < 0) {
        selinux_log(SELINUX_ERROR, "SELinux:  Could not load policy:  %s\n",strerror(errno));
        munmap(map, sb.st_size);
        return -1;
    }

    munmap(map, sb.st_size);
    selinux_log(SELINUX_INFO, "SELinux: Loaded policy from %s\n", description);
    load_successful = 1;
    return 0;
}

void set_selinuxmnt(const char *mnt)
{
    selinux_mnt = strdup(mnt); // SELINUXMNT == "/sys/fs/selinux"
}


int security_load_policy(void *data, size_t len)
{
    char path[PATH_MAX];
    int fd, ret;

    if (!selinux_mnt) {
        errno = ENOENT;
        return -1;
    }

    snprintf(path, sizeof path, "%s/load", selinux_mnt); //   path=="/sys/fs/selinux/load"
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;

    ret = write(fd, data, len);
    close(fd);
    if (ret < 0)
        return -1;
    return 0;
}



int security_getenforce(void)
{
    int fd, ret, enforce = 0;
    char path[PATH_MAX];
    char buf[20];

    if (!selinux_mnt) {
        errno = ENOENT;
        return -1;
    }

    snprintf(path, sizeof path, "%s/enforce", selinux_mnt);  // path == "/sys/fs/selinux/enforce"
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    memset(buf, 0, sizeof buf);
    ret = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (ret < 0)
        return -1;

    if (sscanf(buf, "%d", &enforce) != 1)
        return -1;

    return !!enforce;
}
    


static bool selinux_is_enforcing(void)
{
    if (ALLOW_PERMISSIVE_SELINUX) { // 由编译引入ALLOW_PERMISSIVE_SELINUX=1
        return selinux_status_from_cmdline() == SELINUX_ENFORCING;
    }
    return true;
}

static selinux_enforcing_status selinux_status_from_cmdline() {
    selinux_enforcing_status status = SELINUX_ENFORCING;
    // 这里 androidboot.selinux=permissive
    // system/core/init/util.cpp 
    import_kernel_cmdline(false, [&](const std::string& key, const std::string& value, bool in_qemu) {
        if (key == "androidboot.selinux" && value == "permissive") {
            status = SELINUX_PERMISSIVE;
        }
    });

    return status;
}

void import_kernel_cmdline(bool in_qemu, const std::function<void(const std::string&, const std::string&, bool)>& fn) {
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);

/**
# cat /proc/cmdline                                       
console=ttymxc0,115200 init=/init video=mxcfb0:dev=ldb,fbpix=RGB32,bpp=32 video=mxcfb1:off 
video=mxcfb2:off video=mxcfb3:off vmalloc=400M androidboot.console=ttymxc0 consoleblank=0 
androidboot.hardware=freescale cma=256M androidboot.selinux=permissive buildvariant=eng 
androidboot.serialno=111139d4e6fdf288 androidboot.soc_type=imx6q androidboot.storage_type=emmc
*/


    for (const auto& entry : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> pieces = android::base::Split(entry, "=");
        if (pieces.size() == 2) {
            fn(pieces[0], pieces[1], in_qemu);
        }
    }
}



int security_setenforce(int value)
{
    int fd, ret;
    char path[PATH_MAX];
    char buf[20];

    if (!selinux_mnt) {
        errno = ENOENT;
        return -1;
    }

    snprintf(path, sizeof path, "%s/enforce", selinux_mnt);
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;

    snprintf(buf, sizeof buf, "%d", value);
    ret = write(fd, buf, strlen(buf));
    close(fd);
    if (ret < 0)
        return -1;

    return 0;
}
