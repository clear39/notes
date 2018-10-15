//	selinux_initialize(false);  //分析三
static void selinux_initialize(bool in_kernel_domain == false) {
    Timer t;


    //  external/selinux/libselinux/include/selinux/selinux.h
    selinux_callback cb;

    //  external/selinux/libselinux/src/callbacks.c
    //  设置不同类型日志打印函数
    cb.func_log = selinux_klog_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);
    cb.func_audit = audit_callback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);//设置selinux记录回到函数

    if (in_kernel_domain) {//false
        。。。。。。
    } else {
        selinux_init_all_handles();
    }
}


static void selinux_init_all_handles(void)
{
    sehandle = selinux_android_file_context_handle(); //	@external/selinux/libselinux/src/android/android_platform.c
    selinux_android_set_sehandle(sehandle);
    sehandle_prop = selinux_android_prop_context_handle(); //	@external/selinux/libselinux/src/android/android.c
}


struct selabel_handle* selinux_android_file_context_handle(void)
{
    if (selinux_android_opts_file_exists(seopts_file_split)) {
        return selinux_android_file_context(seopts_file_split,ARRAY_SIZE(seopts_file_split));
    } else {
        return selinux_android_file_context(seopts_file_rootfs,ARRAY_SIZE(seopts_file_rootfs));
    }
}


void selinux_android_set_sehandle(const struct selabel_handle *hndl)
{
      fc_sehandle = (struct selabel_handle *) hndl;
}

//	@
/* Structure for passing options, used by AVC and label subsystems */
struct selinux_opt {
	int type;
	const char *value;
};

//	@external/selinux/libselinux/include/selinux/label.h:51:#define SELABEL_OPT_PATH	3
static const struct selinux_opt seopts_prop_split[] = {
    { SELABEL_OPT_PATH, "/system/etc/selinux/plat_property_contexts" },
    { SELABEL_OPT_PATH, "/vendor/etc/selinux/nonplat_property_contexts"}
};

static const struct selinux_opt seopts_prop_rootfs[] = {
    { SELABEL_OPT_PATH, "/plat_property_contexts" },
    { SELABEL_OPT_PATH, "/nonplat_property_contexts"}
};

struct selabel_handle* selinux_android_prop_context_handle(void)
{
    struct selabel_handle* sehandle;
    const struct selinux_opt* seopts_prop;

    // Prefer files from /system & /vendor, fall back to files from /
    if (access(seopts_prop_split[0].value, R_OK) != -1) {
        seopts_prop = seopts_prop_split;	//	@external/selinux/libselinux/src/android/android.c
    } else {
        seopts_prop = seopts_prop_rootfs;
    }

    //	@external/selinux/libselinux/include/selinux/label.h:36:#define SELABEL_CTX_ANDROID_PROP 4
    sehandle = selabel_open(SELABEL_CTX_ANDROID_PROP,seopts_prop, 2);
    if (!sehandle) {
        selinux_log(SELINUX_ERROR, "%s: Error getting property context handle (%s)\n", __FUNCTION__, strerror(errno));
        return NULL;
    }
    selinux_log(SELINUX_INFO, "SELinux: Loaded property_contexts from %s & %s.\n",seopts_prop[0].value, seopts_prop[1].value);

    return sehandle;
}


//		@external/selinux/libselinux/src/label_internal.h
struct selabel_handle {
	/* arguments that were passed to selabel_open */
	unsigned int backend;
	int validating;

	/* labeling operations */
	struct selabel_lookup_rec *(*func_lookup) (struct selabel_handle *h,const char *key, int type);
	void (*func_close) (struct selabel_handle *h);
	void (*func_stats) (struct selabel_handle *h);
	bool (*func_partial_match) (struct selabel_handle *h, const char *key);
	struct selabel_lookup_rec *(*func_lookup_best_match)
						    (struct selabel_handle *h,
						    const char *key,
						    const char **aliases,
						    int type);
	enum selabel_cmp_result (*func_cmp)(struct selabel_handle *h1,struct selabel_handle *h2);

	/* supports backend-specific state information */
	void *data;

	/*
	 * The main spec file(s) used. Note for file contexts the local and/or
	 * homedirs could also have been used to resolve a context.
	 */
	size_t spec_files_len;
	char **spec_files;


	/* substitution support */
	struct selabel_sub *dist_subs;
	struct selabel_sub *subs;
	/* ptr to SHA1 hash information if SELABEL_OPT_DIGEST set */
	struct selabel_digest *digest;
};

//		@external/selinux/libselinux/src/label.c
static selabel_initfunc initfuncs[] = {
	CONFIG_FILE_BACKEND(selabel_file_init),
	CONFIG_MEDIA_BACKEND(selabel_media_init),
	CONFIG_X_BACKEND(selabel_x_init),
	CONFIG_DB_BACKEND(selabel_db_init),
	CONFIG_ANDROID_BACKEND(selabel_property_init), //	
	CONFIG_ANDROID_BACKEND(selabel_service_init),
};


//		@external/selinux/libselinux/src/label.c
struct selabel_handle *selabel_open(unsigned int backend == 4,const struct selinux_opt *opts,unsigned nopts)
{
	struct selabel_handle *rec = NULL;

	if (backend >= ARRAY_SIZE(initfuncs)) {
		errno = EINVAL;
		goto out;
	}

	if (!initfuncs[backend]) {//selabel_property_init
		errno = ENOTSUP;
		goto out;
	}

	rec = (struct selabel_handle *)malloc(sizeof(*rec));
	if (!rec)
		goto out;

	memset(rec, 0, sizeof(*rec));
	rec->backend = backend; //4
	rec->validating = selabel_is_validate_set(opts, nopts);	//	0

	rec->subs = NULL;
	rec->dist_subs = NULL;
	rec->digest = selabel_is_digest_set(opts, nopts, rec->digest);//NULL

	if ((*initfuncs[backend])(rec, opts, nopts)) {//selabel_property_init(rec, opts, nopts) //	@external/selinux/libselinux/src/label_backends_android.c:
		selabel_close(rec);
		rec = NULL;
	}
out:
	return rec;
}


static inline int selabel_is_validate_set(const struct selinux_opt *opts,unsigned n)
{
	while (n--)
		if (opts[n].type == SELABEL_OPT_VALIDATE)	//	#define SELABEL_OPT_VALIDATE	1
			return !!opts[n].value;

	return 0;
}


static inline struct selabel_digest *selabel_is_digest_set(const struct selinux_opt *opts, unsigned n,struct selabel_digest *entry)
{
	struct selabel_digest *digest = NULL;

	while (n--) {
		if (opts[n].type == SELABEL_OPT_DIGEST && opts[n].value == (char *)1) {//#define SELABEL_OPT_DIGEST	5
			digest = calloc(1, sizeof(*digest));
			if (!digest)
				goto err;

			digest->digest = calloc(1, DIGEST_SPECFILE_SIZE + 1);
			if (!digest->digest)
				goto err;

			digest->specfile_list = calloc(DIGEST_FILES_MAX, sizeof(char *));
			if (!digest->specfile_list)
				goto err;

			entry = digest;
			return entry;
		}
	}
	return NULL;

err:
	if (digest) {
		free(digest->digest);
		free(digest->specfile_list);
		free(digest);
	}
	return NULL;
}




int selabel_property_init(struct selabel_handle *rec,const struct selinux_opt *opts,unsigned nopts)
{
	struct saved_data *data;

	data = (struct saved_data *)calloc(1, sizeof(*data));
	if (!data)
		return -1;

	rec->data = data;
	rec->func_close = &closef;
	rec->func_stats = &stats;
	rec->func_lookup = &property_lookup;

	return init(rec, opts, nopts);
}


static int init(struct selabel_handle *rec, const struct selinux_opt *opts,unsigned n)
{
	struct saved_data *data = (struct saved_data *)rec->data;
	char **paths = NULL;
	size_t num_paths = 0;
	int status = -1;
	size_t i;

	/* Process arguments */
	i = n;
	while (i--) {
		switch (opts[i].type) {
		case SELABEL_OPT_PATH:
			num_paths++; //2
			break;
		}
	}

	if (!num_paths)
		return -1;

	paths = calloc(num_paths, sizeof(*paths));
	if (!paths)
		return -1;

	rec->spec_files = paths;
	rec->spec_files_len = num_paths;

	i = n;
	while (i--) {
		switch(opts[i].type) {
		case SELABEL_OPT_PATH:
			*paths = strdup(opts[i].value);
			if (*paths == NULL)
				goto finish;
			paths++;
		}
	}

	for (i = 0; i < num_paths; i++) {
		status = process_file(rec, rec->spec_files[i]);
		if (status)
			goto finish;
	}

	/* warn about duplicates after all files have been processed. */
	status = nodups_specs(data);
	if (status)
		goto finish;

	qsort(data->spec_arr, data->nspec, sizeof(struct spec), cmp);

	digest_gen_hash(rec->digest);

finish:
	if (status)
		closef(rec);

	return status;
}


static int process_file(struct selabel_handle *rec, const char *path)
{
	struct saved_data *data = (struct saved_data *)rec->data;
	char line_buf[BUFSIZ];
	unsigned int lineno, maxnspec, pass;
	struct stat sb;
	FILE *fp;
	int status = -1;
	unsigned int nspec;
	spec_t *spec_arr;

	/* Open the specification file. */
	if ((fp = fopen(path, "re")) == NULL)
		return -1;

	if (fstat(fileno(fp), &sb) < 0)
		goto finish;

	errno = EINVAL;

	if (!S_ISREG(sb.st_mode))
		goto finish;

	/*
	 * Two passes per specification file. First is to get the size.
	 * After the first pass, the spec array is malloced / realloced to
	 * the appropriate size. Second pass is to populate the spec array.
	 */
	maxnspec = UINT_MAX / sizeof(spec_t);
	for (pass = 0; pass < 2; pass++) {
		nspec = 0;
		lineno = 0;

		while (fgets(line_buf, sizeof(line_buf) - 1, fp) && nspec < maxnspec) {
			if (process_line(rec, path, line_buf, pass, ++lineno))
				goto finish;
			nspec++;
		}

		if (pass == 0) {
			if (nspec == 0) {
				status = 0;
				goto finish;
			}

			/* grow spec array if required */
			spec_arr = realloc(data->spec_arr,(data->nspec + nspec) * sizeof(spec_t));
			if (spec_arr == NULL)
				goto finish;

			memset(&spec_arr[data->nspec], 0, nspec * sizeof(spec_t));
			data->spec_arr = spec_arr;
			maxnspec = nspec;
			rewind(fp);
		}
	}

	status = digest_add_specfile(rec->digest, fp, NULL, sb.st_size, path);

finish:
	fclose(fp);
	return status;
}