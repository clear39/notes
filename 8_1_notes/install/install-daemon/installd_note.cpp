service installd /system/bin/installd
    class main


//	@frameworks/native/cmds/installd/installd.cpp
int main(const int argc, char *argv[]) {
    return android::installd::installd_main(argc, argv);
}


static int installd_main(const int argc ATTRIBUTE_UNUSED, char *argv[]) {
    int ret;
    int selinux_enabled = (is_selinux_enabled() > 0);

    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv);

    SLOGI("installd firing up");

    union selinux_callback cb;
    cb.func_log = log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);
    /*初始化

    dir_rec_t android_app_dir;
    dir_rec_t android_app_ephemeral_dir;
    dir_rec_t android_app_lib_dir;
    dir_rec_t android_app_private_dir;
    dir_rec_t android_asec_dir;
    dir_rec_t android_data_dir;
    dir_rec_t android_media_dir;
    dir_rec_t android_mnt_expand_dir;
    dir_rec_t android_profiles_dir;

    dir_rec_array_t android_system_dirs;

    变量

    */
    if (!initialize_globals()) {
        SLOGE("Could not initialize globals; exiting.\n");
        exit(1);
    }

    /**
     
    */

    if (initialize_directories() < 0) {
        SLOGE("Could not create directories; exiting.\n");
        exit(1);
    }

    if (selinux_enabled && selinux_status_open(true) < 0) {
        SLOGE("Could not open selinux status; exiting.\n");
        exit(1);
    }

    if ((ret = InstalldNativeService::start()) != android::OK) {
        SLOGE("Unable to start InstalldNativeService: %d", ret);
        exit(1);
    }

    IPCThreadState::self()->joinThreadPool();

    LOG(INFO) << "installd shutting down";

    return 0;
}


//			external/selinux/libselinux/src/enabled.c
int is_selinux_enabled(void)
{
	/* init_selinuxmnt() gets called before this function. We
 	 * will assume that if a selinux file system is mounted, then
 	 * selinux is enabled. */
#ifdef ANDROID
	return (selinux_mnt ? 1 : 0);
#else
	return (selinux_mnt && has_selinux_config);
#endif
}


static int log_callback(int type, const char *fmt, ...) {
    va_list ap;
    int priority;

    switch (type) {
    case SELINUX_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
    case SELINUX_INFO:
        priority = ANDROID_LOG_INFO;
        break;
    default:
        priority = ANDROID_LOG_ERROR;
        break;
    }
    va_start(ap, fmt);
    LOG_PRI_VA(priority, "SELinux", fmt, ap);
    va_end(ap);
    return 0;
}


static bool initialize_globals() {
    const char* data_path = getenv("ANDROID_DATA");
    if (data_path == nullptr) {
        SLOGE("Could not find ANDROID_DATA");
        return false;
    }
    const char* root_path = getenv("ANDROID_ROOT");
    if (root_path == nullptr) {
        SLOGE("Could not find ANDROID_ROOT");
        return false;
    }

    return init_globals_from_data_and_root(data_path, root_path);
}

//  @native/cmds/installd/globals.h
struct dir_rec_t {
    char* path;
    size_t len;
};

struct dir_rec_array_t {
    size_t count;
    dir_rec_t* dirs;
};

//			frameworks/native/cmds/installd/globals.cpp
dir_rec_t android_app_dir;
dir_rec_t android_app_ephemeral_dir;
dir_rec_t android_app_lib_dir;
dir_rec_t android_app_private_dir;
dir_rec_t android_asec_dir;
dir_rec_t android_data_dir;
dir_rec_t android_media_dir;
dir_rec_t android_mnt_expand_dir;
dir_rec_t android_profiles_dir;

bool init_globals_from_data_and_root(const char* data = "/data", const char* root = "/system") {
    // Get the android data directory.
    //  get_path_from_string @frameworks/native/cmds/installd/utils.cpp  
    //  get_path_from_string 将data赋值给android_data_dir 如果 data 不是以“/”结尾，就添加一个“/”
    if (get_path_from_string(&android_data_dir, data) < 0) { // android_data_dir = "/data/"
        return false;
    }

    // Get the android app directory.
    //  static constexpr const char* APP_SUBDIR = "app/"; // sub-directory under ANDROID_DATA
    if (copy_and_append(&android_app_dir, &android_data_dir, APP_SUBDIR) < 0) { //android_app_dir = "/data/app/"
        return false;
    }

    // Get the android protected app directory.
    //  static constexpr const char* PRIVATE_APP_SUBDIR = "app-private/";
    if (copy_and_append(&android_app_private_dir, &android_data_dir, PRIVATE_APP_SUBDIR) < 0) {//   android_app_private_dir = "/data/app-private/"
        return false;
    }

    // Get the android ephemeral app directory.
    //  static constexpr const char* EPHEMERAL_APP_SUBDIR = "app-ephemeral/";
    if (copy_and_append(&android_app_ephemeral_dir, &android_data_dir, EPHEMERAL_APP_SUBDIR) < 0) {//   android_app_ephemeral_dir = "/data/app-ephemeral/"
        return false;
    }

    // Get the android app native library directory.
    //  static constexpr const char* APP_LIB_SUBDIR = "app-lib/";
    if (copy_and_append(&android_app_lib_dir, &android_data_dir, APP_LIB_SUBDIR) < 0) {//   android_app_lib_dir = "/data/app-lib/"
        return false;
    }

    // Get the sd-card ASEC mount point.
    //  static constexpr const char* ASEC_MOUNTPOINT_ENV_NAME = "ASEC_MOUNTPOINT";
    //  get_path_from_env @frameworks/native/cmds/installd/utils.cpp
    // get_path_from_env 获取环境变量 ASEC_MOUNTPOINT_ENV_NAME 的值 赋值给 android_asec_dir
    if (get_path_from_env(&android_asec_dir, ASEC_MOUNTPOINT_ENV_NAME) < 0) {// android_asec_dir = "/mnt/asec"
        return false;
    }

    // Get the android media directory.
    //  static constexpr const char* MEDIA_SUBDIR = "media/";
    if (copy_and_append(&android_media_dir, &android_data_dir, MEDIA_SUBDIR) < 0) {//   android_media_dir = "/data/media/"
        return false;
    }

    // Get the android external app directory.
    if (get_path_from_string(&android_mnt_expand_dir, "/mnt/expand/") < 0) { // android_mnt_expand_dir = "/mnt/expand/"
        return false;
    }

    // Get the android profiles directory.
    //  static constexpr const char* PROFILES_SUBDIR = "misc/profiles"
    if (copy_and_append(&android_profiles_dir, &android_data_dir, PROFILES_SUBDIR) < 0) {// android_profiles_dir = "/data/misc/profiles"
        return false;
    }

    // Take note of the system and vendor directories.
    android_system_dirs.count = 4;//    dir_rec_array_t android_system_dirs;

    android_system_dirs.dirs = (dir_rec_t*) calloc(android_system_dirs.count, sizeof(dir_rec_t));
    if (android_system_dirs.dirs == NULL) {
        ALOGE("Couldn't allocate array for dirs; aborting\n");
        return false;
    }

    dir_rec_t android_root_dir;
    if (get_path_from_string(&android_root_dir, root) < 0) { //android_root_dir = "/system/"
        return false;
    }

    //build_string2 @frameworks/native/cmds/installd/utils.cpp  将传入的俩个参数链接到一起，得到一个新的路径

    //static constexpr const char* APP_SUBDIR = "app/"; 
    android_system_dirs.dirs[0].path = build_string2(android_root_dir.path, APP_SUBDIR); //android_system_dirs.dirs[0].path = "/system/app/"
    android_system_dirs.dirs[0].len = strlen(android_system_dirs.dirs[0].path);

    //static constexpr const char* PRIV_APP_SUBDIR = "priv-app/"
    android_system_dirs.dirs[1].path = build_string2(android_root_dir.path, PRIV_APP_SUBDIR);//android_system_dirs.dirs[1].path = "/system/priv-app/"
    android_system_dirs.dirs[1].len = strlen(android_system_dirs.dirs[1].path);

    android_system_dirs.dirs[2].path = strdup("/vendor/app/");
    android_system_dirs.dirs[2].len = strlen(android_system_dirs.dirs[2].path);

    android_system_dirs.dirs[3].path = strdup("/oem/app/");
    android_system_dirs.dirs[3].len = strlen(android_system_dirs.dirs[3].path);

    return true;
}




static int initialize_directories() {
    int res = -1;

    // Read current filesystem layout version to handle upgrade paths
    char version_path[PATH_MAX];
    snprintf(version_path, PATH_MAX, "%s.layout_version", android_data_dir.path);// version_path = "/data/.layout_version"

    int oldVersion;
    if (fs_read_atomic_int(version_path, &oldVersion) == -1) {
        oldVersion = 0;
    }
    int version = oldVersion;

    if (version < 2) {
        SLOGD("Assuming that device has multi-user storage layout; upgrade no longer supported");
        version = 2;
    }

    //  @frameworks/native/cmds/installd/utils.cpp
    //  创建/data/misc/user/<userid>并设置权限 这里为/data/misc/user/0 
    if (ensure_config_user_dirs(0) == -1) {
        SLOGE("Failed to setup misc for user 0");
        goto fail;
    }

    if (version == 2) {
        SLOGD("Upgrading to /data/misc/user directories");

        char misc_dir[PATH_MAX];
        snprintf(misc_dir, PATH_MAX, "%smisc", android_data_dir.path);// misc_dir = “/data/misc”

        char keychain_added_dir[PATH_MAX];
        snprintf(keychain_added_dir, PATH_MAX, "%s/keychain/cacerts-added", misc_dir);//    keychain_added_dir = “/data/misc/keychain/cacerts-added"

        char keychain_removed_dir[PATH_MAX];
        snprintf(keychain_removed_dir, PATH_MAX, "%s/keychain/cacerts-removed", misc_dir);//    keychain_removed_dir = “/data/misc/keychain/cacerts-removed"

        DIR *dir;
        struct dirent *dirent;
        dir = opendir("/data/user");
        if (dir != NULL) {
            while ((dirent = readdir(dir))) {
                const char *name = dirent->d_name;

                // skip "." and ".."
                if (name[0] == '.') {
                    if (name[1] == 0) continue;
                    if ((name[1] == '.') && (name[2] == 0)) continue;
                }

                uint32_t user_id = atoi(name);

                // /data/misc/user/<user_id>
                if (ensure_config_user_dirs(user_id) == -1) {
                    goto fail;
                }

                char misc_added_dir[PATH_MAX];
                snprintf(misc_added_dir, PATH_MAX, "%s/user/%s/cacerts-added", misc_dir, name); // misc_added_dir = "/data/misc/user/0/cacerts-added"

                char misc_removed_dir[PATH_MAX];
                snprintf(misc_removed_dir, PATH_MAX, "%s/user/%s/cacerts-removed", misc_dir, name);// misc_removed_dir = "/data/misc/user/0/cacerts-removed"

                uid_t uid = multiuser_get_uid(user_id, AID_SYSTEM);
                gid_t gid = uid;
                if (access(keychain_added_dir, F_OK) == 0) {
                    if (copy_dir_files(keychain_added_dir, misc_added_dir, uid, gid) != 0) {
                        SLOGE("Some files failed to copy");
                    }
                }
                if (access(keychain_removed_dir, F_OK) == 0) {
                    if (copy_dir_files(keychain_removed_dir, misc_removed_dir, uid, gid) != 0) {
                        SLOGE("Some files failed to copy");
                    }
                }
            }
            closedir(dir);

            if (access(keychain_added_dir, F_OK) == 0) {
                delete_dir_contents(keychain_added_dir, 1, 0);
            }
            if (access(keychain_removed_dir, F_OK) == 0) {
                delete_dir_contents(keychain_removed_dir, 1, 0);
            }
        }

        version = 3;
    }

    // Persist layout version if changed
    if (version != oldVersion) {
        if (fs_write_atomic_int(version_path, version) == -1) {
            SLOGE("Failed to save version to %s: %s", version_path, strerror(errno));
            goto fail;
        }
    }

    // Success!
    res = 0;

fail:
    return res;
}


int ensure_config_user_dirs(userid_t userid) {
    // writable by system, readable by any app within the same user
    const int uid = multiuser_get_uid(userid, AID_SYSTEM);
    const int gid = multiuser_get_uid(userid, AID_EVERYBODY);

    // Ensure /data/misc/user/<userid> exists
    auto path = create_data_misc_legacy_path(userid);// path = "/data/misc/user/0"
    return fs_prepare_dir(path.c_str(), 0750, uid, gid);//  @system/core/libcutils/fs.c
}

//  @system/core/libcutils/multiuser.c
uid_t multiuser_get_uid(userid_t user_id, appid_t app_id) {
    //  @system/core/include/cutils/android_filesystem_config.h:#define AID_USER_OFFSET 100000
    return (user_id * AID_USER_OFFSET) + (app_id % AID_USER_OFFSET);
}

std::string create_data_misc_legacy_path(userid_t userid) {
    return StringPrintf("%s/misc/user/%u", create_data_path(nullptr).c_str(), userid);
}

std::string create_data_path(const char* volume_uuid) {
    if (volume_uuid == nullptr) {
        return "/data";
    } else if (!strcmp(volume_uuid, "TEST")) {
        CHECK(property_get_bool("ro.debuggable", false));
        return "/data/local/tmp";
    } else {
        CHECK(is_valid_filename(volume_uuid));
        return StringPrintf("/mnt/expand/%s", volume_uuid);
    }
}

//  @system/core/libcutils/fs.c
int fs_prepare_dir(const char* path, mode_t mode, uid_t uid, gid_t gid) {
    //  @system/core/libcutils/fs.c
    return fs_prepare_path_impl(path, mode, uid, gid, /*allow_fixup*/ 1, /*prepare_as_dir*/ 1);
}


//		external/selinux/libselinux/src/sestatus.c
int selinux_status_open(int fallback == true)
{
	int	fd;
	char	path[PATH_MAX];
	long	pagesize;

	if (!selinux_mnt) {
		errno = ENOENT;
		return -1;
	}

	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0)
		return -1;

	snprintf(path, sizeof(path), "%s/status", selinux_mnt);// /sys/
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		goto error;

	selinux_status = mmap(NULL, pagesize, PROT_READ, MAP_SHARED, fd, 0);
	if (selinux_status == MAP_FAILED) {
		close(fd);
		goto error;
	}
	selinux_status_fd = fd;
	last_seqno = (uint32_t)(-1);

	return 0;

error:
	/*
	 * If caller wants fallback routine, we try to provide
	 * an equivalent functionality using existing netlink
	 * socket, although it needs system call invocation to
	 * receive event notification.
	 */
	if (fallback && avc_netlink_open(0) == 0) {
		union selinux_callback	cb;

		/* register my callbacks */
		cb.func_setenforce = fallback_cb_setenforce;
		selinux_set_callback(SELINUX_CB_SETENFORCE, cb);
		cb.func_policyload = fallback_cb_policyload;
		selinux_set_callback(SELINUX_CB_POLICYLOAD, cb);

		/* mark as fallback mode */
		selinux_status = MAP_FAILED;
		selinux_status_fd = avc_netlink_acquire_fd();
		last_seqno = (uint32_t)(-1);

		fallback_sequence = 0;
		fallback_enforcing = security_getenforce();
		fallback_policyload = 0;

		return 1;
	}
	selinux_status = NULL;

	return -1;
}
