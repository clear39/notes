
------Security Context-----         		--------Security Server----------
|Mac Permission		   |			| PackageManagerService		 |
|File Context		   |			| Zygote 	init 	installd |
|Property Context          |
|App Context               |
|--------------------------|


-----------------------------------libselinux---------------------------
					|
				    read/write
					|
-------------------------------SELinux File System---------------------



--------------------------------SELinux LSM Module--------------------- 			Kernel Space



以SELinux文件系统接口为边界，SEAndroid安全机制包含有内核空间和用户空间两部分支持。
在内核空间，主要涉及到一个称为SELinux LSM的模块。
而在用户空间中，涉及到Security Context、Security Server和SEAndroid Policy等模块


1. 内核空间的SELinux LSM(Linux Security Model)模块负责内核资源的安全访问控制。
2. 用户空间的SEAndroid Policy描述的是资源安全访问策略。系统在启动的时候，用户空间的Security Server需要将这些安全访问策略加载内核空间的SELinux LSM模块中去。这是通过SELinux文件系统接口实现的。
3. 用户空间的Security Context描述的是资源安全上下文。SEAndroid的安全访问策略就是在资源的安全上下文基础上实现的。
4. 用户空间的Security Server一方面需要到用户空间的Security Context去检索对象的安全上下文，另一方面也需要到内核空间去操作对象的安全上下文。
5. 用户空间的selinux库封装了对SELinux文件系统接口的读写操作。用户空间的Security Server访问内核空间的SELinux LSM模块时，都是间接地通过selinux进行的。这样可以将对SELinux文件系统接口的读写操作封装成更有意义的函数调用。
6. 用户空间的Security Server到用户空间的Security Context去检索对象的安全上下文时，同样也是通过selinux库来进行的。

 一. 内核空间
 在内核空间中，存在一个SELinux LSM模块，这个模块包含:
有一个访问向量缓冲（Access Vector Cache）
一个安全服务（Security Server）




 二. 用户空间
 在用户空间中，SEAndroid包含有三个主要的模块:
	安全上下文（Security Context）
	安全策略（SEAndroid Policy）
	安全服务（Security Server）




int main(int argc, char** argv) {
    。。。。。。

    bool is_first_stage = (getenv("INIT_SECOND_STAGE") == nullptr);

    if (is_first_stage) {
 	。。。。。。
        mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL);

	。。。。。。

        // Set up SELinux, loading the SELinux policy.
        selinux_initialize(true);  //分析一

        // We're in the kernel domain, so re-exec init to transition to the init domain now
        // that the SELinux policy has been loaded.
	// external/selinux/libselinux/src/android/android_platform.c
        if (selinux_android_restorecon("/init", 0) == -1) {//分析二
            PLOG(ERROR) << "restorecon failed";
            security_failure();
        }

        setenv("INIT_SECOND_STAGE", "true", 1);

        。。。。。。
        security_failure();
    }

    。。。。。。

    // Now set up SELinux for second stage.
    selinux_initialize(false);
    selinux_restore_context();

    。。。。。。

    return 0;
}

//分析一
static void selinux_initialize(bool in_kernel_domain) {
    Timer t;


    //	external/selinux/libselinux/include/selinux/selinux.h
    selinux_callback cb;

    //	external/selinux/libselinux/src/callbacks.c
    //  设置回调函数
    cb.func_log = selinux_klog_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);
    cb.func_audit = audit_callback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);

    if (in_kernel_domain) {
        LOG(INFO) << "Loading SELinux policy";
        //	system/core/init/init.cpp  // 将 /sepolicy 文件内容写入到 /sys/fs/selinux/load 中
        if (!selinux_load_policy()) {
            panic();
        }

	// external/selinux/libselinux/src/getenforce.c   //	读取/sys/fs/selinux/enforce
        bool kernel_enforcing = (security_getenforce() == 1); // 这里 security_getenforce() 返回值为0
        bool is_enforcing = selinux_is_enforcing(); // 这里 is_enforcing 为 false
        if (kernel_enforcing != is_enforcing) {
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
//	system/core/init/init.cpp
static bool selinux_load_policy() {
    //这里执行selinux_load_monolithic_policy()，selinux_load_split_policy()以后在分析
    return selinux_is_split_policy_device() ? selinux_load_split_policy() : selinux_load_monolithic_policy();
}


//	返回false
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
    if (selinux_android_load_policy() < 0) {
        PLOG(ERROR) << "Failed to load monolithic SELinux policy";
        return false;
    }
    return true;
}



int selinux_android_load_policy()
{
	int fd = -1;
        //	static const char *const sepolicy_file = "/sepolicy";
	fd = open(sepolicy_file, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0) {
		selinux_log(SELINUX_ERROR, "SELinux:  Could not open %s:  %s\n",sepolicy_file, strerror(errno));
		return -1;
	}
	int ret = selinux_android_load_policy_from_fd(fd, sepolicy_file);
	close(fd);
	return ret;
}


int selinux_android_load_policy_from_fd(int fd, const char *description)
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
	selinux_mnt = strdup(mnt);
}


int security_load_policy(void *data, size_t len)
{
	char path[PATH_MAX];
	int fd, ret;

	if (!selinux_mnt) {
		errno = ENOENT;
		return -1;
	}

	snprintf(path, sizeof path, "%s/load", selinux_mnt);
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

	snprintf(path, sizeof path, "%s/enforce", selinux_mnt);
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
    if (ALLOW_PERMISSIVE_SELINUX) {
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


//分析二
// if (selinux_android_restorecon("/init", 0) == -1)
int selinux_android_restorecon(const char *file, unsigned int flags)
{
    return selinux_android_restorecon_common(file, NULL, -1, flags);
}

static int selinux_android_restorecon_common(const char* pathname_orig,const char *seinfo,uid_t uid,unsigned int flags)
{
    bool nochange = (flags & SELINUX_ANDROID_RESTORECON_NOCHANGE) ? true : false;
    bool verbose = (flags & SELINUX_ANDROID_RESTORECON_VERBOSE) ? true : false;
    bool recurse = (flags & SELINUX_ANDROID_RESTORECON_RECURSE) ? true : false;
    bool force = (flags & SELINUX_ANDROID_RESTORECON_FORCE) ? true : false;
    bool datadata = (flags & SELINUX_ANDROID_RESTORECON_DATADATA) ? true : false;
    bool skipce = (flags & SELINUX_ANDROID_RESTORECON_SKIPCE) ? true : false;
    bool cross_filesystems = (flags & SELINUX_ANDROID_RESTORECON_CROSS_FILESYSTEMS) ? true : false;
    bool issys;
    bool setrestoreconlast = true;
    struct stat sb;
    struct statfs sfsb;
    FTS *fts;
    FTSENT *ftsent;
    char *pathname = NULL, *pathdnamer = NULL, *pathdname, *pathbname;
    char * paths[2] = { NULL , NULL };
    int ftsflags = FTS_NOCHDIR | FTS_PHYSICAL;
    int error, sverrno;
    char xattr_value[FC_DIGEST_SIZE];
    ssize_t size;

    if (!cross_filesystems) {  //	cross_filesystems == false
        ftsflags |= FTS_XDEV;
    }

    //返回1； external/selinux/libselinux/src/enabled.c
    if (is_selinux_enabled() <= 0)
        return 0;

    //	external/selinux/libselinux/src/android/android_platform.c:1092:static pthread_once_t fc_once = PTHREAD_ONCE_INIT;
    __selinux_once(fc_once, file_context_init); // file_context_init external/selinux/libselinux/src/android/android_platform.c

    if (!fc_sehandle)
        return 0;

    /*
     * Convert passed-in pathname to canonical pathname by resolving realpath of
     * containing dir, then appending last component name.
     */
    pathbname = basename(pathname_orig);
    if (!strcmp(pathbname, "/") || !strcmp(pathbname, ".") || !strcmp(pathbname, "..")) {
        pathname = realpath(pathname_orig, NULL);
        if (!pathname)
            goto realpatherr;
    } else {
        pathdname = dirname(pathname_orig);
        pathdnamer = realpath(pathdname, NULL);
        if (!pathdnamer)
            goto realpatherr;
        if (!strcmp(pathdnamer, "/"))
            error = asprintf(&pathname, "/%s", pathbname);
        else
            error = asprintf(&pathname, "%s/%s", pathdnamer, pathbname);
        if (error < 0)
            goto oom;
    }

    paths[0] = pathname;
    issys = (!strcmp(pathname, SYS_PATH) || !strncmp(pathname, SYS_PREFIX, sizeof(SYS_PREFIX)-1)) ? true : false;

    if (!recurse) {
        if (lstat(pathname, &sb) < 0) {
            error = -1;
            goto cleanup;
        }

        error = restorecon_sb(pathname, &sb, nochange, verbose, seinfo, uid);
        goto cleanup;
    }

    /*
     * Ignore restorecon_last on /data/data or /data/user
     * since their labeling is based on seapp_contexts and seinfo
     * assignments rather than file_contexts and is managed by
     * installd rather than init.
     */
    if (!strncmp(pathname, DATA_DATA_PREFIX, sizeof(DATA_DATA_PREFIX)-1) ||
        !strncmp(pathname, DATA_USER_PREFIX, sizeof(DATA_USER_PREFIX)-1) ||
        !strncmp(pathname, DATA_USER_DE_PREFIX, sizeof(DATA_USER_DE_PREFIX)-1) ||
        !fnmatch(EXPAND_USER_PATH, pathname, FNM_LEADING_DIR|FNM_PATHNAME) ||
        !fnmatch(EXPAND_USER_DE_PATH, pathname, FNM_LEADING_DIR|FNM_PATHNAME))
        setrestoreconlast = false;

    /* Also ignore on /sys since it is regenerated on each boot regardless. */
    if (issys)
        setrestoreconlast = false;

    /* Ignore files on in-memory filesystems */
    if (statfs(pathname, &sfsb) == 0) {
        if (sfsb.f_type == RAMFS_MAGIC || sfsb.f_type == TMPFS_MAGIC)
            setrestoreconlast = false;
    }

    if (setrestoreconlast) {
        size = getxattr(pathname, RESTORECON_LAST, xattr_value, sizeof fc_digest);
        if (!force && size == sizeof fc_digest && memcmp(fc_digest, xattr_value, sizeof fc_digest) == 0) {
            selinux_log(SELINUX_INFO,"SELinux: Skipping restorecon_recursive(%s)\n",pathname);
            error = 0;
            goto cleanup;
        }
    }

    fts = fts_open(paths, ftsflags, NULL);
    if (!fts) {
        error = -1;
        goto cleanup;
    }

    error = 0;
    while ((ftsent = fts_read(fts)) != NULL) {
        switch (ftsent->fts_info) {
        case FTS_DC:
            selinux_log(SELINUX_ERROR,"SELinux:  Directory cycle on %s.\n", ftsent->fts_path);
            errno = ELOOP;
            error = -1;
            goto out;
        case FTS_DP:
            continue;
        case FTS_DNR:
            selinux_log(SELINUX_ERROR,"SELinux:  Could not read %s: %s.\n", ftsent->fts_path, strerror(errno));
            fts_set(fts, ftsent, FTS_SKIP);
            continue;
        case FTS_NS:
            selinux_log(SELINUX_ERROR,"SELinux:  Could not stat %s: %s.\n", ftsent->fts_path, strerror(errno));
            fts_set(fts, ftsent, FTS_SKIP);
            continue;
        case FTS_ERR:
            selinux_log(SELINUX_ERROR,"SELinux:  Error on %s: %s.\n", ftsent->fts_path, strerror(errno));
            fts_set(fts, ftsent, FTS_SKIP);
            continue;
        case FTS_D:
            if (issys && !selabel_partial_match(fc_sehandle, ftsent->fts_path)) {
                fts_set(fts, ftsent, FTS_SKIP);
                continue;
            }

            if (skipce &&
                (!strncmp(ftsent->fts_path, DATA_SYSTEM_CE_PREFIX, sizeof(DATA_SYSTEM_CE_PREFIX)-1) ||
                 !strncmp(ftsent->fts_path, DATA_MISC_CE_PREFIX, sizeof(DATA_MISC_CE_PREFIX)-1))) {
                // Don't label anything below this directory.
                fts_set(fts, ftsent, FTS_SKIP);
                // but fall through and make sure we label the directory itself
            }

            if (!datadata &&
                (!strcmp(ftsent->fts_path, DATA_DATA_PATH) ||
                 !strncmp(ftsent->fts_path, DATA_USER_PREFIX, sizeof(DATA_USER_PREFIX)-1) ||
                 !strncmp(ftsent->fts_path, DATA_USER_DE_PREFIX, sizeof(DATA_USER_DE_PREFIX)-1) ||
                 !fnmatch(EXPAND_USER_PATH, ftsent->fts_path, FNM_LEADING_DIR|FNM_PATHNAME) ||
                 !fnmatch(EXPAND_USER_DE_PATH, ftsent->fts_path, FNM_LEADING_DIR|FNM_PATHNAME))) {
                // Don't label anything below this directory.
                fts_set(fts, ftsent, FTS_SKIP);
                // but fall through and make sure we label the directory itself
            }
            /* fall through */
        default:
            error |= restorecon_sb(ftsent->fts_path, ftsent->fts_statp, nochange, verbose, seinfo, uid);
            break;
        }
    }

    // Labeling successful. Mark the top level directory as completed.
    if (setrestoreconlast && !nochange && !error)
        setxattr(pathname, RESTORECON_LAST, fc_digest, sizeof fc_digest, 0);

out:
    sverrno = errno;
    (void) fts_close(fts);
    errno = sverrno;
cleanup:
    free(pathdnamer);
    free(pathname);
    return error;
oom:
    sverrno = errno;
    selinux_log(SELINUX_ERROR, "%s:  Out of memory\n", __FUNCTION__);
    errno = sverrno;
    error = -1;
    goto cleanup;
realpatherr:
    sverrno = errno;
    selinux_log(SELINUX_ERROR, "SELinux: Could not get canonical path for %s restorecon: %s.\n",pathname_orig, strerror(errno));
    errno = sverrno;
    error = -1;
    goto cleanup;
}

// 返回1
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


static struct selabel_handle *fc_sehandle = NULL;
static void file_context_init(void)
{
    if (!fc_sehandle)
        fc_sehandle = selinux_android_file_context_handle();
}

/* Structure for passing options, used by AVC and label subsystems */
struct selinux_opt {
	int type;
	const char *value;
};

static const struct selinux_opt seopts_file_split[] = {
    { SELABEL_OPT_PATH, "/system/etc/selinux/plat_file_contexts" },
    { SELABEL_OPT_PATH, "/vendor/etc/selinux/nonplat_file_contexts" }
};

static const struct selinux_opt seopts_file_rootfs[] = {
    { SELABEL_OPT_PATH, "/plat_file_contexts" },
    { SELABEL_OPT_PATH, "/nonplat_file_contexts" }
};

struct selabel_handle* selinux_android_file_context_handle(void)
{
    if (selinux_android_opts_file_exists(seopts_file_split)) {//false
        return selinux_android_file_context(seopts_file_split,ARRAY_SIZE(seopts_file_split));
    } else {
        return selinux_android_file_context(seopts_file_rootfs,ARRAY_SIZE(seopts_file_rootfs));
    }
}


static bool selinux_android_opts_file_exists(const struct selinux_opt *opt)
{
    return (access(opt[0].value, R_OK) != -1);
}


static struct selabel_handle* selinux_android_file_context(const struct selinux_opt *opts,unsigned nopts)
{
    struct selabel_handle *sehandle;
    struct selinux_opt fc_opts[nopts + 1];

    memcpy(fc_opts, opts, nopts*sizeof(struct selinux_opt));
    fc_opts[nopts].type = SELABEL_OPT_BASEONLY;
    fc_opts[nopts].value = (char *)1;

    //	external/selinux/libselinux/include/selinux/label.h:28:#define SELABEL_CTX_FILE	0
    sehandle = selabel_open(SELABEL_CTX_FILE, fc_opts, ARRAY_SIZE(fc_opts));
    if (!sehandle) {
        selinux_log(SELINUX_ERROR, "%s: Error getting file context handle (%s)\n",__FUNCTION__, strerror(errno));
        return NULL;
    }
    if (!compute_file_contexts_hash(fc_digest, opts, nopts)) {
        selabel_close(sehandle);
        return NULL;
    }

    selinux_log(SELINUX_INFO, "SELinux: Loaded file_contexts\n");

    return sehandle;
}


static selabel_initfunc initfuncs[] = {
	CONFIG_FILE_BACKEND(selabel_file_init), //external/selinux/libselinux/src/label_file.c 
	CONFIG_MEDIA_BACKEND(selabel_media_init),
	CONFIG_X_BACKEND(selabel_x_init),
	CONFIG_DB_BACKEND(selabel_db_init),
	CONFIG_ANDROID_BACKEND(selabel_property_init),
	CONFIG_ANDROID_BACKEND(selabel_service_init),
};

struct selabel_handle *selabel_open(unsigned int backend,const struct selinux_opt *opts,unsigned nopts)
{
	struct selabel_handle *rec = NULL;

	if (backend >= ARRAY_SIZE(initfuncs)) {
		errno = EINVAL;
		goto out;
	}

	if (!initfuncs[backend]) {
		errno = ENOTSUP;
		goto out;
	}

	rec = (struct selabel_handle *)malloc(sizeof(*rec));
	if (!rec)
		goto out;

	memset(rec, 0, sizeof(*rec));
	rec->backend = backend;
	rec->validating = selabel_is_validate_set(opts, nopts);

	rec->subs = NULL;
	rec->dist_subs = NULL;
	rec->digest = selabel_is_digest_set(opts, nopts, rec->digest);

	if ((*initfuncs[backend])(rec, opts, nopts)) {
		selabel_close(rec);
		rec = NULL;
	}
out:
	return rec;
}


int selabel_file_init(struct selabel_handle *rec,const struct selinux_opt *opts,unsigned nopts)
{
	struct saved_data *data;

	data = (struct saved_data *)malloc(sizeof(*data));
	if (!data)
		return -1;
	memset(data, 0, sizeof(*data));

	rec->data = data;
	rec->func_close = &closef;
	rec->func_stats = &stats;
	rec->func_lookup = &lookup;
	rec->func_partial_match = &partial_match;
	rec->func_lookup_best_match = &lookup_best_match;
	rec->func_cmp = &cmp;

	return init(rec, opts, nopts);
}

static int init(struct selabel_handle *rec, const struct selinux_opt *opts,unsigned n)
{
	struct saved_data *data = (struct saved_data *)rec->data;
	size_t num_paths = 0;
	char **path = NULL;
	const char *prefix = NULL;
	int status = -1;
	size_t i;
	bool baseonly = false;
	bool path_provided;

	/* Process arguments */
	i = n;
	while (i--)
		switch(opts[i].type) {
		case SELABEL_OPT_PATH:
			num_paths++;
			break;
		case SELABEL_OPT_SUBSET:
			prefix = opts[i].value;
			break;
		case SELABEL_OPT_BASEONLY:
			baseonly = !!opts[i].value;
			break;
		}

	if (!num_paths) {
		num_paths = 1;
		path_provided = false;
	} else {
		path_provided = true;
	}

	path = calloc(num_paths, sizeof(*path));
	if (path == NULL) {
		goto finish;
	}
	rec->spec_files = path;
	rec->spec_files_len = num_paths;

	if (path_provided) {
		for (i = 0; i < n; i++) {
			switch(opts[i].type) {
			case SELABEL_OPT_PATH:
				*path = strdup(opts[i].value);
				if (*path == NULL)
					goto finish;
				path++;
				break;
			default:
				break;
			}
		}
	}
#if !defined(BUILD_HOST) && !defined(ANDROID)
	char subs_file[PATH_MAX + 1];
	/* Process local and distribution substitution files */
	if (!path_provided) {
		rec->dist_subs = selabel_subs_init(selinux_file_context_subs_dist_path(),rec->dist_subs, rec->digest);
		rec->subs = selabel_subs_init(selinux_file_context_subs_path(),rec->subs, rec->digest);
		rec->spec_files[0] = strdup(selinux_file_context_path());
		if (rec->spec_files[0] == NULL)
			goto finish;
	} else {
		for (i = 0; i < num_paths; i++) {
			snprintf(subs_file, sizeof(subs_file), "%s.subs_dist", rec->spec_files[i]);
			rec->dist_subs = selabel_subs_init(subs_file, rec->dist_subs, rec->digest);
			snprintf(subs_file, sizeof(subs_file), "%s.subs", rec->spec_files[i]);
			rec->subs = selabel_subs_init(subs_file, rec->subs, rec->digest);
		}
	}
#else
	if (!path_provided) {
		selinux_log(SELINUX_ERROR, "No path given to file labeling backend\n");
		goto finish;
	}
#endif

	/*
	 * Do detailed validation of the input and fill the spec array
	 */
	for (i = 0; i < num_paths; i++) {
		status = process_file(rec->spec_files[i], NULL, rec, prefix, rec->digest);
		if (status)
			goto finish;

		if (rec->validating) {
			status = nodups_specs(data, rec->spec_files[i]);
			if (status)
				goto finish;
		}
	}

	if (!baseonly) {
		status = process_file(rec->spec_files[0], "homedirs", rec, prefix,rec->digest);
		if (status && errno != ENOENT)
			goto finish;

		status = process_file(rec->spec_files[0], "local", rec, prefix,rec->digest);
		if (status && errno != ENOENT)
			goto finish;
	}

	digest_gen_hash(rec->digest);

	status = sort_specs(data);

finish:
	if (status)
		closef(rec);

	return status;
}







