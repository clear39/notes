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
    pathbname = basename(pathname_orig); //pathname_orig = "/init"
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





static struct selabel_handle *fc_sehandle = NULL;
static void file_context_init(void)
{
    if (!fc_sehandle)
        fc_sehandle = selinux_android_file_context_handle();
}
