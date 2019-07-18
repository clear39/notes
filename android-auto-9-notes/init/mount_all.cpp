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

    std::string prop_name = "ro.boottime.init.mount_all."s + prop_post_fix;
    android::base::Timer t;
    auto mount_fstab_return_code = mount_fstab(fstabfile, mount_mode);
    if (!mount_fstab_return_code) {
        return Error() << "mount_fstab() failed " << mount_fstab_return_code.error();
    }
    property_set(prop_name, std::to_string(t.duration().count()));

    if (import_rc) {
        /* Paths of .rc files are specified at the 2nd argument and beyond */
        import_late(args.args, 2, path_arg_end);
    }

    if (queue_event) {
        /* queue_fs_event will queue event based on mount_fstab return code
         * and return processed return code*/
        auto queue_fs_result = queue_fs_event(*mount_fstab_return_code);
        if (!queue_fs_result) {
            return Error() << "queue_fs_event() failed: " << queue_fs_result.error();
        }
    }

    return Success();
}