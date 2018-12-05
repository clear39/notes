

/**
if (execl(kFusePath, kFusePath,
                    "-u", "1023", // AID_MEDIA_RW
                    "-g", "1023", // AID_MEDIA_RW
                    "-U", std::to_string(getMountUserId()).c_str(),
                    mRawPath.c_str(),
                    stableName.c_str(),
                    NULL)) {
                PLOG(ERROR) << "Failed to exec";
            }

*/


//	@system/core/sdcard/sdcard.cpp
int main(int argc, char **argv) {
    const char *source_path = NULL;
    const char *label = NULL;
    uid_t uid = 0;
    gid_t gid = 0;
    userid_t userid = 0;
    bool multi_user = false;
    bool full_write = false;
    bool derive_gid = false;
    int i;
    struct rlimit rlim;
    int fs_version;

    int opt;
    while ((opt = getopt(argc, argv, "u:g:U:mwG")) != -1) {
        switch (opt) {
            case 'u':
                uid = strtoul(optarg, NULL, 10);
                break;
            case 'g':
                gid = strtoul(optarg, NULL, 10);
                break;
            case 'U':
                userid = strtoul(optarg, NULL, 10);
                break;
            case 'm':
                multi_user = true;
                break;
            case 'w':
                full_write = true;
                break;
            case 'G':
                derive_gid = true;
                break;
            case '?':
            default:
                return usage();
        }
    }

    for (i = optind; i < argc; i++) {
        char* arg = argv[i];
        if (!source_path) {
            source_path = arg;
        } else if (!label) {
            label = arg;
        } else {
            LOG(ERROR) << "too many arguments";
            return usage();
        }
    }

    if (!source_path) {
        LOG(ERROR) << "no source path specified";
        return usage();
    }
    if (!label) {
        LOG(ERROR) << "no label specified";
        return usage();
    }
    if (!uid || !gid) {
        LOG(ERROR) << "uid and gid must be nonzero";
        return usage();
    }

    rlim.rlim_cur = 8192;
    rlim.rlim_max = 8192;
    if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
        PLOG(ERROR) << "setting RLIMIT_NOFILE failed";
    }

    while ((fs_read_atomic_int("/data/.layout_version", &fs_version) == -1) || (fs_version < 3)) {
        LOG(ERROR) << "installd fs upgrade not yet complete; waiting...";
        sleep(1);
    }

    if (should_use_sdcardfs()) {//这里返回true
        run_sdcardfs(source_path, label, uid, gid, userid, multi_user, full_write, derive_gid);
    } else {
        run(source_path, label, uid, gid, userid, multi_user, full_write);
    }
    return 1;
}



static bool should_use_sdcardfs(void) {
    char property[PROPERTY_VALUE_MAX];

    // Allow user to have a strong opinion about state
    property_get(PROP_SDCARDFS_USER, property, "");	//	#define PROP_SDCARDFS_USER "persist.sys.sdcardfs" //这里为空
    if (!strcmp(property, "force_on")) {
        LOG(WARNING) << "User explicitly enabled sdcardfs";
        return supports_sdcardfs();
    } else if (!strcmp(property, "force_off")) {
        LOG(WARNING) << "User explicitly disabled sdcardfs";
        return false;
    }

    // Fall back to device opinion about state
    if (property_get_bool(PROP_SDCARDFS_DEVICE, true)) {//	#define PROP_SDCARDFS_DEVICE "ro.sys.sdcardfs" //这里也没有设置，但是默认值为true
        LOG(WARNING) << "Device explicitly enabled sdcardfs";
        return supports_sdcardfs();
    } else {
        LOG(WARNING) << "Device explicitly disabled sdcardfs";
        return false;
    }
}


/**
autolink_8qxp:/ # cat /proc/filesystems
nodev   sysfs
nodev   rootfs
nodev   ramfs
nodev   bdev
nodev   proc
nodev   cpuset
nodev   cgroup
nodev   cgroup2
nodev   tmpfs
nodev   devtmpfs
nodev   configfs
nodev   debugfs
nodev   tracefs
nodev   sockfs
nodev   bpf
nodev   pipefs
nodev   hugetlbfs
nodev   devpts
        ext3
        ext2
        ext4
        squashfs
        vfat
        msdos
nodev   sdcardfs    //存在
nodev   autofs
        fuseblk
nodev   fuse
nodev   fusectl
nodev   overlay
nodev   pstore
nodev   mqueue
nodev   selinuxfs
nodev   ubifs
nodev   functionfs

*/
static bool supports_sdcardfs(void) {
    std::string filesystems;
    if (!android::base::ReadFileToString("/proc/filesystems", &filesystems)) {
        PLOG(ERROR) << "Could not read /proc/filesystems";
        return false;
    }
    for (const auto& fs : android::base::Split(filesystems, "\n")) {
        if (fs.find("sdcardfs") != std::string::npos) return true;
    }
    return false;
}






static void run_sdcardfs(const std::string& source_path, const std::string& label, uid_t uid,
                         gid_t gid, userid_t userid, bool multi_user = false, bool full_write = false,
                         bool derive_gid = false) {
    std::string dest_path_default = "/mnt/runtime/default/" + label;
    std::string dest_path_read = "/mnt/runtime/read/" + label;
    std::string dest_path_write = "/mnt/runtime/write/" + label;

    umask(0);
    if (multi_user) {//false
        // Multi-user storage is fully isolated per user, so "other"
        // permissions are completely masked off.
        if (!sdcardfs_setup(source_path, dest_path_default, uid, gid, multi_user, userid,
                            AID_SDCARD_RW, 0006, derive_gid) ||
            !sdcardfs_setup_bind_remount(dest_path_default, dest_path_read, AID_EVERYBODY, 0027) ||
            !sdcardfs_setup_bind_remount(dest_path_default, dest_path_write, AID_EVERYBODY,
                                         full_write ? 0007 : 0027)) {
            LOG(FATAL) << "failed to sdcardfs_setup";
        }
    } else {
        // Physical storage is readable by all users on device, but
        // the Android directories are masked off to a single user
        // deep inside attr_from_stat().
        if (!sdcardfs_setup(source_path, dest_path_default, uid, gid, multi_user, userid, AID_SDCARD_RW, 0006, derive_gid) ||
            !sdcardfs_setup_bind_remount(dest_path_default, dest_path_read, AID_EVERYBODY, full_write ? 0027 : 0022) ||
            !sdcardfs_setup_bind_remount(dest_path_default, dest_path_write, AID_EVERYBODY, full_write ? 0007 : 0022)) {
            LOG(FATAL) << "failed to sdcardfs_setup";
        }
    }

    // Will abort if priv-dropping fails.
    drop_privs(uid, gid);

    if (multi_user) {//false
        std::string obb_path = source_path + "/obb";
        fs_prepare_dir(obb_path.c_str(), 0775, uid, gid);
    }

    exit(0);
}


static void drop_privs(uid_t uid, gid_t gid) {
    ScopedMinijail j(minijail_new());
    minijail_set_supplementary_gids(j.get(), arraysize(kGroups), kGroups);
    minijail_change_gid(j.get(), gid);
    minijail_change_uid(j.get(), uid);
    /* minijail_enter() will abort if priv-dropping fails. */
    minijail_enter(j.get());
}



static bool sdcardfs_setup(const std::string& source_path, const std::string& dest_path,
                           uid_t fsuid, gid_t fsgid, bool multi_user, userid_t userid, gid_t gid,
                           mode_t mask, bool derive_gid) {
    std::string opts = android::base::StringPrintf(
        "fsuid=%d,fsgid=%d,%s%smask=%d,userid=%d,gid=%d", fsuid, fsgid,
        multi_user ? "multiuser," : "", derive_gid ? "derive_gid," : "", mask, userid, gid);

    if (mount(source_path.c_str(), dest_path.c_str(), "sdcardfs",MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME, opts.c_str()) == -1) {
        if (derive_gid) {
            PLOG(ERROR) << "trying to mount sdcardfs filesystem without derive_gid";
            /* Maybe this isn't supported on this kernel. Try without. */
            opts = android::base::StringPrintf("fsuid=%d,fsgid=%d,%smask=%d,userid=%d,gid=%d",
                                               fsuid, fsgid, multi_user ? "multiuser," : "", mask,
                                               userid, gid);
            if (mount(source_path.c_str(), dest_path.c_str(), "sdcardfs",MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME, opts.c_str()) == -1) {
                PLOG(ERROR) << "failed to mount sdcardfs filesystem";
                return false;
            }
        } else {
            PLOG(ERROR) << "failed to mount sdcardfs filesystem";
            return false;
        }
    }
    return true;
}



static bool sdcardfs_setup_bind_remount(const std::string& source_path, const std::string& dest_path,gid_t gid, mode_t mask) {
    std::string opts = android::base::StringPrintf("mask=%d,gid=%d", mask, gid);

    if (mount(source_path.c_str(), dest_path.c_str(), nullptr,MS_BIND, nullptr) != 0) {
        PLOG(ERROR) << "failed to bind mount sdcardfs filesystem";
        return false;
    }

    if (mount(source_path.c_str(), dest_path.c_str(), "none",MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME, opts.c_str()) != 0) {
        PLOG(ERROR) << "failed to mount sdcardfs filesystem";
        if (umount2(dest_path.c_str(), MNT_DETACH))
            PLOG(WARNING) << "Failed to unmount bind";
        return false;
    }

    return true;
}

