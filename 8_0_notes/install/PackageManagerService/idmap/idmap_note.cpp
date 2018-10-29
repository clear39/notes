autolink_6dq_t21fl:/data/resource-cache # ls -al
total 16
drwxrwx--x  2 system system  4096 1970-01-01 00:01 .
drwxrwx--x 34 system system  4096 1970-01-01 00:00 ..
-rw-r--r--  1 system system     0 1970-01-01 01:10 overlays.list
-rw-r--r--  1 system all_a27  544 1970-01-01 00:01 vendor@overlay@SysuiDarkTheme@SysuiDarkThemeOverlay.apk@idmap


//	@frameworks/base/cmds/idmap/idmap.cpp
//这里是在Zygote中启动
/*
/system/bin/idmap
--scan
android
/system/framework/framework-res.apk"
/data/resource-cache
/vendor/overlay
*/

int main(int argc, char **argv)
{
#if 0
    {
        char buf[1024];
        buf[0] = '\0';
        for (int i = 0; i < argc; ++i) {
            strncat(buf, argv[i], sizeof(buf) - 1);
            strncat(buf, " ", sizeof(buf) - 1);
        }
        ALOGD("%s:%d: uid=%d gid=%d argv=%s\n", __FILE__, __LINE__, getuid(), getgid(), buf);
    }
#endif

   	。。。。。。

    if (argc >= 6 && !strcmp(argv[1], "--scan")) {
        android::Vector<const char *> v;
        for (int i = 5; i < argc; i++) {
            v.push(argv[i]);
        }
        return maybe_scan(argv[2], argv[3], argv[4], &v);
    }

    。。。。。。
    return EXIT_FAILURE;
}




int maybe_scan(const char *target_package_name = "android", const char *target_apk_path = "/system/framework/framework-res.apk",
	const char *idmap_dir = "/data/resource-cache", const android::Vector<const char *> *overlay_dirs)
{
	//确认用户
    if (!verify_root_or_system()) {
        fprintf(stderr, "error: permission denied: not user root or user system\n");
        return -1;
    }

	//确认文件/system/framework/framework-res.apk 是否存在
    if (!verify_file_readable(target_apk_path)) {
        ALOGD("error: failed to read apk %s: %s\n", target_apk_path, strerror(errno));
        return -1;
    }

    //确认文件/data/resource-cache是否可写
    if (!verify_directory_writable(idmap_dir)) {
        ALOGD("error: no write access to %s: %s\n", idmap_dir, strerror(errno));
        return -1;
    }


	//确认/vendor/overlay是否可读可执行
    const size_t N = overlay_dirs->size();//	N = 1
    for (size_t i = 0; i < N; i++) {
        const char *dir = overlay_dirs->itemAt(i);
        if (!verify_directory_readable(dir)) {
            ALOGD("error: no read access to %s: %s\n", dir, strerror(errno));
            return -1;
        }
    }

    return idmap_scan(target_package_name, target_apk_path, idmap_dir, overlay_dirs);
}

bool verify_root_or_system()
{
    uid_t uid = getuid();
    gid_t gid = getgid();

    return (uid == 0 && gid == 0) || (uid == AID_SYSTEM && gid == AID_SYSTEM);
}

bool verify_file_readable(const char *path)
{
    return access(path, R_OK) == 0;
}


bool verify_directory_writable(const char *path)	
{
    return access(path, W_OK) == 0;
}

bool verify_directory_readable(const char *path)
{
    return access(path, R_OK | X_OK) == 0;
}

//	@frameworks/base/cmds/idmap/scan.cpp
//	const char *target_package_name = "android", const char *target_apk_path = "/system/framework/framework-res.apk",
//	const char *idmap_dir = "/data/resource-cache", const android::Vector<const char *> *overlay_dirs
int idmap_scan(const char *target_package_name, const char *target_apk_path,const char *idmap_dir, const android::Vector<const char *> *overlay_dirs)
{
    String8 filename = String8(idmap_dir);// "/data/resource-cache"
    filename.appendPath("overlays.list");//	"/data/resource-cache/overlays.list"

    SortedVector<Overlay> overlayVector;
    const size_t N = overlay_dirs->size();//1
    for (size_t i = 0; i < N; ++i) {
        const char *overlay_dir = overlay_dirs->itemAt(i);
        DIR *dir = opendir(overlay_dir);//overlay_dir == "/vendor/overlay"
        if (dir == NULL) {
            return EXIT_FAILURE;
        }

        struct dirent *dirent;
        while ((dirent = readdir(dir)) != NULL) {
            struct stat st;
            char overlay_apk_path[PATH_MAX + 1];
            snprintf(overlay_apk_path, PATH_MAX, "%s/%s", overlay_dir, dirent->d_name);
            if (stat(overlay_apk_path, &st) < 0) {
                continue;
            }
            // 由于全部都为目录，所以直接返回
            if (!S_ISREG(st.st_mode)) {//是否是一个常规文件,是返回true，不是返回false
                continue;
            }

            。。。。。。
        }

        closedir(dir);
    }

    if (!writePackagesList(filename.string(), overlayVector)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


//overlayVector没有任何成员
bool writePackagesList(const char *filename = "/data/resource-cache/overlays.list", const SortedVector<Overlay>& overlayVector)
{
    // the file is opened for appending so that it doesn't get truncated
    // before we can guarantee mutual exclusion via the flock
    FILE* fout = fopen(filename, "a");
    if (fout == NULL) {
        return false;
    }

    if (TEMP_FAILURE_RETRY(flock(fileno(fout), LOCK_EX)) != 0) {
        fclose(fout);
        return false;
    }

    if (TEMP_FAILURE_RETRY(ftruncate(fileno(fout), 0)) != 0) {
        TEMP_FAILURE_RETRY(flock(fileno(fout), LOCK_UN));
        fclose(fout);
        return false;
    }

    for (size_t i = 0; i < overlayVector.size(); ++i) {
        const Overlay& overlay = overlayVector[i];
        fprintf(fout, "%s %s\n", overlay.apk_path.string(), overlay.idmap_path.string());
    }

    TEMP_FAILURE_RETRY(fflush(fout));
    TEMP_FAILURE_RETRY(flock(fileno(fout), LOCK_UN));
    fclose(fout);

    // Make file world readable since Zygote (running as root) will read
    // it when creating the initial AssetManger object
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 0644
    if (chmod(filename, mode) == -1) {
        unlink(filename);
        return false;
    }

    return true;
}




/**
/system/bin/idmap 
--verify 
/system/priv-app/SystemUI/SystemUI.apk 
/vendor/overlay/SysuiDarkTheme/SysuiDarkThemeOverlay.apk 
7 
*/
int maybe_verify_fd(const char *target_apk_path = "/system/priv-app/SystemUI/SystemUI.apk ", 
	const char *overlay_apk_path = "/vendor/overlay/SysuiDarkTheme/SysuiDarkThemeOverlay.apk ",const char *idmap_str = "7")
{
    char *endptr;
    int idmap_fd = strtol(idmap_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "error: failed to parse file descriptor argument %s\n", idmap_str);
        return -1;
    }

    if (!verify_file_readable(target_apk_path)) {
        ALOGD("error: failed to read apk %s: %s\n", target_apk_path, strerror(errno));
        return -1;
    }

    if (!verify_file_readable(overlay_apk_path)) {
        ALOGD("error: failed to read apk %s: %s\n", overlay_apk_path, strerror(errno));
        return -1;
    }

    return idmap_verify_fd(target_apk_path, overlay_apk_path, idmap_fd);
}

int idmap_verify_fd(const char *target_apk_path, const char *overlay_apk_path, int fd)
{
    return !is_idmap_stale_fd(target_apk_path, overlay_apk_path, fd) ? EXIT_SUCCESS : EXIT_FAILURE;
}

bool is_idmap_stale_fd(const char *target_apk_path, const char *overlay_apk_path, int idmap_fd)
    {
        static const size_t N = ResTable::IDMAP_HEADER_SIZE_BYTES;
        struct stat st;
        if (fstat(idmap_fd, &st) == -1) {
            return true;
        }
        if (st.st_size < static_cast<off_t>(N)) {
            // file is empty or corrupt
            return true;
        }

        char buf[N];
        size_t bytesLeft = N;
        if (lseek(idmap_fd, 0, SEEK_SET) < 0) {
            return true;
        }
        for (;;) {
            ssize_t r = TEMP_FAILURE_RETRY(read(idmap_fd, buf + N - bytesLeft, bytesLeft));
            if (r < 0) {
                return true;
            }
            bytesLeft -= static_cast<size_t>(r);
            if (bytesLeft == 0) {
                break;
            }
            if (r == 0) {
                // "shouldn't happen"
                return true;
            }
        }

        uint32_t cached_target_crc, cached_overlay_crc;
        String8 cached_target_path, cached_overlay_path;
        if (!ResTable::getIdmapInfo(buf, N, NULL, &cached_target_crc, &cached_overlay_crc,&cached_target_path, &cached_overlay_path)) {
            return true;
        }

        if (cached_target_path != target_apk_path) {
            return true;
        }
        if (cached_overlay_path != overlay_apk_path) {
            return true;
        }

        uint32_t actual_target_crc, actual_overlay_crc;
        if (get_zip_entry_crc(target_apk_path, AssetManager::RESOURCES_FILENAME,&actual_target_crc) == -1) {
            return true;
        }
        if (get_zip_entry_crc(overlay_apk_path, AssetManager::RESOURCES_FILENAME, &actual_overlay_crc) == -1) {
            return true;
        }

        return cached_target_crc != actual_target_crc || cached_overlay_crc != actual_overlay_crc;
    }