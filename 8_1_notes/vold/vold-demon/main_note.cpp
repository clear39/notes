service vold /system/bin/vold \
        --blkid_context=u:r:blkid:s0 --blkid_untrusted_context=u:r:blkid_untrusted:s0 \
        --fsck_context=u:r:fsck:s0 --fsck_untrusted_context=u:r:fsck_untrusted:s0
    class core
    socket vold stream 0660 root mount
    socket cryptd stream 0660 root mount
    ioprio be 2
    writepid /dev/cpuset/foreground/tasks
    shutdown critical




//	@system/vold/main.cpp
int main(int argc, char** argv) {
    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));

    LOG(INFO) << "Vold 3.0 (the awakening) firing up";

    LOG(VERBOSE) << "Detected support for:"
            << (android::vold::IsFilesystemSupported("ext4") ? " ext4" : "")
            << (android::vold::IsFilesystemSupported("f2fs") ? " f2fs" : "")
            << (android::vold::IsFilesystemSupported("vfat") ? " vfat" : "");

    VolumeManager *vm;
    CommandListener *cl;
    CryptCommandListener *ccl;
    NetlinkManager *nm;

    parse_args(argc, argv);

    sehandle = selinux_android_file_context_handle();
    if (sehandle) {
        selinux_android_set_sehandle(sehandle);
    }

    // Quickly throw a CLOEXEC on the socket we just inherited from init
    fcntl(android_get_control_socket("vold"), F_SETFD, FD_CLOEXEC);
    fcntl(android_get_control_socket("cryptd"), F_SETFD, FD_CLOEXEC);

    mkdir("/dev/block/vold", 0755);

    /* For when cryptfs checks and mounts an encrypted filesystem */
    klog_set_level(6);

    /* Create our singleton managers */
    if (!(vm = VolumeManager::Instance())) {
        LOG(ERROR) << "Unable to create VolumeManager";
        exit(1);
    }

    if (!(nm = NetlinkManager::Instance())) {
        LOG(ERROR) << "Unable to create NetlinkManager";
        exit(1);
    }

    if (property_get_bool("vold.debug", false)) {//这里为空，返回false
        vm->setDebug(true);
    }

    cl = new CommandListener();
    ccl = new CryptCommandListener();
    vm->setBroadcaster((SocketListener *) cl);
    nm->setBroadcaster((SocketListener *) cl);

    if (vm->start()) {
        PLOG(ERROR) << "Unable to start VolumeManager";
        exit(1);
    }

    bool has_adoptable;
    bool has_quota;

    if (process_config(vm, &has_adoptable, &has_quota)) {
        PLOG(ERROR) << "Error reading configuration... continuing anyways";
    }

    if (nm->start()) {
        PLOG(ERROR) << "Unable to start NetlinkManager";
        exit(1);
    }

    /*
     * Now that we're up, we can respond to commands
     */
    if (cl->startListener()) {
        PLOG(ERROR) << "Unable to start CommandListener";
        exit(1);
    }

    if (ccl->startListener()) {
        PLOG(ERROR) << "Unable to start CryptCommandListener";
        exit(1);
    }

    // This call should go after listeners are started to avoid
    // a deadlock between vold and init (see b/34278978 for details)
    property_set("vold.has_adoptable", has_adoptable ? "1" : "0");
    property_set("vold.has_quota", has_quota ? "1" : "0");

    // Do coldboot here so it won't block booting,
    // also the cold boot is needed in case we have flash drive
    // connected before Vold launched
    coldboot("/sys/block");
    // Eventually we'll become the monitoring thread
    while(1) {
        pause();
    }

    LOG(ERROR) << "Vold exiting";
    exit(0);
}


//日志打印分析//////////////////////////////////////////////////////////
/*
   setenv("ANDROID_LOG_TAGS", "*:v", 1);
   android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));
*/

//	@system/core/base/logging.cpp:256
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
    ProgramInvocationName() = basename(argv[0]);
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
          gMinimumLogSeverity = VERBOSE; //这里
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
    LOG(FATAL) << "unsupported '" << spec << "' in ANDROID_LOG_TAGS (" << tags
               << ")";
  }
}


LogdLogger::LogdLogger(LogId default_log_id) : default_log_id_(default_log_id) {
}

void LogdLogger::operator()(LogId id, LogSeverity severity, const char* tag,
                            const char* file, unsigned int line,
                            const char* message) {
  static constexpr android_LogPriority kLogSeverityToAndroidLogPriority[] = {
      ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO,
      ANDROID_LOG_WARN,    ANDROID_LOG_ERROR, ANDROID_LOG_FATAL,
      ANDROID_LOG_FATAL,
  };
  static_assert(arraysize(kLogSeverityToAndroidLogPriority) == FATAL + 1,"Mismatch in size of kLogSeverityToAndroidLogPriority and values in LogSeverity");

  int priority = kLogSeverityToAndroidLogPriority[severity];
  if (id == DEFAULT) {
    id = default_log_id_;
  }

  static constexpr log_id kLogIdToAndroidLogId[] = {
    LOG_ID_MAX, LOG_ID_MAIN, LOG_ID_SYSTEM,
  };
  static_assert(arraysize(kLogIdToAndroidLogId) == SYSTEM + 1,"Mismatch in size of kLogIdToAndroidLogId and values in LogId");
  log_id lg_id = kLogIdToAndroidLogId[id];

  if (priority == ANDROID_LOG_FATAL) {
    __android_log_buf_print(lg_id, priority, tag, "%s:%u] %s", file, line,message);
  } else {
    __android_log_buf_print(lg_id, priority, tag, "%s", message);
  }
}









//参数解析分析////////////////////////////////////////////////////////
static void parse_args(int argc, char** argv) {
    static struct option opts[] = {
        {"blkid_context", required_argument, 0, 'b' },				//--blkid_context=u:r:blkid:s0
        {"blkid_untrusted_context", required_argument, 0, 'B' },	//--blkid_untrusted_context=u:r:blkid_untrusted:s0
        {"fsck_context", required_argument, 0, 'f' },				//--fsck_context=u:r:fsck:s0 
        {"fsck_untrusted_context", required_argument, 0, 'F' },		//	--fsck_untrusted_context=u:r:fsck_untrusted:s0
    };

    int c;
    while ((c = getopt_long(argc, argv, "", opts, nullptr)) != -1) {
        switch (c) {
        case 'b': android::vold::sBlkidContext = optarg; break;				//	u:r:blkid:s0
        case 'B': android::vold::sBlkidUntrustedContext = optarg; break;	//	u:r:blkid_untrusted:s0
        case 'f': android::vold::sFsckContext = optarg; break;				//	u:r:fsck:s0 
        case 'F': android::vold::sFsckUntrustedContext = optarg; break;		//	u:r:fsck_untrusted:s0
        }
    }

    CHECK(android::vold::sBlkidContext != nullptr);
    CHECK(android::vold::sBlkidUntrustedContext != nullptr);
    CHECK(android::vold::sFsckContext != nullptr);
    CHECK(android::vold::sFsckUntrustedContext != nullptr);
}




//selinux分析////////////////////////////////////////////////////////

/*
 	sehandle = selinux_android_file_context_handle();
    if (sehandle) {
        selinux_android_set_sehandle(sehandle);
    }
*/

//	@external/selinux/libselinux/src/android/android_platform.c:136
struct selabel_handle* selinux_android_file_context_handle(void)
{
    if (selinux_android_opts_file_exists(seopts_file_split)) {
        return selinux_android_file_context(seopts_file_split,ARRAY_SIZE(seopts_file_split));
    } else {
        return selinux_android_file_context(seopts_file_rootfs,ARRAY_SIZE(seopts_file_rootfs));
    }
}

static const struct selinux_opt seopts_file_split[] = {
    { SELABEL_OPT_PATH, "/system/etc/selinux/plat_file_contexts" },
    { SELABEL_OPT_PATH, "/vendor/etc/selinux/nonplat_file_contexts" }
};
static bool selinux_android_opts_file_exists(const struct selinux_opt *opt)
{
    return (access(opt[0].value, R_OK) != -1);
}

//return selinux_android_file_context(seopts_file_split,ARRAY_SIZE(seopts_file_split));
static struct selabel_handle* selinux_android_file_context(const struct selinux_opt *opts,unsigned nopts = 2)
{
    struct selabel_handle *sehandle;
    struct selinux_opt fc_opts[nopts + 1];

    memcpy(fc_opts, opts, nopts*sizeof(struct selinux_opt));
    fc_opts[nopts].type = SELABEL_OPT_BASEONLY;
    fc_opts[nopts].value = (char *)1;

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




/* 
 * Available backends.
 */

/* file contexts */
#define SELABEL_CTX_FILE	0
/* media contexts */
#define SELABEL_CTX_MEDIA	1
/* x contexts */
#define SELABEL_CTX_X		2
/* db objects */
#define SELABEL_CTX_DB		3
/* Android property service contexts */
#define SELABEL_CTX_ANDROID_PROP 4
/* Android service contexts */
#define SELABEL_CTX_ANDROID_SERVICE 5

//	@external/selinux/libselinux/src/label.c:
typedef int (*selabel_initfunc)(struct selabel_handle *rec,const struct selinux_opt *opts,unsigned nopts);

static selabel_initfunc initfuncs[] = {
	CONFIG_FILE_BACKEND(selabel_file_init),
	CONFIG_MEDIA_BACKEND(selabel_media_init),
	CONFIG_X_BACKEND(selabel_x_init),
	CONFIG_DB_BACKEND(selabel_db_init),
	CONFIG_ANDROID_BACKEND(selabel_property_init),
	CONFIG_ANDROID_BACKEND(selabel_service_init),
};


//	@external/selinux/libselinux/src/label.c:354
struct selabel_handle *selabel_open(unsigned int backend = SELABEL_CTX_FILE,const struct selinux_opt *opts,unsigned nopts = 3)
{
	struct selabel_handle *rec = NULL;

	if (backend >= ARRAY_SIZE(initfuncs)) {
		errno = EINVAL;
		goto out;
	}

	if (!initfuncs[backend]) {//
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

	if ((*initfuncs[backend])(rec, opts, nopts)) {//这里执行selabel_file_init
		selabel_close(rec);
		rec = NULL;
	}
out:
	return rec;
}



//	external/selinux/libselinux/src/label_file.c:1002
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
	。。。。。。
#else
	if (!path_provided) {//path_provided = true
		selinux_log(SELINUX_ERROR, "No path given to file labeling backend\n");
		goto finish;
	}
#endif

	/*
	 * Do detailed validation of the input and fill the spec array
	 */
	for (i = 0; i < num_paths; i++) {
		status = process_file(rec->spec_files[i], NULL, rec, prefix, rec->digest);	// prefix = null
		if (status)
			goto finish;

		if (rec->validating) {
			status = nodups_specs(data, rec->spec_files[i]);
			if (status)
				goto finish;
		}
	}

	if (!baseonly) {//	baseonly = false
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



static int process_file(const char *path, const char *suffix,struct selabel_handle *rec,const char *prefix = null, struct selabel_digest *digest)
{
	int rc;
	unsigned int i;
	struct stat sb;
	FILE *fp = NULL;
	char found_path[PATH_MAX];

	/*
	 * On the first pass open the newest modified file. If it fails to
	 * process, then the second pass shall open the oldest file. If both
	 * passes fail, then it's a fatal error.
	 */
	for (i = 0; i < 2; i++) {
		//	open_file 拼接路径，打开文件，并将路径保存到found_path中
		fp = open_file(path, suffix, found_path, sizeof(found_path),&sb, i > 0);
		if (fp == NULL)
			return -1;
		//由于平台为文本格式，所以执行process_text_file
		rc = fcontext_is_binary(fp) ? load_mmap(fp, sb.st_size, rec, found_path) : process_text_file(fp, prefix, rec, found_path);
		if (!rc)
			rc = digest_add_specfile(digest, fp, NULL, sb.st_size,found_path);

		fclose(fp);

		if (!rc)
			return 0;
	}
	return -1;
}


static bool fcontext_is_binary(FILE *fp)
{
	uint32_t magic;

	size_t len = fread(&magic, sizeof(magic), 1, fp);
	rewind(fp);

	//	@external/selinux/libselinux/src/label_file.h:20:#define SELINUX_MAGIC_COMPILED_FCONTEXT	0xf97cff8a
	return (len && (magic == SELINUX_MAGIC_COMPILED_FCONTEXT));//	
}



static int process_text_file(FILE *fp, const char *prefix,struct selabel_handle *rec, const char *path)
{
	int rc;
	size_t line_len;
	unsigned int lineno = 0;
	char *line_buf = NULL;

	while (getline(&line_buf, &line_len, fp) > 0) {
		rc = process_line(rec, path, prefix, line_buf, ++lineno);
		if (rc)
			goto out;
	}
	rc = 0;
out:
	free(line_buf);
	return rc;
}


//	@external/selinux/libselinux/src/label_file.h:375
/* This service is used by label_file.c process_file() and
 * utils/sefcontext_compile.c */
static inline int process_line(struct selabel_handle *rec,const char *path, const char *prefix,char *line_buf, unsigned lineno)
{
	int items, len, rc;
	char *regex = NULL, *type = NULL, *context = NULL;
	struct saved_data *data = (struct saved_data *)rec->data;
	struct spec *spec_arr;
	unsigned int nspec = data->nspec;
	const char *errbuf = NULL;

	items = read_spec_entries(line_buf, &errbuf, 3, &regex, &type, &context);
	if (items < 0) {
		rc = errno;
		selinux_log(SELINUX_ERROR,"%s:  line %u error due to: %s\n", path,lineno, errbuf ?: strerror(errno));
		errno = rc;
		return -1;
	}

	if (items == 0)
		return items;

	if (items < 2) {
		COMPAT_LOG(SELINUX_ERROR,"%s:  line %u is missing fields\n", path,lineno);
		if (items == 1)
			free(regex);
		errno = EINVAL;
		return -1;
	} else if (items == 2) {
		/* The type field is optional. */
		context = type;
		type = 0;
	}

	len = get_stem_from_spec(regex);
	if (len && prefix && strncmp(prefix, regex, len)) {
		/* Stem of regex does not match requested prefix, discard. */
		free(regex);
		free(type);
		free(context);
		return 0;
	}

	rc = grow_specs(data);
	if (rc)
		return rc;

	spec_arr = data->spec_arr;

	/* process and store the specification in spec. */
	spec_arr[nspec].stem_id = find_stem_from_spec(data, regex);
	spec_arr[nspec].regex_str = regex;

	spec_arr[nspec].type_str = type;
	spec_arr[nspec].mode = 0;

	spec_arr[nspec].lr.ctx_raw = context;

	/*
	 * bump data->nspecs to cause closef() to cover it in its free
	 * but do not bump nspec since it's used below.
	 */
	data->nspec++;

	if (rec->validating && compile_regex(data, &spec_arr[nspec], &errbuf)) {
		COMPAT_LOG(SELINUX_ERROR,
			   "%s:  line %u has invalid regex %s:  %s\n",
			   path, lineno, regex, errbuf);
		errno = EINVAL;
		return -1;
	}

	if (type) {
		mode_t mode = string_to_mode(type);

		if (mode == (mode_t)-1) {
			COMPAT_LOG(SELINUX_ERROR,
				   "%s:  line %u has invalid file type %s\n",
				   path, lineno, type);
			errno = EINVAL;
			return -1;
		}
		spec_arr[nspec].mode = mode;
	}

	/* Determine if specification has
	 * any meta characters in the RE */
	spec_hasMetaChars(&spec_arr[nspec]);

	if (strcmp(context, "<<none>>") && rec->validating)
		return compat_validate(rec, &spec_arr[nspec].lr, path, lineno);

	return 0;
}





