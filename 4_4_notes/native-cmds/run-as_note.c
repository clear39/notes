


typedef struct {
    uid_t  uid;
    char   isDebuggable;
    char   dataDir[PATH_MAX];
    char   seinfo[PATH_MAX];
} PackageInfo;


//	system/core/run-as/run-as.c

int main(int argc, char **argv)
{
    const char* pkgname;
    int myuid, uid, gid;
    PackageInfo info;

    /* check arguments */
    if (argc < 2)
        usage();

    /* check userid of caller - must be 'shell' or 'root' */
    myuid = getuid();//获取uid，必须为shell或者root用户id
    if (myuid != AID_SHELL && myuid != AID_ROOT) {
        panic("only 'shell' or 'root' users can run this program\n");
    }

    /* retrieve package information from system */
    pkgname = argv[1];//获取参数包名
    // 根据包名获取PackageInfo信息
    if (get_package_info(pkgname, &info) < 0) {
        panic("Package '%s' is unknown\n", pkgname);
        return 1;
    }

    /* reject system packages */
    if (info.uid < AID_APP) {
        panic("Package '%s' is not an application\n", pkgname);
        return 1;
    }

    /* reject any non-debuggable package */
    //  该属性在被调试的APK在其AndroidManifext.xml里必须将android:debuggable属性设置为true
    if (!info.isDebuggable) {
        panic("Package '%s' is not debuggable\n", pkgname);
        return 1;
    }

    /* check that the data directory path is valid */
    if (check_data_path(info.dataDir, info.uid) < 0) {
        panic("Package '%s' has corrupt installation\n", pkgname);
        return 1;
    }

    /* Ensure that we change all real/effective/saved IDs at the
     * same time to avoid nasty surprises.
     */
    uid = gid = info.uid;
    if(setresgid(gid,gid,gid) || setresuid(uid,uid,uid)) {
        panic("Permission denied\n");
        return 1;
    }

    if (selinux_android_setcontext(uid, 0, info.seinfo, pkgname) < 0) {
        panic("Could not set SELinux security context:  %s\n", strerror(errno));
        return 1;
    }

    /* cd into the data directory */
    {
        int ret;
        do {
            ret = chdir(info.dataDir);
        } while (ret < 0 && errno == EINTR);

        if (ret < 0) {
            panic("Could not cd to package's data directory: %s\n", strerror(errno));
            return 1;
        }
    }

    /* User specified command for exec. */
    if (argc >= 3 ) {
        if (execvp(argv[2], argv+2) < 0) {
            panic("exec failed for %s Error:%s\n", argv[2], strerror(errno));
            return -errno;
        }
    }

    /* Default exec shell. */
    execlp("/system/bin/sh", "sh", NULL);

    panic("exec failed\n");
    return 1;
}



/* Read the system's package database and extract information about
 * 'pkgname'. Return 0 in case of success, or -1 in case of error.
 *
 * If the package is unknown, return -1 and set errno to ENOENT
 * If the package database is corrupted, return -1 and set errno to EINVAL
 */
int	get_package_info(const char* pkgName, PackageInfo *info)
{
    char*        buffer;
    size_t       buffer_len;
    const char*  p;
    const char*  buffer_end;
    int          result = -1;

    info->uid          = 0;
    info->isDebuggable = 0;
    info->dataDir[0]   = '\0';
    info->seinfo[0]    = '\0';


	

    //	#define PACKAGES_LIST_FILE  "/data/system/packages.list"  //The file containing the list of installed packages on the system 
    buffer = map_file(PACKAGES_LIST_FILE, &buffer_len);
    if (buffer == NULL)
        return -1;

    p          = buffer;
    buffer_end = buffer + buffer_len;

    /* expect the following format on each line of the control file:
     *
     *  <pkgName> <uid> <debugFlag> <dataDir> <seinfo>
     *
     * where:
     *  <pkgName>    is the package's name
     *  <uid>        is the application-specific user Id (decimal)
     *  <debugFlag>  is 1 if the package is debuggable, or 0 otherwise
     *  <dataDir>    is the path to the package's data directory (e.g. /data/data/com.example.foo)
     *  <seinfo>     is the seinfo label associated with the package
     *
     * The file is generated in com.android.server.PackageManagerService.Settings.writeLP()
     */

    while (p < buffer_end) {
        /* find end of current line and start of next one */
        const char*  end  = find_first(p, buffer_end, '\n');
        const char*  next = (end < buffer_end) ? end + 1 : buffer_end;
        const char*  q;
        int          uid, debugFlag;

        /* first field is the package name */
        p = compare_name(p, end, pkgName);
        if (p == NULL)
            goto NEXT_LINE;

        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;

        /* second field is the pid */
        uid = parse_positive_decimal(&p, end);
        if (uid < 0)
            return -1;

        info->uid = (uid_t) uid;

        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;

        /* third field is debug flag (0 or 1) */
        debugFlag = parse_positive_decimal(&p, end);
        switch (debugFlag) {
        case 0:
            info->isDebuggable = 0;
            break;
        case 1:
            info->isDebuggable = 1;
            break;
        default:
            goto BAD_FORMAT;
        }

        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;

        /* fourth field is data directory path and must not contain
         * spaces.
         */
        q = skip_non_spaces(p, end);
        if (q == p)
            goto BAD_FORMAT;

        p = string_copy(info->dataDir, sizeof info->dataDir, p, q - p);

        /* skip spaces */
        if (parse_spaces(&p, end) < 0)
            goto BAD_FORMAT;

        /* fifth field is the seinfo string */
        q = skip_non_spaces(p, end);
        if (q == p)
            goto BAD_FORMAT;

        string_copy(info->seinfo, sizeof info->seinfo, p, q - p);

        /* Ignore the rest */
        result = 0;
        goto EXIT;

    NEXT_LINE:
        p = next;
    }

    /* the package is unknown */
    errno = ENOENT;
    result = -1;
    goto EXIT;

BAD_FORMAT:
    errno = EINVAL;
    result = -1;

EXIT:
    unmap_file(buffer, buffer_len);
    return result;
}


static void* map_file(const char* filename, size_t* filesize)
{
    int  fd, ret, old_errno;
    struct stat  st;
    size_t  length = 0;
    void*   address = NULL;
    gid_t   oldegid;

    *filesize = 0;

    /*
     * Temporarily switch effective GID to allow us to read
     * the packages file
     */
    oldegid = getegid();//获取有效用户id
    if (setegid(AID_PACKAGE_INFO) < 0) {
        return NULL;
    }

    /* open the file for reading */
    fd = TEMP_FAILURE_RETRY(open(filename, O_RDONLY));
    if (fd < 0) {
        return NULL;
    }

    /* restore back to our old egid */
    if (setegid(oldegid) < 0) {
        goto EXIT;
    }

    /* get its size */
    ret = TEMP_FAILURE_RETRY(fstat(fd, &st));
    if (ret < 0)
        goto EXIT;

    /* Ensure that the file is owned by the system user */
    if ((st.st_uid != AID_SYSTEM) || (st.st_gid != AID_PACKAGE_INFO)) {
        goto EXIT;
    }

    /* Ensure that the file has sane permissions */
    if ((st.st_mode & S_IWOTH) != 0) {
        goto EXIT;
    }

    /* Ensure that the size is not ridiculously large */
    length = (size_t)st.st_size;
    if ((off_t)length != st.st_size) {
        errno = ENOMEM;
        goto EXIT;
    }

    /* Memory-map the file now */
    address = TEMP_FAILURE_RETRY(mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0));
    if (address == MAP_FAILED) {
        address = NULL;
        goto EXIT;
    }

    /* We're good, return size */
    *filesize = length;

EXIT:
    /* close the file, preserve old errno for better diagnostics */
    old_errno = errno;
    close(fd);
    errno = old_errno;

    return address;
}


/* Check that the non-space string starting at 'p' and eventually
 * ending at 'end' equals 'name'. Return new position (after name)
 * on success, or NULL on failure.
 *
 * This function fails is 'name' is NULL, empty or contains any space.
 */
static const char* compare_name(const char* p, const char* end, const char* name)
{
    /* 'name' must not be NULL or empty */
    if (name == NULL || name[0] == '\0' || p == end)
        return NULL;

    /* compare characters to those in 'name', excluding spaces */
    while (*name) {
        /* note, we don't check for *p == '\0' since
         * it will be caught in the next conditional.
         */
        if (p >= end || is_space(*p))
            goto BAD;

        if (*p != *name)
            goto BAD;

        p++;
        name++;
    }

    /* must be followed by end of line or space */
    if (p < end && !is_space(*p))
        goto BAD;

    return p;

BAD:
    return NULL;
}


/* Parse one or more whitespace characters starting from '*pp'
 * until 'end' is reached. Updates '*pp' on exit.
 *
 * Return 0 on success, -1 on failure.
 */
static int parse_spaces(const char** pp, const char* end)
{
    const char* p = *pp;

    if (p >= end || !is_space(*p)) {
        errno = EINVAL;
        return -1;
    }
    p   = skip_spaces(p, end);
    *pp = p;
    return 0;
}


