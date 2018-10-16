//分析二
// if (selinux_android_restorecon("/init", 0) == -1)
int selinux_android_restorecon(const char *file, unsigned int flags)
{
    return selinux_android_restorecon_common(file, NULL, -1, flags);
}

static int selinux_android_restorecon_common(const char* pathname_orig = "/init" ,const char *seinfo = NULL,uid_t uid = -1,unsigned int flags = 0)
{
    bool nochange = (flags & SELINUX_ANDROID_RESTORECON_NOCHANGE) ? true : false; //false
    bool verbose = (flags & SELINUX_ANDROID_RESTORECON_VERBOSE) ? true : false; //false
    bool recurse = (flags & SELINUX_ANDROID_RESTORECON_RECURSE) ? true : false;//false
    bool force = (flags & SELINUX_ANDROID_RESTORECON_FORCE) ? true : false;//false
    bool datadata = (flags & SELINUX_ANDROID_RESTORECON_DATADATA) ? true : false;//false
    bool skipce = (flags & SELINUX_ANDROID_RESTORECON_SKIPCE) ? true : false;//false
    bool cross_filesystems = (flags & SELINUX_ANDROID_RESTORECON_CROSS_FILESYSTEMS) ? true : false;//false


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
    //  @bionic/libc/bionic/libgen.cpp
    pathbname = basename(pathname_orig); //pathname_orig = "/init"  pathbname == “init” basename为系统api
    if (!strcmp(pathbname, "/") || !strcmp(pathbname, ".") || !strcmp(pathbname, "..")) {
        。。。。。。//这里一般不会执行
    } else {
        pathdname = dirname(pathname_orig); //  @bionic/libc/bionic/libgen.cpp  pathdname="/" dirname为系统api
        pathdnamer = realpath(pathdname, NULL); //  realpath返回一个绝对路径   pathdnamer = “/”
        if (!pathdnamer)
            goto realpatherr;
        if (!strcmp(pathdnamer, "/"))
            error = asprintf(&pathname, "/%s", pathbname); //执行这里 pathname = “/init”
        else
            error = asprintf(&pathname, "%s/%s", pathdnamer, pathbname);
        if (error < 0)
            goto oom;
    }

    paths[0] = pathname;
    //  @external/selinux/libselinux/src/selinux_restorecon.c
    //  #define SYS_PATH "/sys"
    //  #define SYS_PREFIX SYS_PATH "/"
    issys = (!strcmp(pathname, SYS_PATH) || !strncmp(pathname, SYS_PREFIX, sizeof(SYS_PREFIX)-1)) ? true : false;// issys = false

    if (!recurse) { //recurse(递归) == false
        if (lstat(pathname, &sb) < 0) {
            error = -1;
            goto cleanup;
        }

        error = restorecon_sb(pathname, &sb, nochange, verbose, seinfo, uid);
        goto cleanup;
    }

    。。。。。。

out:
    。。。。。。

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

//  @external/selinux/libselinux/src/label_internal.h
struct selabel_handle {
    /* arguments that were passed to selabel_open */
    unsigned int backend;
    int validating;

    /* labeling operations */
    struct selabel_lookup_rec *(*func_lookup) (struct selabel_handle *h,const char *key, int type);
    void (*func_close) (struct selabel_handle *h);
    void (*func_stats) (struct selabel_handle *h);
    bool (*func_partial_match) (struct selabel_handle *h, const char *key);
    struct selabel_lookup_rec *(*func_lookup_best_match)(struct selabel_handle *h,const char *key,const char **aliases,int type);
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


static struct selabel_handle *fc_sehandle = NULL;
static void file_context_init(void)
{
    if (!fc_sehandle)
        fc_sehandle = selinux_android_file_context_handle();
}

//  @external/selinux/libselinux/src/android/android_platform.c

static const struct selinux_opt seopts_file_split[] = {
    { SELABEL_OPT_PATH, "/system/etc/selinux/plat_file_contexts" },
    { SELABEL_OPT_PATH, "/vendor/etc/selinux/nonplat_file_contexts" }
};

static const struct selinux_opt seopts_file_rootfs[] = {
    { SELABEL_OPT_PATH, "/plat_file_contexts" },            //SELABEL_OPT_PATH == 3
    { SELABEL_OPT_PATH, "/nonplat_file_contexts" }
};

struct selabel_handle* selinux_android_file_context_handle(void)
{
    if (selinux_android_opts_file_exists(seopts_file_split)) {
        return selinux_android_file_context(seopts_file_split,ARRAY_SIZE(seopts_file_split));
    } else {
        return selinux_android_file_context(seopts_file_rootfs,ARRAY_SIZE(seopts_file_rootfs));//执行这里
    }
}

static struct selabel_handle* selinux_android_file_context(const struct selinux_opt *opts,unsigned nopts)
{
    struct selabel_handle *sehandle;
    struct selinux_opt fc_opts，[nopts + 1];

    //赋值给fc_opts，并新增一个 SELABEL_OPT_BASEONLY 类型
    memcpy(fc_opts, opts, nopts*sizeof(struct selinux_opt));
    fc_opts[nopts].type = SELABEL_OPT_BASEONLY;
    fc_opts[nopts].value = (char *)1;

    //  @external/selinux/libselinux/include/selinux/label.h:28:#define SELABEL_CTX_FILE    0   /* 表示为 file contexts */
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


//  external/selinux/libselinux/src/label.c

static selabel_initfunc initfuncs[] = {
    CONFIG_FILE_BACKEND(selabel_file_init),
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

    if (!initfuncs[backend]) {  //backend == 0
        errno = ENOTSUP;
        goto out;
    }

    rec = (struct selabel_handle *)malloc(sizeof(*rec));
    if (!rec)
        goto out;

    memset(rec, 0, sizeof(*rec));
    rec->backend = backend;
    rec->validating = selabel_is_validate_set(opts, nopts);//   @external/selinux/libselinux/src/label.c     rec->validating == 0

    rec->subs = NULL;
    rec->dist_subs = NULL;
    rec->digest = selabel_is_digest_set(opts, nopts, rec->digest);//   @external/selinux/libselinux/src/label.c     rec->digest == NULL

    if ((*initfuncs[backend])(rec, opts, nopts)) {//selabel_file_init(rec, opts, nopts)
        selabel_close(rec);
        rec = NULL;
    }
out:
    return rec;
}

static inline int selabel_is_validate_set(const struct selinux_opt *opts,unsigned n)
{
    while (n--)
        if (opts[n].type == SELABEL_OPT_VALIDATE)
            return !!opts[n].value;

    return 0;
}

static inline struct selabel_digest *selabel_is_digest_set(const struct selinux_opt *opts,unsigned n,struct selabel_digest *entry)
{
    struct selabel_digest *digest = NULL;

    while (n--) {
        if (opts[n].type == SELABEL_OPT_DIGEST && opts[n].value == (char *)1) {
            digest = calloc(1, sizeof(*digest));
            if (!digest)
                goto err;

            digest->digest = calloc(1, DIGEST_SPECFILE_SIZE + 1);
            if (!digest->digest)
                goto err;

            digest->specfile_list = calloc(DIGEST_FILES_MAX,sizeof(char *));
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

//  @external/selinux/libselinux/src/label_file.c
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
            num_paths++;//  num_paths == 2
            break;
        case SELABEL_OPT_SUBSET:
            prefix = opts[i].value;
            break;
        case SELABEL_OPT_BASEONLY:
            baseonly = !!opts[i].value;//注意，最后一个为SELABEL_OPT_BASEONLY，value==1
            break;
        }

    if (!num_paths) {
       .......
    } else {
        path_provided = true;//执行这里
    }

    path = calloc(num_paths, sizeof(*path));
    if (path == NULL) {
        goto finish;
    }
    rec->spec_files = path;
    rec->spec_files_len = num_paths;

    if (path_provided) {//  true
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
        ......
    } else {
        //由于这些文件都不存在，所以这里都是空实现
        for (i = 0; i < num_paths; i++) {
            //      /plat_file_contexts.subs_dist
            //      /nonplat_file_contexts.subs_dist
            snprintf(subs_file, sizeof(subs_file), "%s.subs_dist", rec->spec_files[i]);
            rec->dist_subs = selabel_subs_init(subs_file, rec->dist_subs, rec->digest); 
            //      /plat_file_contexts.subs
            //      /nonplat_file_contexts.subs
            snprintf(subs_file, sizeof(subs_file), "%s.subs", rec->spec_files[i]);
            rec->subs = selabel_subs_init(subs_file, rec->subs, rec->digest);
        }
    }
#else
    ......
#endif

    /*
     * Do detailed validation of the input and fill the spec array
     */
    for (i = 0; i < num_paths; i++) {
        status = process_file(rec->spec_files[i], NULL, rec, prefix, rec->digest);
        if (status)
            goto finish;

        if (rec->validating) { //   rec->validating == 0
           ......
        }
    }

    if (!baseonly) {//  baseonly == 1
        ......
    }

    digest_gen_hash(rec->digest);

    status = sort_specs(data);

finish:
    if (status)
        closef(rec);

    return status;
}

//  @external/selinux/libselinux/src/label.c
struct selabel_sub *selabel_subs_init(const char *path,struct selabel_sub *list,struct selabel_digest *digest)
{
    char buf[1024];
    FILE *cfg = fopen(path, "re");
    struct selabel_sub *sub = NULL;
    struct stat sb;

    if (!cfg)
        return list;

    if (fstat(fileno(cfg), &sb) < 0)
        return list;

    while (fgets_unlocked(buf, sizeof(buf) - 1, cfg)) {
        char *ptr = NULL;
        char *src = buf;
        char *dst = NULL;

        while (*src && isspace(*src))
            src++;
        if (src[0] == '#') continue;
        ptr = src;
        while (*ptr && ! isspace(*ptr))
            ptr++;
        *ptr++ = '\0';
        if (! *src) continue;

        dst = ptr;
        while (*dst && isspace(*dst))
            dst++;
        ptr=dst;
        while (*ptr && ! isspace(*ptr))
            ptr++;
        *ptr='\0';
        if (! *dst)
            continue;

        sub = malloc(sizeof(*sub));
        if (! sub)
            goto err;
        memset(sub, 0, sizeof(*sub));

        sub->src=strdup(src);
        if (! sub->src)
            goto err;

        sub->dst=strdup(dst);
        if (! sub->dst)
            goto err;

        sub->slen = strlen(src);
        sub->next = list;
        list = sub;
    }

    if (digest_add_specfile(digest, cfg, NULL, sb.st_size, path) < 0)
        goto err;

out:
    fclose(cfg);
    return list;
err:
    if (sub)
        free(sub->src);
    free(sub);
    goto out;
}

//  @external/selinux/libselinux/src/label_file.c
static int process_file(const char *path, const char *suffix = NULL,struct selabel_handle *rec,const char *prefix = NULL, struct selabel_digest *digest = NULL)
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
        fp = open_file(path, suffix, found_path, sizeof(found_path),&sb, i > 0);
        if (fp == NULL)
            return -1;

        rc = fcontext_is_binary(fp) ? load_mmap(fp, sb.st_size, rec, found_path) : process_text_file(fp, prefix, rec, found_path);
        if (!rc)
            rc = digest_add_specfile(digest, fp, NULL, sb.st_size,found_path);

        fclose(fp);

        if (!rc)
            return 0;
    }
    return -1;
}

static FILE *open_file(const char *path, const char *suffix,char *save_path, size_t len, struct stat *sb, bool open_oldest)
{
    unsigned int i;
    int rc;
    char stack_path[len];
    struct file_details *found = NULL;

    /*
     * Rolling append of suffix. Try to open with path.suffix then the
     * next as path.suffix.suffix and so forth.
     */
    struct file_details fdetails[2] = {
            { .suffix = suffix },
            { .suffix = "bin" }
    };

    rc = snprintf(stack_path, sizeof(stack_path), "%s", path);
    if (rc >= (int) sizeof(stack_path)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    for (i = 0; i < ARRAY_SIZE(fdetails); i++) {

        /* This handles the case if suffix is null */
        path = rolling_append(stack_path, fdetails[i].suffix,sizeof(stack_path));
        if (!path)
            return NULL;

        rc = stat(path, &fdetails[i].sb);
        if (rc)
            continue;

        /* first file thing found, just take it */
        if (!found) {
            strcpy(save_path, path);
            found = &fdetails[i];
            continue;
        }

        /*
         * Keep picking the newest file found. Where "newest"
         * includes equality. This provides a precedence on
         * secondary suffixes even when the timestamp is the
         * same. Ie choose file_contexts.bin over file_contexts
         * even if the time stamp is the same. Invert this logic
         * on open_oldest set to true. The idea is that if the
         * newest file failed to process, we can attempt to
         * process the oldest. The logic here is subtle and depends
         * on the array ordering in fdetails for the case when time
         * stamps are the same.
         */
        if (open_oldest ^ (fdetails[i].sb.st_mtime >= found->sb.st_mtime)) {
            found = &fdetails[i];
            strcpy(save_path, path);
        }
    }

    if (!found) {
        errno = ENOENT;
        return NULL;
    }

    memcpy(sb, &found->sb, sizeof(*sb));
    return fopen(save_path, "re");
}


static char *rolling_append(char *current, const char *suffix, size_t max)
{
    size_t size;
    size_t suffix_size;
    size_t current_size;

    if (!suffix)
        return current;

    current_size = strlen(current);
    suffix_size = strlen(suffix);

    size = current_size + suffix_size;
    if (size < current_size || size < suffix_size)
        return NULL;

    /* ensure space for the '.' and the '\0' characters. */
    if (size >= (SIZE_MAX - 2))
        return NULL;

    size += 2;

    if (size > max)
        return NULL;

    /* Append any given suffix */
    char *to = current + current_size;
    *to++ = '.';
    strcpy(to, suffix);

    return current;
}


static bool fcontext_is_binary(FILE *fp)
{
    uint32_t magic;

    size_t len = fread(&magic, sizeof(magic), 1, fp);
    rewind(fp);

    //external/selinux/libselinux/src/label_file.h:20:#define SELINUX_MAGIC_COMPILED_FCONTEXT   0xf97cff8a
    return (len && (magic == SELINUX_MAGIC_COMPILED_FCONTEXT));
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

//  @external/selinux/libselinux/src/label_internal.h
struct selabel_lookup_rec {
    char * ctx_raw;
    char * ctx_trans;
    int validated;
};

//  @external/selinux/libselinux/src/label_file.h
/* A file security context specification. */
struct spec {
    struct selabel_lookup_rec lr;   /* holds contexts for lookup result */
    char *regex_str;    /* regular expession string for diagnostics */
    char *type_str;     /* type string for diagnostic messages */
    struct regex_data * regex; /* backend dependent regular expression data */
    mode_t mode;        /* mode format value */
    int matches;        /* number of matching pathnames */
    int stem_id;        /* indicates which stem-compression item */
    char hasMetaChars;  /* regular expression has meta-chars */
    char from_mmap;     /* this spec is from an mmap of the data */
    size_t prefix_len;      /* length of fixed path prefix */
};


/* Our stored configuration */
struct saved_data {
    /*
     * The array of specifications, initially in the same order as in
     * the specification file. Sorting occurs based on hasMetaChars.
     */
    struct spec *spec_arr;
    unsigned int nspec;
    unsigned int alloc_specs;

    /*
     * The array of regular expression stems.
     */
    struct stem *stem_arr;
    int num_stems;
    int alloc_stems;
    struct mmap_area *mmap_areas;
};


//  @external/selinux/libselinux/src/label_file.h
static inline int process_line(struct selabel_handle *rec,const char *path, const char *prefix = NULL,char *line_buf, unsigned lineno)
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
    } else if (items == 2) {//如果只有俩项的话，进行纠正，type没有，赋值为0
        /* The type field is optional. */
        context = type;
        type = 0;
    }

    //这段代码可以忽略，因为prefix为 NULL
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
        COMPAT_LOG(SELINUX_ERROR,"%s:  line %u has invalid regex %s:  %s\n",path, lineno, regex, errbuf);
        errno = EINVAL;
        return -1;
    }

    if (type) {
        mode_t mode = string_to_mode(type);

        if (mode == (mode_t)-1) {
            COMPAT_LOG(SELINUX_ERROR,"%s:  line %u has invalid file type %s\n",path, lineno, type);
            errno = EINVAL;
            return -1;
        }
        spec_arr[nspec].mode = mode;
    }

    /* Determine if specification has
     * any meta characters in the RE */
    spec_hasMetaChars(&spec_arr[nspec]);//设置通配符标志位

    if (strcmp(context, "<<none>>") && rec->validating)
        return compat_validate(rec, &spec_arr[nspec].lr, path, lineno);

    return 0;
}


static inline void spec_hasMetaChars(struct spec *spec)
{
    char *c;
    int len;
    char *end;

    c = spec->regex_str;
    len = strlen(spec->regex_str);
    end = c + len;

    spec->hasMetaChars = 0;
    spec->prefix_len = len;

    /* Look at each character in the RE specification string for a
     * meta character. Return when any meta character reached. */
    while (c < end) {
        switch (*c) {
        case '.':
        case '^':
        case '$':
        case '?':
        case '*':
        case '+':
        case '|':
        case '[':
        case '(':
        case '{':
            spec->hasMetaChars = 1;
            spec->prefix_len = c - spec->regex_str;
            return;
        case '\\':  /* skip the next character */
            c++;
            break;
        default:
            break;

        }
        c++;
    }
}


/*
 * line_buf - Buffer containing the spec entries .
 * errbuf   - Double pointer used for passing back specific error messages.
 * num_args - The number of spec parameter entries to process.
 * ...      - A 'char **spec_entry' for each parameter.
 * returns  - The number of items processed. On error, it returns -1 with errno
 *            set and may set errbuf to a specific error message.
 *
 * This function calls read_spec_entry() to do the actual string processing.
 * As such, can return anything from that function as well.
 */
int hidden read_spec_entries(char *line_buf, const char **errbuf, int num_args, ...)
{
    char **spec_entry, *buf_p;
    int len, rc, items, entry_len = 0;
    va_list ap;

    *errbuf = NULL;

    len = strlen(line_buf);
    if (line_buf[len - 1] == '\n')
        line_buf[len - 1] = '\0';
    else
        /* Handle case if line not \n terminated by bumping
         * the len for the check below (as the line is NUL
         * terminated by getline(3)) */
        len++;

    buf_p = line_buf;
    while (isspace(*buf_p))
        buf_p++;

    /* Skip comment lines and empty lines. */
    if (*buf_p == '#' || *buf_p == '\0')
        return 0;

    /* Process the spec file entries */
    va_start(ap, num_args);//   num_args 最后一个固定参数

    items = 0;
    while (items < num_args) {
        spec_entry = va_arg(ap, char **); //    char ** 表示参数类型

        if (len - 1 == buf_p - line_buf) {
            va_end(ap);
            return items;
        }

        rc = read_spec_entry(spec_entry, &buf_p, &entry_len, errbuf);
        if (rc < 0) {
            va_end(ap);
            return rc;
        }
        if (entry_len)
            items++;
    }
    va_end(ap);
    return items;
}

/*
 * Read an entry from a spec file (e.g. file_contexts)
 * entry - Buffer to allocate for the entry.
 * ptr - current location of the line to be processed.
 * returns  - 0 on success and *entry is set to be a null
 *            terminated value. On Error it returns -1 and
 *            errno will be set.
 *
 */
static inline int read_spec_entry(char **entry, char **ptr, int *len, const char **errbuf)
{
    *entry = NULL;
    char *tmp_buf = NULL;

    while (isspace(**ptr) && **ptr != '\0')
        (*ptr)++;

    tmp_buf = *ptr;
    *len = 0;

    while (!isspace(**ptr) && **ptr != '\0') {
        if (!isascii(**ptr)) {
            errno = EINVAL;
            *errbuf = "Non-ASCII characters found";
            return -1;
        }
        (*ptr)++;
        (*len)++;
    }

    if (*len) {
        *entry = strndup(tmp_buf, *len);
        if (!*entry)
            return -1;
    }

    return 0;
}

//  @external/selinux/libselinux/src/label_file.h
/* Return the length of the text that can be considered the stem, returns 0
 * if there is no identifiable（可识别的） stem（词） */
//有通配符就是返回0，
static inline int get_stem_from_spec(const char *const buf)
{
    const char *tmp = strchr(buf + 1, '/'); //查找除第一个字符外，第一个“/”
    const char *ind;

    if (!tmp)
        return 0;

    for (ind = buf; ind < tmp; ind++) {
        if (strchr(".^$?*+|[({", (int)*ind))
            return 0;
    }
    return tmp - buf;
}

//  @external/selinux/libselinux/src/label_file.h
static inline int grow_specs(struct saved_data *data)
{
    struct spec *specs;
    size_t new_specs, total_specs;

    if (data->nspec < data->alloc_specs)
        return 0;

    new_specs = data->nspec + 16;
    total_specs = data->nspec + new_specs;

    specs = realloc(data->spec_arr, total_specs * sizeof(*specs));
    if (!specs) {
        perror("realloc");
        return -1;
    }

    /* blank the new entries */
    memset(&specs[data->nspec], 0, new_specs * sizeof(*specs));

    data->spec_arr = specs;
    data->alloc_specs = total_specs;
    return 0;
}

//  @external/selinux/libselinux/src/label_file.h
/* find the stem of a file spec, returns the index into stem_arr for a new
 * or existing stem, (or -1 if there is no possible stem - IE for a file in
 * the root directory or a regex that is too complex for us). */
static inline int find_stem_from_spec(struct saved_data *data, const char *buf)
{
    int stem_len = get_stem_from_spec(buf);
    int stemid;
    char *stem;

    if (!stem_len)
        return -1;

    stemid = find_stem(data, buf, stem_len);
    if (stemid >= 0)
        return stemid;

    /* not found, allocate a new one */
    stem = strndup(buf, stem_len);
    if (!stem)
        return -1;

    return store_stem(data, stem, stem_len);
}


static inline mode_t string_to_mode(char *mode)
{
    size_t len;

    if (!mode)
        return 0;
    len = strlen(mode);
    if (mode[0] != '-' || len != 2)
        return -1;
    switch (mode[1]) {
    case 'b':
        return S_IFBLK;
    case 'c':
        return S_IFCHR;
    case 'd':
        return S_IFDIR;
    case 'p':
        return S_IFIFO;
    case 'l':
        return S_IFLNK;
    case 's':
        return S_IFSOCK;
    case '-':
        return S_IFREG;
    default:
        return -1;
    }
    /* impossible to get here */
    return 0;
}











//  @external/selinux/libselinux/src/android/android_platform.c
static int restorecon_sb(const char *pathname = "/init", const struct stat *sb,bool nochange = false, bool verbose = false,const char *seinfo = null , uid_t uid = -1)
{
    char *secontext = NULL;
    char *oldsecontext = NULL;
    int rc = 0;

    if (selabel_lookup(fc_sehandle, &secontext, pathname, sb->st_mode) < 0)
        return 0;  /* no match, but not an error */

    if (lgetfilecon(pathname, &oldsecontext) < 0)
        goto err;

    /*
     * For subdirectories of /data/data or /data/user, we ignore selabel_lookup()
     * and use pkgdir_selabel_lookup() instead. Files within those directories
     * have different labeling rules, based off of /seapp_contexts, and
     * installd is responsible for managing these labels instead of init.
     */
    //  @external/selinux/libselinux/src/android/android_platform.c:1178:#define DATA_DATA_PATH "/data/data"
    //  @external/selinux/libselinux/src/android/android_platform.c:1183:#define DATA_DATA_PREFIX DATA_DATA_PATH "/"

    //  @external/selinux/libselinux/src/android/android_platform.c:1179:#define DATA_USER_PATH "/data/user"
    //  @external/selinux/libselinux/src/android/android_platform.c:1184:#define DATA_USER_PREFIX DATA_USER_PATH "/"

    //  @external/selinux/libselinux/src/android/android_platform.c:1180:#define DATA_USER_DE_PATH "/data/user_de"
    //  @external/selinux/libselinux/src/android/android_platform.c:1185:#define DATA_USER_DE_PREFIX DATA_USER_DE_PATH "/"

    //  @external/selinux/libselinux/src/android/android_platform.c:1181:#define EXPAND_USER_PATH "/mnt/expand/\?\?\?\?\?\?\?\?-\?\?\?\?-\?\?\?\?-\?\?\?\?-\?\?\?\?\?\?\?\?\?\?\?\?/user"

    //  @external/selinux/libselinux/src/android/android_platform.c:1182:#define EXPAND_USER_DE_PATH "/mnt/expand/\?\?\?\?\?\?\?\?-\?\?\?\?-\?\?\?\?-\?\?\?\?-\?\?\?\?\?\?\?\?\?\?\?\?/user_de"
    if (!strncmp(pathname, DATA_DATA_PREFIX, sizeof(DATA_DATA_PREFIX)-1) ||
        !strncmp(pathname, DATA_USER_PREFIX, sizeof(DATA_USER_PREFIX)-1) ||
        !strncmp(pathname, DATA_USER_DE_PREFIX, sizeof(DATA_USER_DE_PREFIX)-1) ||
        !fnmatch(EXPAND_USER_PATH, pathname, FNM_LEADING_DIR|FNM_PATHNAME) ||
        !fnmatch(EXPAND_USER_DE_PATH, pathname, FNM_LEADING_DIR|FNM_PATHNAME)) { //fnmatch系统api，匹配文件或者文件夹
        ......
    }

    if (strcmp(oldsecontext, secontext) != 0) {
        if (verbose)
            selinux_log(SELINUX_INFO,"SELinux:  Relabeling %s from %s to %s.\n", pathname, oldsecontext, secontext);
        if (!nochange) {
            if (lsetfilecon(pathname, secontext) < 0)
                goto err;
        }
    }

    rc = 0;

out:
    freecon(oldsecontext);
    freecon(secontext);
    return rc;

err:
    selinux_log(SELINUX_ERROR,"SELinux: Could not set context for %s:  %s\n",pathname, strerror(errno));
    rc = -1;
    goto out;
}



//   if (selabel_lookup(fc_sehandle, &secontext, pathname, sb->st_mode) < 0)
//  @external/selinux/libselinux/src/label.c
int selabel_lookup(struct selabel_handle *rec, char **con,const char *key, int type)
{
    struct selabel_lookup_rec *lr;

    lr = selabel_lookup_common(rec, 1, key, type);
    if (!lr)
        return -1;

    *con = strdup(lr->ctx_trans);
    return *con ? 0 : -1;
}

static struct selabel_lookup_rec *
selabel_lookup_common(struct selabel_handle *rec, int translating,const char *key, int type)
{
    struct selabel_lookup_rec *lr;
    char *ptr = NULL;

    if (key == NULL) {
        errno = EINVAL;
        return NULL;
    }

    ptr = selabel_sub_key(rec, key);
    if (ptr) {
        lr = rec->func_lookup(rec, ptr, type);
        free(ptr);
    } else {
        lr = rec->func_lookup(rec, key, type);//执行这里 func_lookup在初始化赋值对应lookup
    }
    if (!lr)
        return NULL;

    if (selabel_fini(rec, lr, translating))
        return NULL;

    return lr;
}

static char *selabel_sub_key(struct selabel_handle *rec, const char *key)
{
    char *ptr = NULL;
    char *dptr = NULL;

    //由于这里rec->subs和rec->dist_subs都为NULL
    ptr = selabel_sub(rec->subs, key);
    if (ptr) {
        dptr = selabel_sub(rec->dist_subs, ptr);
        if (dptr) {
            free(ptr);
            ptr = dptr;
        }
    } else {
        ptr = selabel_sub(rec->dist_subs, key);
    }
    if (ptr)
        return ptr;

    return NULL;
}

//  external/selinux/libselinux/src/label_file.c
static struct selabel_lookup_rec *lookup(struct selabel_handle *rec,const char *key, int type)
{
    struct spec *spec;

    spec = lookup_common(rec, key, type, false);
    if (spec)
        return &spec->lr;
    return NULL;
}

static struct spec *lookup_common(struct selabel_handle *rec,const char *key,int type,bool partial = false)
{
    struct saved_data *data = (struct saved_data *)rec->data;
    struct spec *spec_arr = data->spec_arr;
    int i, rc, file_stem;
    mode_t mode = (mode_t)type;
    const char *buf;
    struct spec *ret = NULL;
    char *clean_key = NULL;
    const char *prev_slash, *next_slash;
    unsigned int sofar = 0;

    if (!data->nspec) {
        errno = ENOENT;
        goto finish;
    }

    /* Remove duplicate slashes */
    if ((next_slash = strstr(key, "//"))) {
        clean_key = (char *) malloc(strlen(key) + 1);
        if (!clean_key)
            goto finish;
        prev_slash = key;
        while (next_slash) {
            memcpy(clean_key + sofar, prev_slash, next_slash - prev_slash);
            sofar += next_slash - prev_slash;
            prev_slash = next_slash + 1;
            next_slash = strstr(prev_slash, "//");
        }
        strcpy(clean_key + sofar, prev_slash);
        key = clean_key;
    }

    buf = key;
    file_stem = find_stem_from_file(data, &buf);
    mode &= S_IFMT;

    /*
     * Check for matching specifications in reverse order, so that
     * the last matching specification is used.
     */
    for (i = data->nspec - 1; i >= 0; i--) {
        struct spec *spec = &spec_arr[i];
        /* if the spec in question matches no stem or has the same
         * stem as the file AND if the spec in question has no mode
         * specified or if the mode matches the file mode then we do
         * a regex check        */
        if ((spec->stem_id == -1 || spec->stem_id == file_stem) &&
            (!mode || !spec->mode || mode == spec->mode)) {
            if (compile_regex(data, spec, NULL) < 0)
                goto finish;
            if (spec->stem_id == -1)
                rc = regex_match(spec->regex, key, partial);
            else
                rc = regex_match(spec->regex, buf, partial);
            if (rc == REGEX_MATCH) {
                spec->matches++;
                break;
            } else if (partial && rc == REGEX_MATCH_PARTIAL)
                break;

            if (rc == REGEX_NO_MATCH)
                continue;

            errno = ENOENT;
            /* else it's an error */
            goto finish;
        }
    }

    if (i < 0 || strcmp(spec_arr[i].lr.ctx_raw, "<<none>>") == 0) {
        /* No matching specification. */
        errno = ENOENT;
        goto finish;
    }

    errno = 0;
    ret = &spec_arr[i];

finish:
    free(clean_key);
    return ret;
}


//  @ external/selinux/libselinux/src/lgetfilecon.c
int lgetfilecon(const char *path, char ** context)
{
    int ret;
    char * rcontext = NULL;

    *context = NULL;

    ret = lgetfilecon_raw(path, &rcontext);

    if (ret > 0) {
        ret = selinux_raw_to_trans_context(rcontext, context);
        freecon(rcontext);
    }

    if (ret >= 0 && *context)
        return strlen(*context) + 1;
    return ret;
}

int lgetfilecon_raw(const char *path, char ** context)
{
    char *buf;
    ssize_t size;
    ssize_t ret;

    size = INITCONTEXTLEN + 1;//    @external/selinux/libselinux/src/policy.h:15:#define INITCONTEXTLEN 255
    buf = malloc(size);
    if (!buf)
        return -1;
    memset(buf, 0, size);

    //lgetxattr成功时返回长度，返回-1，长度不够
    ret = lgetxattr(path, XATTR_NAME_SELINUX, buf, size - 1);   //external/selinux/libselinux/src/policy.h:11:#define XATTR_NAME_SELINUX "security.selinux"
    if (ret < 0 && errno == ERANGE) {
        char *newbuf;

        size = lgetxattr(path, XATTR_NAME_SELINUX, NULL, 0);
        if (size < 0)
            goto out;

        size++;
        newbuf = realloc(buf, size);
        if (!newbuf)
            goto out;

        buf = newbuf;
        memset(buf, 0, size);
        ret = lgetxattr(path, XATTR_NAME_SELINUX, buf, size - 1);
    }
out:
    if (ret == 0) {
        /* Re-map empty attribute values to errors. */
        errno = ENOTSUP;
        ret = -1;
    }
    if (ret < 0)
        free(buf);
    else
        *context = buf;
    return ret;
}

//  external/selinux/libselinux/src/lsetfilecon.c
int lsetfilecon_raw(const char *path, const char * context)
{
    int rc = lsetxattr(path, XATTR_NAME_SELINUX, context, strlen(context) + 1,
             0);
    if (rc < 0 && errno == ENOTSUP) {
        char * ccontext = NULL;
        int err = errno;
        if ((lgetfilecon_raw(path, &ccontext) >= 0) &&
            (strcmp(context,ccontext) == 0)) {
            rc = 0;
        } else {
            errno = err;
        }
        freecon(ccontext);
    }
    return rc;
}

hidden_def(lsetfilecon_raw)

int lsetfilecon(const char *path, const char *context)
{
    int ret;
    char * rcontext;

    if (selinux_trans_to_raw_context(context, &rcontext))
        return -1;

    ret = lsetfilecon_raw(path, rcontext);

    freecon(rcontext);

    return ret;
}


//  @external/selinux/libselinux/src/setrans_client.c
int selinux_raw_to_trans_context(const char * raw,char ** transp)
{
    if (!raw) {
        *transp = NULL;
        return 0;
    }

    __selinux_once(once, init_context_translations);
    init_thread_destructor();

    if (!has_setrans)  {//has_setrans == false
        *transp = strdup(raw);
        goto out;
    }

    if (prev_r2t_raw && strcmp(prev_r2t_raw, raw) == 0) {
        *transp = strdup(prev_r2t_trans);
    } else {
        free(prev_r2t_raw);
        prev_r2t_raw = NULL;
        free(prev_r2t_trans);
        prev_r2t_trans = NULL;
        if (raw_to_trans_context(raw, transp))
            *transp = strdup(raw);
        if (*transp) {
            prev_r2t_raw = strdup(raw);
            if (!prev_r2t_raw)
                goto out;
            prev_r2t_trans = strdup(*transp);
            if (!prev_r2t_trans) {
                free(prev_r2t_raw);
                prev_r2t_raw = NULL;
            }
        }
    }
out:
    return *transp ? 0 : -1;
}


static void init_context_translations(void)
{
    //  @external/selinux/mcstrans/src/mcstransd.c:30:#define SETRANS_UNIX_SOCKET "/var/run/setrans/.setrans-unix"
    has_setrans = (access(SETRANS_UNIX_SOCKET, F_OK) == 0);
    if (!has_setrans)
        return;
    if (__selinux_key_create(&destructor_key, setrans_thread_destructor) == 0)
        destructor_key_initialized = 1;
}































































//  @bionic/libc/bionic/libgen.cpp
char* basename(const char* path) {
  char* buf = __get_bionic_tls().basename_buf;
  int rc = __basename_r(path, buf, sizeof(__get_bionic_tls().basename_buf));
  return (rc < 0) ? NULL : buf;
}

//  @bionic/libc/bionic/pthread_internal.h
static inline __always_inline bionic_tls& __get_bionic_tls() {
  return *__get_thread()->bionic_tls;
}

// Make __get_thread() inlined for performance reason. See http://b/19825434.
static inline __always_inline pthread_internal_t* __get_thread() {
  //    @bionic/libc/private/__get_tls.h
  //    # define __get_tls() ({ void** __val; __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(__val)); __val; })

  void** tls = __get_tls();
  //    @bionic/libc/include/sys/cdefs.h:178:#define __predict_true(exp) __builtin_expect((exp) != 0, 1)
  if (__predict_true(tls)) {
    return reinterpret_cast<pthread_internal_t*>(tls[TLS_SLOT_THREAD_ID]);
  }

  // This happens when called during libc initialization before TLS has been initialized.
  return nullptr;
}

//  @bionic/libc/bionic/pthread_internal.h
class pthread_internal_t {
 public:
  class pthread_internal_t* next;
  class pthread_internal_t* prev;

  pid_t tid;

 private:
  pid_t cached_pid_;

 public:
  pid_t invalidate_cached_pid() {
    pid_t old_value;
    get_cached_pid(&old_value);
    set_cached_pid(0);
    return old_value;
  }

  void set_cached_pid(pid_t value) {
    cached_pid_ = value;
  }

  bool get_cached_pid(pid_t* cached_pid) {
    *cached_pid = cached_pid_;
    return (*cached_pid != 0);
  }

  pthread_attr_t attr;

  _Atomic(ThreadJoinState) join_state;

  __pthread_cleanup_t* cleanup_stack;

  void* (*start_routine)(void*);
  void* start_routine_arg;
  void* return_value;

  void* alternate_signal_stack;

  Lock startup_handshake_lock;

  size_t mmap_size;

  thread_local_dtor* thread_local_dtors;

  void* tls[BIONIC_TLS_SLOTS];

  pthread_key_data_t key_data[BIONIC_PTHREAD_KEY_COUNT];

  /*
   * The dynamic linker implements dlerror(3), which makes it hard for us to implement this
   * per-thread buffer by simply using malloc(3) and free(3).
   */
#define __BIONIC_DLERROR_BUFFER_SIZE 512
  char dlerror_buffer[__BIONIC_DLERROR_BUFFER_SIZE];

  bionic_tls* bionic_tls;
};


//  @bionic/libc/private/bionic_tls.h
// ~3 pages.
struct bionic_tls {
  locale_t locale;

  char basename_buf[MAXPATHLEN];
  char dirname_buf[MAXPATHLEN];

  mntent mntent_buf;
  char mntent_strings[BUFSIZ];

  char ptsname_buf[32];
  char ttyname_buf[64];

  char strerror_buf[NL_TEXTMAX];
  char strsignal_buf[NL_TEXTMAX];

  group_state_t group;
  passwd_state_t passwd;
};


//  截取path文件名或者文件夹名
//  "/init" => "init"
//  "/init/ooo/" =>"ooo"
static int __basename_r(const char* path, char* buffer, size_t buffer_size) {
  const char* startp = NULL;
  const char* endp = NULL;
  int len;
  int result;

  // Empty or NULL string gets treated as ".".
  if (path == NULL || *path == '\0') {
    startp = ".";
    len = 1;
    goto Exit;
  }

  //找到字符窜尾端非“/”字符
  // Strip trailing slashes(斜线). 
  endp = path + strlen(path) - 1;
  while (endp > path && *endp == '/') {
    endp--;
  }

  // All slashes becomes "/".
  if (endp == path && *endp == '/') {
    startp = "/";
    len = 1;
    goto Exit;
  }

  // Find the start of the base.
  startp = endp; //查找尾部第一个 “/”
  while (startp > path && *(startp - 1) != '/') {
    startp--;
  }

  len = endp - startp +1;

 Exit:
  result = len;
  if (buffer == NULL) {
    return result;
  }
  if (len > static_cast<int>(buffer_size) - 1) {
    len = buffer_size - 1;
    result = -1;
    errno = ERANGE;
  }

  if (len >= 0) {
    memcpy(buffer, startp, len);
    buffer[len] = 0;
  }
  return result;
}


//  截取path文件名或者文件夹名的路径
//  "/init" => "/"
//  "/init/ooo/" =>"/init"
static int __dirname_r(const char* path, char* buffer, size_t buffer_size) {
  const char* endp = NULL;
  int len;
  int result;

  // Empty or NULL string gets treated as ".".
  if (path == NULL || *path == '\0') {
    path = ".";
    len = 1;
    goto Exit;
  }

  //    找path中最后一个非“/”字符位置
  // Strip trailing slashes.
  endp = path + strlen(path) - 1;
  while (endp > path && *endp == '/') {
    endp--;
  }

  //    找path中最后一个非“/”字符位置
  // Find the start of the dir.
  while (endp > path && *endp != '/') {
    endp--;
  }

  // Either the dir is "/" or there are no slashes.
  if (endp == path) {
    path = (*endp == '/') ? "/" : ".";
    len = 1;
    goto Exit;
  }

  do {
    endp--;
  } while (endp > path && *endp == '/');

  len = endp - path + 1;

 Exit:
  result = len;
  if (len + 1 > MAXPATHLEN) {
    errno = ENAMETOOLONG;
    return -1;
  }
  if (buffer == NULL) {
    return result;
  }

  if (len > static_cast<int>(buffer_size) - 1) {
    len = buffer_size - 1;
    result = -1;
    errno = ERANGE;
  }

  if (len >= 0) {
    memcpy(buffer, path, len);
    buffer[len] = 0;
  }
  return result;
}


//  @bionic/libc/include/stdlib.h 由于__clang__未定义
char* __realpath_real(const char*, char*) __RENAME(realpath);
__errordecl(__realpath_size_error, __realpath_buf_too_small_str);



__BIONIC_FORTIFY_INLINE char* realpath(const char* path, char* resolved = null) {
    size_t bos = __bos(resolved);

    if (bos != __BIONIC_FORTIFY_UNKNOWN_SIZE && bos < __PATH_MAX) {
        __realpath_size_error();
    }

    return __realpath_real(path, resolved);
}
