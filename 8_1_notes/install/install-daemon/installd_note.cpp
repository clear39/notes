//	@frameworks/native/cmds/installd/installd.cpp
int main(const int argc, char *argv[]) {
    return android::installd::installd_main(argc, argv);
}

static int installd_main(const int argc ATTRIBUTE_UNUSED, char *argv[]) {
    int ret;
    int selinux_enabled = (is_selinux_enabled() > 0);

    setenv("ANDROID_LOG_TAGS", "*:v", 1);//设置日志等级环境变量
    android::base::InitLogging(argv);//在init进程中是被重定向到了kernel打印，重新初始化日志出（重定向到logd）

    SLOGI("installd firing up");

    union selinux_callback cb;
    cb.func_log = log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);//同样在init进程中是被重定向到了kernel打印,设置selinux日志打印回到函数（重定向到logd）

    if (!initialize_globals()) {
        SLOGE("Could not initialize globals; exiting.\n");
        exit(1);
    }

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

//	@frameworks/native/cmds/installd/installd.cpp
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

//	@frameworks/native/cmds/installd/installd.cpp
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

//	@frameworks/native/cmds/installd/globals.cpp
bool init_globals_from_data_and_root(const char* data, const char* root) {
    // Get the android data directory.
    if (get_path_from_string(&android_data_dir, data) < 0) {
        return false;
    }

    // Get the android app directory.
    if (copy_and_append(&android_app_dir, &android_data_dir, APP_SUBDIR) < 0) {
        return false;
    }

    // Get the android protected app directory.
    if (copy_and_append(&android_app_private_dir, &android_data_dir, PRIVATE_APP_SUBDIR) < 0) {
        return false;
    }

    // Get the android ephemeral app directory.
    if (copy_and_append(&android_app_ephemeral_dir, &android_data_dir, EPHEMERAL_APP_SUBDIR) < 0) {
        return false;
    }

    // Get the android app native library directory.
    if (copy_and_append(&android_app_lib_dir, &android_data_dir, APP_LIB_SUBDIR) < 0) {
        return false;
    }

    // Get the sd-card ASEC mount point.
    if (get_path_from_env(&android_asec_dir, ASEC_MOUNTPOINT_ENV_NAME) < 0) {
        return false;
    }

    // Get the android media directory.
    if (copy_and_append(&android_media_dir, &android_data_dir, MEDIA_SUBDIR) < 0) {
        return false;
    }

    // Get the android external app directory.
    if (get_path_from_string(&android_mnt_expand_dir, "/mnt/expand/") < 0) {
        return false;
    }

    // Get the android profiles directory.
    if (copy_and_append(&android_profiles_dir, &android_data_dir, PROFILES_SUBDIR) < 0) {
        return false;
    }

    // Take note of the system and vendor directories.
    android_system_dirs.count = 4;

    android_system_dirs.dirs = (dir_rec_t*) calloc(android_system_dirs.count, sizeof(dir_rec_t));
    if (android_system_dirs.dirs == NULL) {
        ALOGE("Couldn't allocate array for dirs; aborting\n");
        return false;
    }

    dir_rec_t android_root_dir;
    if (get_path_from_string(&android_root_dir, root) < 0) {
        return false;
    }

    android_system_dirs.dirs[0].path = build_string2(android_root_dir.path, APP_SUBDIR);
    android_system_dirs.dirs[0].len = strlen(android_system_dirs.dirs[0].path);

    android_system_dirs.dirs[1].path = build_string2(android_root_dir.path, PRIV_APP_SUBDIR);
    android_system_dirs.dirs[1].len = strlen(android_system_dirs.dirs[1].path);

    android_system_dirs.dirs[2].path = strdup("/vendor/app/");
    android_system_dirs.dirs[2].len = strlen(android_system_dirs.dirs[2].path);

    android_system_dirs.dirs[3].path = strdup("/oem/app/");
    android_system_dirs.dirs[3].len = strlen(android_system_dirs.dirs[3].path);

    return true;
}






//	@frameworks/native/cmds/installd/installd.cpp
static int initialize_directories() {
    int res = -1;

    // Read current filesystem layout version to handle upgrade paths
    char version_path[PATH_MAX];
    snprintf(version_path, PATH_MAX, "%s.layout_version", android_data_dir.path);

    int oldVersion;
    if (fs_read_atomic_int(version_path, &oldVersion) == -1) {
        oldVersion = 0;
    }
    int version = oldVersion;

    if (version < 2) {
        SLOGD("Assuming that device has multi-user storage layout; upgrade no longer supported");
        version = 2;
    }

    if (ensure_config_user_dirs(0) == -1) {
        SLOGE("Failed to setup misc for user 0");
        goto fail;
    }

    if (version == 2) {
        SLOGD("Upgrading to /data/misc/user directories");

        char misc_dir[PATH_MAX];
        snprintf(misc_dir, PATH_MAX, "%smisc", android_data_dir.path);

        char keychain_added_dir[PATH_MAX];
        snprintf(keychain_added_dir, PATH_MAX, "%s/keychain/cacerts-added", misc_dir);

        char keychain_removed_dir[PATH_MAX];
        snprintf(keychain_removed_dir, PATH_MAX, "%s/keychain/cacerts-removed", misc_dir);

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
                snprintf(misc_added_dir, PATH_MAX, "%s/user/%s/cacerts-added", misc_dir, name);

                char misc_removed_dir[PATH_MAX];
                snprintf(misc_removed_dir, PATH_MAX, "%s/user/%s/cacerts-removed", misc_dir, name);

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