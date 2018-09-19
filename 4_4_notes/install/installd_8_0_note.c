//	frameworks/native/cmds/installd/installd.cpp
int main(const int argc, char *argv[]) {
    return android::installd::installd_main(argc, argv);
}


static int installd_main(const int argc ATTRIBUTE_UNUSED, char *argv[]) {
    int ret;
    int selinux_enabled = (is_selinux_enabled() > 0);

    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv); //system/core/base/logging.cpp

    SLOGI("installd firing up");

    union selinux_callback cb;
    cb.func_log = log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);

	//初始化路径全局变量
    if (!initialize_globals()) {//		frameworks/native/cmds/installd/globals.cpp
        SLOGE("Could not initialize globals; exiting.\n");
        exit(1);
    }

    if (initialize_directories() < 0) {//	frameworks/native/cmds/installd/installd.cpp
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

///////////////////////////////////////////////////////////////////////////////
{
	//分析 android::base::InitLogging(argv); 
	void InitLogging(char* argv[], LogFunction&& logger, AbortFunction&& aborter) {
	  SetLogger(std::forward<LogFunction>(logger));
	  SetAborter(std::forward<AbortFunction>(aborter));

	  if (gInitialized) {
		return;
	  }

	  gInitialized = true;

	  // Stash the command line for later use. We can use /proc/self/cmdline on
	  // Linux to recover this, but we don't have that luxury on the Mac/Windows,
	  // and there are a couple of argv[0] variants that are commonly used.
	  if (argv != nullptr) {
		std::lock_guard<std::mutex> lock(LoggingLock());
		ProgramInvocationName() = basename(argv[0]); //basename系统api，剔除目录和后缀；basename（/proc/self/cmdline.h) -> cmdline
	  }

	  const char* tags = getenv("ANDROID_LOG_TAGS");
	  if (tags == nullptr) {
		return;
	  }

	  std::vector<std::string> specs = Split(tags, " ");
	  for (size_t i = 0; i < specs.size(); ++i) {
		// "tag-pattern:[vdiwefs]"
		std::string spec(specs[i]);
		if (spec.size() == 3 && StartsWith(spec, "*:")) {
		  switch (spec[2]) {
			case 'v':
			  gMinimumLogSeverity = VERBOSE;
			  continue;
			case 'd':
			  gMinimumLogSeverity = DEBUG;
			  continue;
			case 'i':
			  gMinimumLogSeverity = INFO;
			  continue;
			case 'w':
			  gMinimumLogSeverity = WARNING;
			  continue;
			case 'e':
			  gMinimumLogSeverity = ERROR;
			  continue;
			case 'f':
			  gMinimumLogSeverity = FATAL_WITHOUT_ABORT;
			  continue;
			// liblog will even suppress FATAL if you say 's' for silent, but that's
			// crazy!
			case 's':
			  gMinimumLogSeverity = FATAL_WITHOUT_ABORT;
			  continue;
		  }
		}
		LOG(FATAL) << "unsupported '" << spec << "' in ANDROID_LOG_TAGS (" << tags << ")";
	  }
	}


	// Configure logging based on ANDROID_LOG_TAGS environment variable. //配置ANDROID_LOG_TAGS环境变量，需要解析
	// We need to parse a string that looks like
	//
	//      *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
	//
	// The tag (or '*' for the global level) comes first, followed by a colon and a
	// letter indicating the minimum priority level we're expected to log.  This can
	// be used to reveal or conceal logs with specific tags.
	#ifdef __ANDROID__
	#define INIT_LOGGING_DEFAULT_LOGGER LogdLogger()
	#else
	#define INIT_LOGGING_DEFAULT_LOGGER StderrLogger
	#endif
	void InitLogging(char* argv[],LogFunction&& logger = INIT_LOGGING_DEFAULT_LOGGER,AbortFunction&& aborter = DefaultAborter); //默认参数

	//INIT_LOGGING_DEFAULT_LOGGER
	void LogdLogger::operator()(LogId id, LogSeverity severity, const char* tag,const char* file, unsigned int line,const char* message) {
	  static constexpr android_LogPriority kLogSeverityToAndroidLogPriority[] = {
		ANDROID_LOG_VERBOSE, 
		ANDROID_LOG_DEBUG, 
		ANDROID_LOG_INFO,
		ANDROID_LOG_WARN,    
		ANDROID_LOG_ERROR, 
		ANDROID_LOG_FATAL,
		ANDROID_LOG_FATAL,
	  };
	  static_assert(arraysize(kLogSeverityToAndroidLogPriority) == FATAL + 1,"Mismatch in size of kLogSeverityToAndroidLogPriority and values in LogSeverity");

	  int priority = kLogSeverityToAndroidLogPriority[severity];
	  if (id == DEFAULT) {
		id = default_log_id_;
	  }

	  static constexpr log_id kLogIdToAndroidLogId[] = {
		LOG_ID_MAX,
		LOG_ID_MAIN,
		LOG_ID_SYSTEM,
	  };
	  static_assert(arraysize(kLogIdToAndroidLogId) == SYSTEM + 1,"Mismatch in size of kLogIdToAndroidLogId and values in LogId");

	  log_id lg_id = kLogIdToAndroidLogId[id];

	  if (priority == ANDROID_LOG_FATAL) {
		__android_log_buf_print(lg_id, priority, tag, "%s:%u] %s", file, line,message);
	  } else {
		__android_log_buf_print(lg_id, priority, tag, "%s", message);
	  }
	}


	void DefaultAborter(const char* abort_message) {
	#ifdef __ANDROID__
	  android_set_abort_message(abort_message);//	bionic/libc/bionic/android_set_abort_message.cpp
	#else
	  UNUSED(abort_message);
	#endif
	  abort();
	}


	void SetLogger(LogFunction&& logger) {
	  std::lock_guard<std::mutex> lock(LoggingLock());
	  Logger() = std::move(logger);
	}

	void SetAborter(AbortFunction&& aborter) {
	  std::lock_guard<std::mutex> lock(LoggingLock());
	  Aborter() = std::move(aborter);
	}


	static std::mutex& LoggingLock() {
	  //C++11 auto 告诉编译器自行推导类型
	  static auto& logging_lock = *new std::mutex();
	  return logging_lock;
	}

	using LogFunction = std::function<void(LogId, LogSeverity, const char*, const char*,unsigned int, const char*)>;
	using AbortFunction = std::function<void(const char*)>;

	LogdLogger::LogdLogger(LogId default_log_id /*= android::base::MAIN*/) : default_log_id_(default_log_id) {
	}


	static LogFunction& Logger() {
	#ifdef __ANDROID__
	  static auto& logger = *new LogFunction(LogdLogger());
	#else
	  static auto& logger = *new LogFunction(StderrLogger);
	#endif
	  return logger;
	}



	static AbortFunction& Aborter() {
	  static auto& aborter = *new AbortFunction(DefaultAborter);
	  return aborter;
	}

	static std::string& ProgramInvocationName() {
	  static auto& programInvocationName = *new std::string(getprogname());
	  return programInvocationName;
	}

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
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
}




///////////////////////////////////////////////////////////////////////////
{
	//分析  if (!initialize_globals()) {
		
	static bool initialize_globals() {
		const char* data_path = getenv("ANDROID_DATA");//	/data
		if (data_path == nullptr) {
			SLOGE("Could not find ANDROID_DATA");
			return false;
		}
		const char* root_path = getenv("ANDROID_ROOT");//	/system
		if (root_path == nullptr) {
			SLOGE("Could not find ANDROID_ROOT");
			return false;
		}

		return init_globals_from_data_and_root(data_path, root_path);//frameworks/native/cmds/installd/globals.cpp
	}

	//init_globals_from_data_and_root("/data", "/system");
	bool init_globals_from_data_and_root(const char* data, const char* root) {
		// Get the android data directory.
		if (get_path_from_string(&android_data_dir, data) < 0) {//赋值操作 将/data路径保存到android_data_dir中
			return false;
		}

		// Get the android app directory.
		// static constexpr const char* APP_SUBDIR = "app/"; // sub-directory under ANDROID_DATA
		if (copy_and_append(&android_app_dir, &android_data_dir, APP_SUBDIR) < 0) { // android_app_dir 路径为 /data/app/
			return false;
		}

		// Get the android protected app directory.
		//static constexpr const char* PRIVATE_APP_SUBDIR = "app-private/"; // sub-directory under ANDROID_DATA
		if (copy_and_append(&android_app_private_dir, &android_data_dir, PRIVATE_APP_SUBDIR) < 0) {//android_app_private_dir 路径为 /data/app-private/
			return false;
		}

		// Get the android ephemeral app directory.
		//static constexpr const char* EPHEMERAL_APP_SUBDIR = "app-ephemeral/"; // sub-directory under ANDROID_DATA
		if (copy_and_append(&android_app_ephemeral_dir, &android_data_dir, EPHEMERAL_APP_SUBDIR) < 0) {//android_app_ephemeral_dir路径为/data/app-ephemeral/
			return false;
		}

		// Get the android app native library directory.
		//static constexpr const char* APP_LIB_SUBDIR = "app-lib/"; // sub-directory under ANDROID_DATA
		if (copy_and_append(&android_app_lib_dir, &android_data_dir, APP_LIB_SUBDIR) < 0) {//android_app_lib_dir 路径为/data/app-lib/
			return false;
		}

		// Get the sd-card ASEC mount point.
		// frameworks/native/cmds/installd/globals.h:29:static constexpr const char* ASEC_MOUNTPOINT_ENV_NAME = "ASEC_MOUNTPOINT";
		// 这里 ASEC_MOUNTPOINT_ENV_NAME 环境变量对应的值	/mnt/asec
		if (get_path_from_env(&android_asec_dir, ASEC_MOUNTPOINT_ENV_NAME) < 0) {//android_asec_dir 路径为/mnt/asec
			return false;
		}

		// Get the android media directory.
		//static constexpr const char* MEDIA_SUBDIR = "media/"; // sub-directory under ANDROID_DATA
		if (copy_and_append(&android_media_dir, &android_data_dir, MEDIA_SUBDIR) < 0) {//android_media_dir 路径为/data/media/
			return false;
		}

		// Get the android external app directory.
		if (get_path_from_string(&android_mnt_expand_dir, "/mnt/expand/") < 0) {//android_media_dir 路径为/mnt/expand/
			return false;
		}

		// Get the android profiles directory.
		// static constexpr const char* PROFILES_SUBDIR = "misc/profiles"; // sub-directory under ANDROID_DATA
		if (copy_and_append(&android_profiles_dir, &android_data_dir, PROFILES_SUBDIR) < 0) {//android_profiles_dir 路径为/data/misc/profiles
			return false;
		}

		// Take note of the system and vendor directories.
		//	frameworks/native/cmds/installd/globals.cpp:58:dir_rec_array_t android_system_dirs;
		android_system_dirs.count = 4;

		android_system_dirs.dirs = (dir_rec_t*) calloc(android_system_dirs.count, sizeof(dir_rec_t));
		if (android_system_dirs.dirs == NULL) {
			ALOGE("Couldn't allocate array for dirs; aborting\n");
			return false;
		}

		dir_rec_t android_root_dir;
		if (get_path_from_string(&android_root_dir, root) < 0) { //android_root_dir 路径为/system
			return false;
		}

		android_system_dirs.dirs[0].path = build_string2(android_root_dir.path, APP_SUBDIR);// /system/app
		android_system_dirs.dirs[0].len = strlen(android_system_dirs.dirs[0].path);

		android_system_dirs.dirs[1].path = build_string2(android_root_dir.path, PRIV_APP_SUBDIR);// /system/priv-app/
		android_system_dirs.dirs[1].len = strlen(android_system_dirs.dirs[1].path);

		android_system_dirs.dirs[2].path = strdup("/vendor/app/");
		android_system_dirs.dirs[2].len = strlen(android_system_dirs.dirs[2].path);

		android_system_dirs.dirs[3].path = strdup("/oem/app/");
		android_system_dirs.dirs[3].len = strlen(android_system_dirs.dirs[3].path);

		return true;
	}


	/* data structures */

	struct dir_rec_t {
		char* path;
		size_t len;
	};
	
	struct dir_rec_array_t {
		size_t count;
		dir_rec_t* dirs;
	};

	/**
	 * Puts the string into the record as a directory. Appends '/' to the end
	 * of all paths. Caller owns the string that is inserted into the directory
	 * record. A null value will result in an error.
	 *
	 * Returns 0 on success and -1 on error.
	 */
	int get_path_from_string(dir_rec_t* rec, const char* path) {
		if (path == NULL) {
			return -1;
		} else {
			const size_t path_len = strlen(path);
			if (path_len <= 0) {
				return -1;
			}

			// Make sure path is absolute.
			if (path[0] != '/') {
				return -1;
			}

			if (path[path_len - 1] == '/') {
				// Path ends with a forward slash. Make our own copy.

				rec->path = strdup(path);
				if (rec->path == NULL) {
					return -1;
				}

				rec->len = path_len;
			} else {
				// Path does not end with a slash. Generate a new string.
				char *dst;

				// Add space for slash and terminating null.
				size_t dst_size = path_len + 2;

				rec->path = (char*) malloc(dst_size);
				if (rec->path == NULL) {
					return -1;
				}

				dst = rec->path;

		   // append_and_increment frameworks/native/cmds/installd/utils.cpp
				if (append_and_increment(&dst, path, &dst_size) < 0 || append_and_increment(&dst, "/", &dst_size)) {
					ALOGE("Error canonicalizing path");
					return -1;
				}

				rec->len = dst - rec->path;
			}
		}
		return 0;
	}

	//赋值操作
	int append_and_increment(char** dst, const char* src, size_t* dst_size) {
		ssize_t ret = strlcpy(*dst, src, *dst_size);
		if (ret < 0 || (size_t) ret >= *dst_size) {
			return -1;
		}
		*dst += ret;
		*dst_size -= ret;
		return 0;
	}

	//追加赋值
	int copy_and_append(dir_rec_t* dst, const dir_rec_t* src, const char* suffix) {
		dst->len = src->len + strlen(suffix);
		const size_t dstSize = dst->len + 1;
		dst->path = (char*) malloc(dstSize);

		if (dst->path == NULL || snprintf(dst->path, dstSize, "%s%s", src->path, suffix) != (ssize_t) dst->len) {
			ALOGE("Could not allocate memory to hold appended path; aborting\n");
			return -1;
		}

		return 0;
	}
	
	/**
	* Get the contents of a environment variable that contains a path. Caller
	* owns the string that is inserted into the directory record. Returns
	* 0 on success and -1 on error.
	* 获取var 环境变量值并赋值给rec
	*/
	int get_path_from_env(dir_rec_t* rec, const char* var) {
		const char* path = getenv(var);
		int ret = get_path_from_string(rec, path);
		if (ret < 0) {
			ALOGW("Problem finding value for environment variable %s\n", var);
		}
		return ret;
	}
	
	
	char *build_string2(const char *s1, const char *s2) {
		if (s1 == NULL || s2 == NULL) return NULL;

		int len_s1 = strlen(s1);
		int len_s2 = strlen(s2);
		int len = len_s1 + len_s2 + 1;
		char *result = (char *) malloc(len);
		if (result == NULL) return NULL;
		
		strcpy(result, s1);
		strcpy(result + len_s1, s2);

		return result;
	}
}

//	frameworks/native/cmds/installd/utils.cpp

/////////////////////////////////////////////
{//分析  if (initialize_directories() < 0) {
	
	static int initialize_directories() {
		int res = -1;

		// Read current filesystem layout version to handle upgrade paths
		char version_path[PATH_MAX];
		snprintf(version_path, PATH_MAX, "%s.layout_version", android_data_dir.path);// version_path = "/data.layout_version"

		int oldVersion;
		if (fs_read_atomic_int(version_path, &oldVersion) == -1) {//system/core/libcutils/fs.c
			oldVersion = 0;
		}
		int version = oldVersion;

		if (version < 2) {
			SLOGD("Assuming that device has multi-user storage layout; upgrade no longer supported");
			version = 2;
		}

		//	frameworks/native/cmds/installd/utils.cpp
		if (ensure_config_user_dirs(0) == -1) {  //配置用户目录/data/misc/user/0以及修改该目录权限
			SLOGE("Failed to setup misc for user 0");
			goto fail;
		}

		if (version == 2) {
			SLOGD("Upgrading to /data/misc/user directories");

			char misc_dir[PATH_MAX];
			snprintf(misc_dir, PATH_MAX, "%smisc", android_data_dir.path);//	misc_dir = /data/misc

			char keychain_added_dir[PATH_MAX];
			snprintf(keychain_added_dir, PATH_MAX, "%s/keychain/cacerts-added", misc_dir);// keychain_added_dir = /data/misc/keychain/cacerts-added

			char keychain_removed_dir[PATH_MAX];
			snprintf(keychain_removed_dir, PATH_MAX, "%s/keychain/cacerts-removed", misc_dir);// keychain_added_dir = /data/misc/keychain/cacerts-removed

			DIR *dir;
			struct dirent *dirent;
			dir = opendir("/data/user");
			if (dir != NULL) {
				while ((dirent = readdir(dir))) { //该目录下只有一个链接目录0指向/data/data/
					const char *name = dirent->d_name;

					// skip "." and ".."
					if (name[0] == '.') {
						if (name[1] == 0) continue;
						if ((name[1] == '.') && (name[2] == 0)) continue;
					}

					uint32_t user_id = atoi(name);	// user_id == 0

					// /data/misc/user/<user_id> 
					if (ensure_config_user_dirs(user_id) == -1) {
						goto fail;
					}

					char misc_added_dir[PATH_MAX]; //	/data/misc/user/0/cacerts-added
					snprintf(misc_added_dir, PATH_MAX, "%s/user/%s/cacerts-added", misc_dir, name); 

					char misc_removed_dir[PATH_MAX];//	/data/misc/user/0/cacerts-removed
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
	
	
	int fs_read_atomic_int(const char* path, int* out_value) {
		int fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY));
		if (fd == -1) {
			ALOGE("Failed to read %s: %s", path, strerror(errno));
			return -1;
		}

		char buf[BUF_SIZE];
		if (TEMP_FAILURE_RETRY(read(fd, buf, BUF_SIZE)) == -1) {
			ALOGE("Failed to read %s: %s", path, strerror(errno));
			goto fail;
		}
		
		if (sscanf(buf, "%d", out_value) != 1) {
			ALOGE("Failed to parse %s: %s", path, strerror(errno));
			goto fail;
		}
		
		close(fd);
		return 0;

	fail:
		close(fd);
		*out_value = -1;
		return -1;
	}
	
	//userid = 0
	int ensure_config_user_dirs(userid_t userid) {
		// writable by system, readable by any app within the same user
		const int uid = multiuser_get_uid(userid, AID_SYSTEM);//system/core/libcutils/multiuser.c
		const int gid = multiuser_get_uid(userid, AID_EVERYBODY);

		// Ensure /data/misc/user/<userid> exists
		auto path = create_data_misc_legacy_path(userid);//	返回 /data/misc/user/0	 frameworks/native/cmds/installd/utils.cpp
		return fs_prepare_dir(path.c_str(), 0750, uid, gid);//设置权限  //	system/core/libcutils/fs.c
	}
	
	uid_t multiuser_get_uid(userid_t user_id, appid_t app_id) {
		return (user_id * AID_USER_OFFSET) + (app_id % AID_USER_OFFSET);//#define AID_USER_OFFSET 100000 /* offset for uid ranges for each user */
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
		
	
	int fs_prepare_dir(const char* path, mode_t mode, uid_t uid, gid_t gid) {
		return fs_prepare_path_impl(path, mode, uid, gid, /*allow_fixup*/ 1, /*prepare_as_dir*/ 1);
	}
	
	static int fs_prepare_path_impl(const char* path, mode_t mode, uid_t uid, gid_t gid,int allow_fixup, int prepare_as_dir) {
		// Check if path needs to be created
		struct stat sb;
		int create_result = -1;
		if (TEMP_FAILURE_RETRY(lstat(path, &sb)) == -1) {
			if (errno == ENOENT) {
				goto create;
			} else {
				ALOGE("Failed to lstat(%s): %s", path, strerror(errno));
				return -1;
			}
		}

		// Exists, verify status
		int type_ok = prepare_as_dir ? S_ISDIR(sb.st_mode) : S_ISREG(sb.st_mode);
		if (!type_ok) {
			ALOGE("Not a %s: %s", (prepare_as_dir ? "directory" : "regular file"), path);
			return -1;
		}

		int owner_match = ((sb.st_uid == uid) && (sb.st_gid == gid));
		int mode_match = ((sb.st_mode & ALL_PERMS) == mode);
		if (owner_match && mode_match) {
			return 0;
		} else if (allow_fixup) {
			goto fixup;
		} else {
			if (!owner_match) {
				ALOGE("Expected path %s with owner %d:%d but found %d:%d",path, uid, gid, sb.st_uid, sb.st_gid);
				return -1;
			} else {
				ALOGW("Expected path %s with mode %o but found %o",path, mode, (sb.st_mode & ALL_PERMS));
				return 0;
			}
		}

	create:
		create_result = prepare_as_dir
			? TEMP_FAILURE_RETRY(mkdir(path, mode))
			: TEMP_FAILURE_RETRY(open(path, O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_RDONLY, 0644));
		if (create_result == -1) {
			if (errno != EEXIST) {
				ALOGE("Failed to %s(%s): %s",
						(prepare_as_dir ? "mkdir" : "open"), path, strerror(errno));
				return -1;
			}
		} else if (!prepare_as_dir) {
			// For regular files we need to make sure we close the descriptor
			if (close(create_result) == -1) {
				ALOGW("Failed to close file after create %s: %s", path, strerror(errno));
			}
		}
	fixup:
		if (TEMP_FAILURE_RETRY(chmod(path, mode)) == -1) {
			ALOGE("Failed to chmod(%s, %d): %s", path, mode, strerror(errno));
			return -1;
		}
		if (TEMP_FAILURE_RETRY(chown(path, uid, gid)) == -1) {
			ALOGE("Failed to chown(%s, %d, %d): %s", path, uid, gid, strerror(errno));
			return -1;
		}

		return 0;
	}
	
	
	
	
}



/////////////////////////////////////////////
//分析   if (selinux_enabled && selinux_status_open(true) < 0) {



/////////////////////////////////////////////
//分析   if ((ret = InstalldNativeService::start()) != android::OK) {
{
	status_t InstalldNativeService::start() {
		IPCThreadState::self()->disableBackgroundScheduling(true); //禁止线程后台执行
		status_t ret = BinderService<InstalldNativeService>::publish();//将InstalldNativeService加入到ServiceManager管理中
		if (ret != android::OK) {
			return ret;
		}
		sp<ProcessState> ps(ProcessState::self());
		ps->startThreadPool();//启动一个异步线程线程，名称为Binder:272_1
		ps->giveThreadPoolName();//设置主线程名称应该为Binder:272_2
		return android::OK;
	}
}



//////////////////////////////////////////////////////////////
//分析 IPCThreadState::self()->joinThreadPool();
{
	void IPCThreadState::joinThreadPool(bool isMain)
	{
		LOG_THREADPOOL("**** THREAD %p (PID %d) IS JOINING THE THREAD POOL\n", (void*)pthread_self(), getpid());

		mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);

		status_t result;
		do {
			processPendingDerefs();
			// now get the next command to be processed, waiting if necessary
			result = getAndExecuteCommand();

			if (result < NO_ERROR && result != TIMED_OUT && result != -ECONNREFUSED && result != -EBADF) {
				ALOGE("getAndExecuteCommand(fd=%d) returned unexpected error %d, aborting",mProcess->mDriverFD, result);
				abort();
			}

			// Let this thread exit the thread pool if it is no longer
			// needed and it is not the main process thread.
			if(result == TIMED_OUT && !isMain) {
				break;
			}
		} while (result != -ECONNREFUSED && result != -EBADF);

		LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%d\n",(void*)pthread_self(), getpid(), result);

		mOut.writeInt32(BC_EXIT_LOOPER);
		talkWithDriver(false);
	}
	
	// When we've cleared the incoming command queue, process any pending derefs
	void IPCThreadState::processPendingDerefs()
	{
		if (mIn.dataPosition() >= mIn.dataSize()) {
			size_t numPending = mPendingWeakDerefs.size();
			if (numPending > 0) {
				for (size_t i = 0; i < numPending; i++) {
					RefBase::weakref_type* refs = mPendingWeakDerefs[i];
					refs->decWeak(mProcess.get());
				}
				mPendingWeakDerefs.clear();
			}

			numPending = mPendingStrongDerefs.size();
			if (numPending > 0) {
				for (size_t i = 0; i < numPending; i++) {
					BBinder* obj = mPendingStrongDerefs[i];
					obj->decStrong(mProcess.get());
				}
				mPendingStrongDerefs.clear();
			}
		}
	}
	
	status_t IPCThreadState::getAndExecuteCommand()
	{
		status_t result;
		int32_t cmd;

		result = talkWithDriver();
		if (result >= NO_ERROR) {
			size_t IN = mIn.dataAvail();
			if (IN < sizeof(int32_t)) return result;
			
			cmd = mIn.readInt32();
			IF_LOG_COMMANDS() {
				alog << "Processing top-level Command: "  << getReturnString(cmd) << endl;
			}

			pthread_mutex_lock(&mProcess->mThreadCountLock);
			mProcess->mExecutingThreadsCount++;
			if (mProcess->mExecutingThreadsCount >= mProcess->mMaxThreads && mProcess->mStarvationStartTimeMs == 0) {
				mProcess->mStarvationStartTimeMs = uptimeMillis();
			}
			pthread_mutex_unlock(&mProcess->mThreadCountLock);

			result = executeCommand(cmd);

			pthread_mutex_lock(&mProcess->mThreadCountLock);
			mProcess->mExecutingThreadsCount--;
			if (mProcess->mExecutingThreadsCount < mProcess->mMaxThreads && mProcess->mStarvationStartTimeMs != 0) {
				int64_t starvationTimeMs = uptimeMillis() - mProcess->mStarvationStartTimeMs;
				if (starvationTimeMs > 100) {
					ALOGE("binder thread pool (%zu threads) starved for %" PRId64 " ms", mProcess->mMaxThreads, starvationTimeMs);
				}
				mProcess->mStarvationStartTimeMs = 0;
			}
			pthread_cond_broadcast(&mProcess->mThreadCountDecrement);
			pthread_mutex_unlock(&mProcess->mThreadCountLock);
		}

		return result;
	}
	
	status_t IPCThreadState::executeCommand(int32_t cmd)
	{
		BBinder* obj;
		RefBase::weakref_type* refs;
		status_t result = NO_ERROR;

		switch ((uint32_t)cmd) {
		case BR_ERROR:
			result = mIn.readInt32();
			break;

		case BR_OK:
			break;

		case BR_ACQUIRE:
			refs = (RefBase::weakref_type*)mIn.readPointer();
			obj = (BBinder*)mIn.readPointer();
			ALOG_ASSERT(refs->refBase() == obj,
					   "BR_ACQUIRE: object %p does not match cookie %p (expected %p)",
					   refs, obj, refs->refBase());
			obj->incStrong(mProcess.get());
			IF_LOG_REMOTEREFS() {
				LOG_REMOTEREFS("BR_ACQUIRE from driver on %p", obj);
				obj->printRefs();
			}
			mOut.writeInt32(BC_ACQUIRE_DONE);
			mOut.writePointer((uintptr_t)refs);
			mOut.writePointer((uintptr_t)obj);
			break;

		case BR_RELEASE:
			refs = (RefBase::weakref_type*)mIn.readPointer();
			obj = (BBinder*)mIn.readPointer();
			ALOG_ASSERT(refs->refBase() == obj,"BR_RELEASE: object %p does not match cookie %p (expected %p)",refs, obj, refs->refBase());
			IF_LOG_REMOTEREFS() {
				LOG_REMOTEREFS("BR_RELEASE from driver on %p", obj);
				obj->printRefs();
			}
			mPendingStrongDerefs.push(obj);
			break;

		case BR_INCREFS:
			refs = (RefBase::weakref_type*)mIn.readPointer();
			obj = (BBinder*)mIn.readPointer();
			refs->incWeak(mProcess.get());
			mOut.writeInt32(BC_INCREFS_DONE);
			mOut.writePointer((uintptr_t)refs);
			mOut.writePointer((uintptr_t)obj);
			break;

		case BR_DECREFS:
			refs = (RefBase::weakref_type*)mIn.readPointer();
			obj = (BBinder*)mIn.readPointer();
			// NOTE: This assertion is not valid, because the object may no
			// longer exist (thus the (BBinder*)cast above resulting in a different
			// memory address).
			//ALOG_ASSERT(refs->refBase() == obj,
			//           "BR_DECREFS: object %p does not match cookie %p (expected %p)",
			//           refs, obj, refs->refBase());
			mPendingWeakDerefs.push(refs);
			break;

		case BR_ATTEMPT_ACQUIRE:
			refs = (RefBase::weakref_type*)mIn.readPointer();
			obj = (BBinder*)mIn.readPointer();

			{
				const bool success = refs->attemptIncStrong(mProcess.get());
				ALOG_ASSERT(success && refs->refBase() == obj,
						   "BR_ATTEMPT_ACQUIRE: object %p does not match cookie %p (expected %p)",
						   refs, obj, refs->refBase());

				mOut.writeInt32(BC_ACQUIRE_RESULT);
				mOut.writeInt32((int32_t)success);
			}
			break;

		case BR_TRANSACTION:
			{
				binder_transaction_data tr;
				result = mIn.read(&tr, sizeof(tr));
				ALOG_ASSERT(result == NO_ERROR,
					"Not enough command data for brTRANSACTION");
				if (result != NO_ERROR) break;

				Parcel buffer;
				buffer.ipcSetDataReference(
					reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
					tr.data_size,
					reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
					tr.offsets_size/sizeof(binder_size_t), freeBuffer, this);

				const pid_t origPid = mCallingPid;
				const uid_t origUid = mCallingUid;
				const int32_t origStrictModePolicy = mStrictModePolicy;
				const int32_t origTransactionBinderFlags = mLastTransactionBinderFlags;

				mCallingPid = tr.sender_pid;
				mCallingUid = tr.sender_euid;
				mLastTransactionBinderFlags = tr.flags;

				//ALOGI(">>>> TRANSACT from pid %d uid %d\n", mCallingPid, mCallingUid);

				Parcel reply;
				status_t error;
				IF_LOG_TRANSACTIONS() {
					TextOutput::Bundle _b(alog);
					alog << "BR_TRANSACTION thr " << (void*)pthread_self()
						<< " / obj " << tr.target.ptr << " / code "
						<< TypeCode(tr.code) << ": " << indent << buffer
						<< dedent << endl
						<< "Data addr = "
						<< reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer)
						<< ", offsets addr="
						<< reinterpret_cast<const size_t*>(tr.data.ptr.offsets) << endl;
				}
				if (tr.target.ptr) {
					// We only have a weak reference on the target object, so we must first try to
					// safely acquire a strong reference before doing anything else with it.
					if (reinterpret_cast<RefBase::weakref_type*>(
							tr.target.ptr)->attemptIncStrong(this)) {
						error = reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,
								&reply, tr.flags);
						reinterpret_cast<BBinder*>(tr.cookie)->decStrong(this);
					} else {
						error = UNKNOWN_TRANSACTION;
					}

				} else {
					error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
				}

				//ALOGI("<<<< TRANSACT from pid %d restore pid %d uid %d\n",
				//     mCallingPid, origPid, origUid);

				if ((tr.flags & TF_ONE_WAY) == 0) {
					LOG_ONEWAY("Sending reply to %d!", mCallingPid);
					if (error < NO_ERROR) reply.setError(error);
					sendReply(reply, 0);
				} else {
					LOG_ONEWAY("NOT sending reply to %d!", mCallingPid);
				}

				mCallingPid = origPid;
				mCallingUid = origUid;
				mStrictModePolicy = origStrictModePolicy;
				mLastTransactionBinderFlags = origTransactionBinderFlags;

				IF_LOG_TRANSACTIONS() {
					TextOutput::Bundle _b(alog);
					alog << "BC_REPLY thr " << (void*)pthread_self() << " / obj "
						<< tr.target.ptr << ": " << indent << reply << dedent << endl;
				}

			}
			break;

		case BR_DEAD_BINDER:
			{
				BpBinder *proxy = (BpBinder*)mIn.readPointer();
				proxy->sendObituary();
				mOut.writeInt32(BC_DEAD_BINDER_DONE);
				mOut.writePointer((uintptr_t)proxy);
			} break;

		case BR_CLEAR_DEATH_NOTIFICATION_DONE:
			{
				BpBinder *proxy = (BpBinder*)mIn.readPointer();
				proxy->getWeakRefs()->decWeak(proxy);
			} break;

		case BR_FINISHED:
			result = TIMED_OUT;
			break;

		case BR_NOOP:
			break;

		case BR_SPAWN_LOOPER:
			mProcess->spawnPooledThread(false);
			break;

		default:
			ALOGE("*** BAD COMMAND %d received from Binder driver\n", cmd);
			result = UNKNOWN_ERROR;
			break;
		}

		if (result != NO_ERROR) {
			mLastError = result;
		}

		return result;
	}
	
	
	status_t IPCThreadState::talkWithDriver(bool doReceive /*= true*/)
	{
		if (mProcess->mDriverFD <= 0) {
			return -EBADF;
		}

		binder_write_read bwr;

		// Is the read buffer empty?
		const bool needRead = mIn.dataPosition() >= mIn.dataSize();

		// We don't want to write anything if we are still reading
		// from data left in the input buffer and the caller
		// has requested to read the next data.
		const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;

		bwr.write_size = outAvail;
		bwr.write_buffer = (uintptr_t)mOut.data();

		// This is what we'll read.
		if (doReceive && needRead) {
			bwr.read_size = mIn.dataCapacity();
			bwr.read_buffer = (uintptr_t)mIn.data();
		} else {
			bwr.read_size = 0;
			bwr.read_buffer = 0;
		}

		IF_LOG_COMMANDS() {
			TextOutput::Bundle _b(alog);
			if (outAvail != 0) {
				alog << "Sending commands to driver: " << indent;
				const void* cmds = (const void*)bwr.write_buffer;
				const void* end = ((const uint8_t*)cmds)+bwr.write_size;
				alog << HexDump(cmds, bwr.write_size) << endl;
				while (cmds < end) cmds = printCommand(alog, cmds);
				alog << dedent;
			}
			alog << "Size of receive buffer: " << bwr.read_size << ", needRead: " << needRead << ", doReceive: " << doReceive << endl;
		}

		// Return immediately if there is nothing to do.
		if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

		bwr.write_consumed = 0;
		bwr.read_consumed = 0;
		status_t err;
		do {
			IF_LOG_COMMANDS() {
				alog << "About to read/write, write size = " << mOut.dataSize() << endl;
			}
	#if defined(__ANDROID__)
			if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
				err = NO_ERROR;
			else
				err = -errno;
	#else
			err = INVALID_OPERATION;
	#endif
			if (mProcess->mDriverFD <= 0) {
				err = -EBADF;
			}
			IF_LOG_COMMANDS() {
				alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
			}
		} while (err == -EINTR);

		IF_LOG_COMMANDS() {
			alog << "Our err: " << (void*)(intptr_t)err << ", write consumed: " << bwr.write_consumed << " (of " << mOut.dataSize()
							<< "), read consumed: " << bwr.read_consumed << endl;
		}

		if (err >= NO_ERROR) {
			if (bwr.write_consumed > 0) {
				if (bwr.write_consumed < mOut.dataSize())
					mOut.remove(0, bwr.write_consumed);
				else
					mOut.setDataSize(0);
			}
			if (bwr.read_consumed > 0) {
				mIn.setDataSize(bwr.read_consumed);
				mIn.setDataPosition(0);
			}
			IF_LOG_COMMANDS() {
				TextOutput::Bundle _b(alog);
				alog << "Remaining data size: " << mOut.dataSize() << endl;
				alog << "Received commands from driver: " << indent;
				const void* cmds = mIn.data();
				const void* end = mIn.data() + mIn.dataSize();
				alog << HexDump(cmds, mIn.dataSize()) << endl;
				while (cmds < end) cmds = printReturnCommand(alog, cmds);
				alog << dedent;
			}
			return NO_ERROR;
		}

		return err;
	}
}
        




