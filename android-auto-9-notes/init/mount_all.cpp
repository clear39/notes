/**
 * 
 * 
 * 
 * 入口　＠/work/workcodes/aosp-p9.x-auto-ga/system/core/init/builtins.cpp
 * 
 * 在init.*.rc中有如下action
 on fs
    # mount ext4 partitions
    mount_all /vendor/etc/fstab.freescale --early


on late-fs
    # Start keymaster service
    start vendor.keymaster-3-0-${ro.boot.keystore}
    start servicemanager
    start surfaceflinger

    # Mount RW partitions which need run fsck
    mount_all /vendor/etc/fstab.freescale --late
 * 
 */


/* mount_all <fstab> [ <path> ]* [--<options>]*
 *
 * This function might request a reboot, in which case it will
 * not return.
 */
static Result<Success> do_mount_all(const BuiltinArguments& args) {
    std::size_t na = 0;
    bool import_rc = true;
    bool queue_event = true;
    int mount_mode = MOUNT_MODE_DEFAULT;
    const char* fstabfile = args[1].c_str();
    std::size_t path_arg_end = args.size();
    const char* prop_post_fix = "default";

    for (na = args.size() - 1; na > 1; --na) {
        if (args[na] == "--early") {
            path_arg_end = na;
            queue_event = false;
            mount_mode = MOUNT_MODE_EARLY;
            prop_post_fix = "early";
        } else if (args[na] == "--late") {
            path_arg_end = na;
            import_rc = false;
            mount_mode = MOUNT_MODE_LATE;
            prop_post_fix = "late";
        }
    }

    /***
     * ro.boottime.init.mount_all.early
     * ro.boottime.init.mount_all.late
     * */
    std::string prop_name = "ro.boottime.init.mount_all."s + prop_post_fix;
    android::base::Timer t;
    /**
     * 如果有参数 --early 则 mount_mode 为 MOUNT_MODE_EARLY
     * 如果有参数 --late 则 mount_mode 为 MOUNT_MODE_LATE
     * */
    auto mount_fstab_return_code = mount_fstab(fstabfile, mount_mode);
    if (!mount_fstab_return_code) {
        return Error() << "mount_fstab() failed " << mount_fstab_return_code.error();
    }
    /**
     * 如果有参数 --early 则 设置 ro.boottime.init.mount_all.early 时间点
     * 如果有参数 --late 则 设置 ro.boottime.init.mount_all.late 时间点
     * */
    property_set(prop_name, std::to_string(t.duration().count()));


     /**
     * 如果有参数 --early 则 import_rc 为 true
     * 如果有参数 --late 则 import_rc 为 false
     * */
    if (import_rc) { // 则 --early 执行
        /* Paths of .rc files are specified at the 2nd argument and beyond */
        import_late(args.args, 2, path_arg_end);
    }

    /**
     * 如果有参数 --early 则 queue_event 为 false
     * 如果有参数 --late 则 queue_event 为 true
     * */
    if (queue_event) { // 这里 只有 --late 执行
        /* queue_fs_event will queue event based on mount_fstab return code
         * and return processed return code*/
        auto queue_fs_result = queue_fs_event(*mount_fstab_return_code);
        if (!queue_fs_result) {
            return Error() << "queue_fs_event() failed: " << queue_fs_result.error();
        }
    }

    return Success();
}



/* mount_fstab
 *
 *  Call fs_mgr_mount_all() to mount the given fstab
 */
static Result<int> mount_fstab(const char* fstabfile, int mount_mode) {
    /*
     * Call fs_mgr_mount_all() to mount all filesystems.  We fork(2) and
     * do the call in the child to provide protection to the main init
     * process if anything goes wrong (crash or memory leak), and wait for
     * the child to finish in the parent.
     */
    pid_t pid = fork();
    if (pid > 0) {
        /* Parent.  Wait for the child to return */
        int status;
        int wp_ret = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));
        if (wp_ret == -1) {
            // Unexpected error code. We will continue anyway.
            PLOG(WARNING) << "waitpid failed";
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return Error() << "child aborted";
        }
    } else if (pid == 0) {
        /* child, call fs_mgr_mount_all() */

        // So we can always see what fs_mgr_mount_all() does.
        // Only needed if someone explicitly changes the default log level in their init.rc.
        android::base::ScopedLogSeverity info(android::base::INFO);

        struct fstab* fstab = fs_mgr_read_fstab(fstabfile);
        int child_ret = fs_mgr_mount_all(fstab, mount_mode);
        fs_mgr_free_fstab(fstab);
        if (child_ret == -1) {
            PLOG(ERROR) << "fs_mgr_mount_all returned an error";
        }
        _exit(child_ret);
    } else {
        return Error() << "fork() failed";
    }
}

//  @   system/core/fs_mgr/fs_mgr.cpp
/* When multiple fstab records share the same mount_point, it will
 * try to mount each one in turn, and ignore any duplicates after a
 * first successful mount.
 * Returns -1 on error, and  FS_MGR_MNTALL_* otherwise.
 */
int fs_mgr_mount_all(struct fstab *fstab, int mount_mode)
{
    int i = 0;
    int encryptable = FS_MGR_MNTALL_DEV_NOT_ENCRYPTABLE;
    int error_count = 0;
    int mret = -1;
    int mount_errno = 0;
    int attempted_idx = -1;
    FsManagerAvbUniquePtr avb_handle(nullptr);
    /****
     * @ system/core/fs_mgr/fs_mgr_fstab.cpp
     * 
     * fs_mgr_read_fstab_dt 在 DoFirstStageMount() 中挂在过
     * 这里用于记载已经挂在过的 路径 不在需要挂在
     * */
    struct fstab *fstab_dt = fs_mgr_read_fstab_dt();

    if (!fstab) {
        return FS_MGR_MNTALL_FAIL;
    }

    for (i = 0; i < fstab->num_entries; i++) {


        /**
         * Don't mount entries that are managed by vold or not for the mount mode
         * fs_mgr_is_latemount @    system/core/fs_mgr/fs_mgr_fstab.cpp
         * fs_mgr_is_latemount 监测 fs_mgr_flags 是否存在 MF_LATEMOUNT 属性
         * */
        if ((fstab->recs[i].fs_mgr_flags & (MF_VOLDMANAGED | MF_RECOVERYONLY)) ||
             ((mount_mode == MOUNT_MODE_LATE) && !fs_mgr_is_latemount(&fstab->recs[i])) ||
             ((mount_mode == MOUNT_MODE_EARLY) && fs_mgr_is_latemount(&fstab->recs[i]))) {
            continue;
        }

        /**
         * system/core/fs_mgr/fs_mgr.cpp
         * 监测是否在 DoFirstStageMount() 挂在过
        */
        if (fs_mgr_dt_mounted(fstab_dt, fstab->recs[i].mount_point))
            continue;

        /* Skip swap and raw partition entries such as boot, recovery, etc */
        if (!strcmp(fstab->recs[i].fs_type, "swap") ||
            !strcmp(fstab->recs[i].fs_type, "emmc") ||  // misc
            !strcmp(fstab->recs[i].fs_type, "mtd")) {
            continue;
        }

        /* Skip mounting the root partition, as it will already have been mounted */
        if (!strcmp(fstab->recs[i].mount_point, "/")) {
            if ((fstab->recs[i].fs_mgr_flags & MS_RDONLY) != 0) {
                fs_mgr_set_blk_ro(fstab->recs[i].blk_device);
            }
            continue;
        }

        /* Translate LABEL= file system labels into block devices */
        if (is_extfs(fstab->recs[i].fs_type)) {
            int tret = translate_ext_labels(&fstab->recs[i]);
            if (tret < 0) {
                LERROR << "Could not translate label to block device";
                continue;
            }
        }
        /**
         * fs_mgr_wait_for_file @   system/core/fs_mgr/fs_mgr.cpp
         * fs_mgr_wait_for_file 实现 就是while循环 等待 fstab->recs[i].blk_device 路径存在了
        */
        if (fstab->recs[i].fs_mgr_flags & MF_WAIT && !fs_mgr_wait_for_file(fstab->recs[i].blk_device, 20s)) {
            LERROR << "Skipping '" << fstab->recs[i].blk_device << "' during mount_all";
            continue;
        }

        if (fstab->recs[i].fs_mgr_flags & MF_AVB) {
            if (!avb_handle) {
                avb_handle = FsManagerAvbHandle::Open(*fstab);
                if (!avb_handle) {
                    LERROR << "Failed to open FsManagerAvbHandle";
                    return FS_MGR_MNTALL_FAIL;
                }
            }
            if (avb_handle->SetUpAvbHashtree(&fstab->recs[i], true /* wait_for_verity_dev */) ==
                SetUpAvbHashtreeResult::kFail) {
                LERROR << "Failed to set up AVB on partition: " << fstab->recs[i].mount_point << ", skipping!";
                /* Skips mounting the device. */
                continue;
            }
        } else if ((fstab->recs[i].fs_mgr_flags & MF_VERIFY)) {
            int rc = fs_mgr_setup_verity(&fstab->recs[i], true);
            if (__android_log_is_debuggable() &&
                    (rc == FS_MGR_SETUP_VERITY_DISABLED ||
                     rc == FS_MGR_SETUP_VERITY_SKIPPED)) {
                LINFO << "Verity disabled";
            } else if (rc != FS_MGR_SETUP_VERITY_SUCCESS) {
                LERROR << "Could not set up verified partition, skipping!";
                continue;
            }
        }

        int last_idx_inspected;
        int top_idx = i;
        /****
         *mount_with_alternatives @    system/core/fs_mgr/fs_mgr.cpp
         * */
        mret = mount_with_alternatives(fstab, i, &last_idx_inspected, &attempted_idx);
        i = last_idx_inspected;
        mount_errno = errno;

        /* Deal with encryptability. */
        if (!mret) {
            int status = handle_encryptable(&fstab->recs[attempted_idx]);

            if (status == FS_MGR_MNTALL_FAIL) {
                /* Fatal error - no point continuing */
                return status;
            }

            if (status != FS_MGR_MNTALL_DEV_NOT_ENCRYPTABLE) {
                if (encryptable != FS_MGR_MNTALL_DEV_NOT_ENCRYPTABLE) {
                    // Log and continue
                    LERROR << "Only one encryptable/encrypted partition supported";
                }
                encryptable = status;
                if (status == FS_MGR_MNTALL_DEV_NEEDS_METADATA_ENCRYPTION) {
                    if (!call_vdc(
                            {"cryptfs", "encryptFstab", fstab->recs[attempted_idx].mount_point})) {
                        LERROR << "Encryption failed";
                        return FS_MGR_MNTALL_FAIL;
                    }
                }
            }

            /* Success!  Go get the next one */
            continue;
        }

        bool wiped = partition_wiped(fstab->recs[top_idx].blk_device);
        bool crypt_footer = false;
        if (mret && mount_errno != EBUSY && mount_errno != EACCES &&
            fs_mgr_is_formattable(&fstab->recs[top_idx]) && wiped) {
            /* top_idx and attempted_idx point at the same partition, but sometimes
             * at two different lines in the fstab.  Use the top one for formatting
             * as that is the preferred one.
             */
            LERROR << __FUNCTION__ << "(): " << fstab->recs[top_idx].blk_device
                   << " is wiped and " << fstab->recs[top_idx].mount_point
                   << " " << fstab->recs[top_idx].fs_type
                   << " is formattable. Format it.";
            if (fs_mgr_is_encryptable(&fstab->recs[top_idx]) &&
                strcmp(fstab->recs[top_idx].key_loc, KEY_IN_FOOTER)) {
                int fd = open(fstab->recs[top_idx].key_loc, O_WRONLY);
                if (fd >= 0) {
                    LINFO << __FUNCTION__ << "(): also wipe "
                          << fstab->recs[top_idx].key_loc;
                    wipe_block_device(fd, get_file_size(fd));
                    close(fd);
                } else {
                    PERROR << __FUNCTION__ << "(): "
                           << fstab->recs[top_idx].key_loc << " wouldn't open";
                }
            } else if (fs_mgr_is_encryptable(&fstab->recs[top_idx]) &&
                !strcmp(fstab->recs[top_idx].key_loc, KEY_IN_FOOTER)) {
                crypt_footer = true;
            }
            if (fs_mgr_do_format(&fstab->recs[top_idx], crypt_footer) == 0) {
                /* Let's replay the mount actions. */
                i = top_idx - 1;
                continue;
            } else {
                LERROR << __FUNCTION__ << "(): Format failed. "
                       << "Suggest recovery...";
                encryptable = FS_MGR_MNTALL_DEV_NEEDS_RECOVERY;
                continue;
            }
        }

        /* mount(2) returned an error, handle the encryptable/formattable case */
        if (mret && mount_errno != EBUSY && mount_errno != EACCES &&
            fs_mgr_is_encryptable(&fstab->recs[attempted_idx])) {
            if (wiped) {
                LERROR << __FUNCTION__ << "(): "
                       << fstab->recs[attempted_idx].blk_device
                       << " is wiped and "
                       << fstab->recs[attempted_idx].mount_point << " "
                       << fstab->recs[attempted_idx].fs_type
                       << " is encryptable. Suggest recovery...";
                encryptable = FS_MGR_MNTALL_DEV_NEEDS_RECOVERY;
                continue;
            } else {
                /* Need to mount a tmpfs at this mountpoint for now, and set
                 * properties that vold will query later for decrypting
                 */
                LERROR << __FUNCTION__ << "(): possibly an encryptable blkdev "
                       << fstab->recs[attempted_idx].blk_device
                       << " for mount " << fstab->recs[attempted_idx].mount_point
                       << " type " << fstab->recs[attempted_idx].fs_type;
                if (fs_mgr_do_tmpfs_mount(fstab->recs[attempted_idx].mount_point) < 0) {
                    ++error_count;
                    continue;
                }
            }
            encryptable = FS_MGR_MNTALL_DEV_MIGHT_BE_ENCRYPTED;
        } else if (mret && mount_errno != EBUSY && mount_errno != EACCES &&
                   should_use_metadata_encryption(&fstab->recs[attempted_idx])) {
            if (!call_vdc({"cryptfs", "mountFstab", fstab->recs[attempted_idx].mount_point})) {
                ++error_count;
            }
            encryptable = FS_MGR_MNTALL_DEV_IS_METADATA_ENCRYPTED;
            continue;
        } else {
            // fs_options might be null so we cannot use PERROR << directly.
            // Use StringPrintf to output "(null)" instead.
            if (fs_mgr_is_nofail(&fstab->recs[attempted_idx])) {
                PERROR << android::base::StringPrintf(
                    "Ignoring failure to mount an un-encryptable or wiped "
                    "partition on %s at %s options: %s",
                    fstab->recs[attempted_idx].blk_device, fstab->recs[attempted_idx].mount_point,
                    fstab->recs[attempted_idx].fs_options);
            } else {
                PERROR << android::base::StringPrintf(
                    "Failed to mount an un-encryptable or wiped partition "
                    "on %s at %s options: %s",
                    fstab->recs[attempted_idx].blk_device, fstab->recs[attempted_idx].mount_point,
                    fstab->recs[attempted_idx].fs_options);
                ++error_count;
            }
            continue;
        }
    }

    fs_mgr_free_fstab(fstab_dt);
    if (error_count) {
        return FS_MGR_MNTALL_FAIL;
    } else {
        return encryptable;
    }
}