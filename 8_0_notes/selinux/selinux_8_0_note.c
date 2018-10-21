查看文件权限：ls -Z
u:object_r:hal_allocator_default_exec:s0 android.hidl.allocator@1.0-service

“u:object_r:hal_allocator_default_exec:s0” 表明 SELinux用户、SELinux角色、类型 和 安全级别分别为u、object_r、rootfs和s0

查看运行进程权限：ps -Zef

u:r:hal_allocator_default:s0   system         251     1 0 00:00:22 ?     00:00:00 android.hidl.allocator@1.0-service
u:r:hal_audio_default:s0       audioserver    252     1 0 00:00:22 ?     00:00:00 android.hardware.audio@2.0-service
u:r:hal_bluetooth_default:s0   bluetooth      253     1 0 00:00:22 ?     00:00:00 android.hardware.bluetooth@1.0-service
u:r:hal_camera_default:s0      cameraserver   254     1 0 00:00:22 ?     00:00:00 android.hardware.camera.provider@2.4-service
u:r:hal_configstore_default:s0 system         255     1 0 00:00:22 ?     00:00:00 android.hardware.configstore@1.0-service
u:r:hal_gnss_default:s0        gps            256     1 0 00:00:22 ?     00:00:00 android.hardware.gnss@1.0-service
u:r:hal_graphics_allocator_default:s0 system  257     1 0 00:00:22 ?     00:00:02 android.hardware.graphics.allocator@2.0-service
u:r:hal_light_default:s0       system         258     1 0 00:00:22 ?     00:00:00 android.hardware.light@2.0-service
u:r:hal_memtrack_default:s0    system         259     1 0 00:00:22 ?     00:00:01 android.hardware.memtrack@1.0-service
u:r:hal_power_default:s0       system         260     1 0 00:00:22 ?     00:00:00 android.hardware.power@1.0-service
u:r:hal_sensors_default:s0     system         261     1 0 00:00:22 ?     00:00:00 android.hardware.sensors@1.0-service
u:r:hal_usb_default:s0         system         262     1 0 00:00:22 ?     00:00:00 android.hardware.usb@1.0-service
u:r:hal_wifi_default:s0        wifi           263     1 0 00:00:22 ?     00:00:00 android.hardware.wifi@1.0-servic



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
5. 用户空间的selinux库封装了对SELinux文件系统接口的读写操作。用户空间的Security Server访问内核空间的SELinux LSM模块时，都是间接地通过selinux进行的。
	这样可以将对SELinux文件系统接口的读写操作封装成更有意义的函数调用。
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




//    完整的基本安全控制语句格式为：
//    AV规则      主体      客体:客体类别    许可
//    allow     netd       proc:file     write

//用户    定义
//      @system/sepolicy/private/users              

//角色    定义
//      @system/sepolicy/public/roles               


//客体类别  定义
//      @system/sepolicy/private/security_classes  


//属性定义      定义了所有Security Context中的通用type
//     @system/sepolicy/private/attributes
//     @system/sepolicy/public/attributes
/** 所有进程 types
    # All types used for processes.
    attribute domain;
*/


//
//          @system/sepolicy/private/access_vectors
/*
//通用文件许可权限定义
common file
{
    ioctl
    ......
}


//客体类别 继承文件 通用许可权限定义
class file
inherits file
{
    execute_no_trans
    entrypoint
    execmod
    open
    audit_access
}
*/





//文件上下
//      @system/sepolicy/private/file_contexts      











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
    if (selinux_android_opts_file_exists(seopts_file_split)) {//这里/system/etc/selinux/plat_file_contexts不存在，返回false
        return selinux_android_file_context(seopts_file_split,ARRAY_SIZE(seopts_file_split));
    } else {
        return selinux_android_file_context(seopts_file_rootfs,ARRAY_SIZE(seopts_file_rootfs));//执行这里
    }
}


static bool selinux_android_opts_file_exists(const struct selinux_opt *opt)
{
    return (access(opt[0].value, R_OK) != -1);
}


static struct selabel_handle* selinux_android_file_context(const struct selinux_opt *opts,unsigned nopts)
{
    struct selabel_handle *sehandle;
    struct selinux_opt fc_opts[nopts + 1]; //这里新增了一个selinux_opt

    memcpy(fc_opts, opts, nopts*sizeof(struct selinux_opt)); //将参数复制到新的fc_opts

    //给新增的fc_opts赋值
    fc_opts[nopts].type = SELABEL_OPT_BASEONLY; //
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

struct selabel_handle *selabel_open(unsigned int backend = 0,const struct selinux_opt *opts,unsigned nopts = 3)
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

//  external/selinux/libselinux/src/label_file.c 
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







