static int process_config(VolumeManager* vm, bool* has_adoptable, bool* has_quota,bool* has_reserved) {
    ATRACE_NAME("process_config");

    fstab_default = fs_mgr_read_fstab_default();
    if (!fstab_default) {
        PLOG(ERROR) << "Failed to open default fstab";
        return -1;
    }

    /* Loop through entries looking for ones that vold manages */
    *has_adoptable = false;
    *has_quota = false;
    *has_reserved = false;
    for (int i = 0; i < fstab_default->num_entries; i++) {
        auto rec = &fstab_default->recs[i];
        if (fs_mgr_is_quota(rec)) {
            *has_quota = true;
        }
        if (rec->reserved_size > 0) {
            *has_reserved = true;
        }

        if (fs_mgr_is_voldmanaged(rec)) {
            if (fs_mgr_is_nonremovable(rec)) {
                LOG(WARNING) << "nonremovable no longer supported; ignoring volume";
                continue;
            }

            std::string sysPattern(rec->blk_device);
            std::string nickname(rec->label);
            int flags = 0;

            if (fs_mgr_is_encryptable(rec)) {
                flags |= android::vold::Disk::Flags::kAdoptable;
                *has_adoptable = true;
            }
            if (fs_mgr_is_noemulatedsd(rec)
                    || android::base::GetBoolProperty("vold.debug.default_primary", false)) {
                flags |= android::vold::Disk::Flags::kDefaultPrimary;
            }

            vm->addDiskSource(std::shared_ptr<VolumeManager::DiskSource>(
                    new VolumeManager::DiskSource(sysPattern, nickname, flags)));
        }
    }
    return 0;
}


//    @system/core/fs_mgr/fs_mgr_fstab.cpp
/*
 * loads the fstab file and combines with fstab entries passed in from device tree.
 */
struct fstab *fs_mgr_read_fstab_default()
{
    std::string default_fstab;

    // Use different fstab paths for normal boot and recovery boot, respectively
    if (access("/sbin/recovery", F_OK) == 0) {
        default_fstab = "/etc/recovery.fstab";
    } else {  // normal boot
        default_fstab = get_fstab_path();  //这里得到文件为  /vendor/etc/fstab.freescale
    }

    struct fstab* fstab = nullptr;
    if (!default_fstab.empty()) {

        //  @   system/core/fs_mgr/fs_mgr_fstab.cpp
        fstab = fs_mgr_read_fstab(default_fstab.c_str());
    } else {
        LINFO << __FUNCTION__ << "(): failed to find device default fstab";
    }

    struct fstab* fstab_dt = fs_mgr_read_fstab_dt();

    // combines fstab entries passed in from device tree with
    // the ones found from default_fstab file
    return in_place_merge(fstab_dt, fstab);
}

//    @system/core/fs_mgr/fs_mgr_fstab.cpp
static std::string get_fstab_path()
{
    for (const char* prop : {"hardware", "hardware.platform"}) {
        std::string hw;

        //  @   system/core/fs_mgr/fs_mgr_boot_config.cpp  
        //这里 hw 得到值为 “freescale”
        if (!fs_mgr_get_boot_config(prop, &hw)) continue;


        //目前系统 只有 /fstab.freescale 文件存在
        for (const char* prefix : {"/odm/etc/fstab.", "/vendor/etc/fstab.", "/fstab."}) {
            std::string fstab_path = prefix + hw;
            if (access(fstab_path.c_str(), F_OK) == 0) {
                return fstab_path;
            }
        }
    }

    return std::string();
}


/**
[ro.boot.hardware]: [freescale]
[ro.hardware]: [freescale]
[ro.hardware.bootctrl]: [avb]
[ro.hardware.lightsensor]: [/sys/class/i2c-dev/i2c-8/device/8-0044/]
*/

//  @system/core/fs_mgr/fs_mgr_boot_config.cpp
// Tries to get the boot config value in properties, kernel cmdline and
// device tree (in that order).  returns 'true' if successfully found, 'false'
// otherwise
bool fs_mgr_get_boot_config(const std::string& key, std::string* out_val) {
    FS_MGR_CHECK(out_val != nullptr);

    // first check if we have "ro.boot" property already
    *out_val = android::base::GetProperty("ro.boot." + key, "");
    if (!out_val->empty()) {
        return true;
    }

    // fallback to kernel cmdline, properties may not be ready yet
    if (fs_mgr_get_boot_config_from_kernel_cmdline(key, out_val)) {
        return true;
    }

    // lastly, check the device tree
    if (is_dt_compatible()) {
        std::string file_name = get_android_dt_dir() + "/" + key;
        if (android::base::ReadFileToString(file_name, out_val)) {
            if (!out_val->empty()) {
                out_val->pop_back();  // Trims the trailing '\0' out.
                return true;
            }
        }
    }

    return false;
}



struct fstab *fs_mgr_read_fstab(const char *fstab_path)
{
    FILE *fstab_file;
    struct fstab *fstab;

    fstab_file = fopen(fstab_path, "r");
    if (!fstab_file) {
        PERROR << __FUNCTION__<< "(): cannot open file: '" << fstab_path << "'";
        return nullptr;
    }

    fstab = fs_mgr_read_fstab_file(fstab_file);
    if (fstab) {
        fstab->fstab_filename = strdup(fstab_path);
    } else {
        LERROR << __FUNCTION__ << "(): failed to load fstab from : '" << fstab_path << "'";
    }

    fclose(fstab_file);
    return fstab;
}


static struct fstab *fs_mgr_read_fstab_file(FILE *fstab_file)
{
    int cnt, entries;
    ssize_t len;
    size_t alloc_len = 0;
    char *line = NULL;
    const char *delim = " \t";
    char *save_ptr, *p;
    struct fstab *fstab = NULL;
    struct fs_mgr_flag_values flag_vals;
#define FS_OPTIONS_LEN 1024
    char tmp_fs_options[FS_OPTIONS_LEN];


    /***
     * 统计行数
     */
    entries = 0;
    while ((len = getline(&line, &alloc_len, fstab_file)) != -1) {
        /* if the last character is a newline, shorten the string by 1 byte */
        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        /* Skip any leading whitespace */
        p = line;
        while (isspace(*p)) {
            p++;
        }
        /* ignore comments or empty lines */
        if (*p == '#' || *p == '\0')
            continue;
        entries++;
    }

    if (!entries) {
        LERROR << "No entries found in fstab";
        goto err;
    }

    /**
     *  Allocate and init the fstab structure
     *    
    */
    fstab = static_cast<struct fstab *>(calloc(1, sizeof(struct fstab)));
    fstab->num_entries = entries;
    fstab->recs = static_cast<struct fstab_rec *>(calloc(fstab->num_entries, sizeof(struct fstab_rec)));

    fseek(fstab_file, 0, SEEK_SET);

    cnt = 0;
    while ((len = getline(&line, &alloc_len, fstab_file)) != -1) {
        /* if the last character is a newline, shorten the string by 1 byte */
        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* Skip any leading whitespace */
        p = line;
        while (isspace(*p)) {
            p++;
        }
        /* ignore comments or empty lines */
        if (*p == '#' || *p == '\0')
            continue;
e
        /* If a non-comment entry is greater than the size we allocated, give an
         * error and quit.  This can happen in the unlikely case the file changes
         * between the two reads.
         */
        if (cnt >= ntries) {
            LERROR << "Tried to process more entries than counted";
            break;
        }

        /***
         * 存储 blk_device  
         */
        if (!(p = strtok_r(line, delim, &save_ptr))) {
            LERROR << "Error parsing mount source";
            goto err;
        }
        fstab->recs[cnt].blk_device = strdup(p);

         /***
         * 挂载点  
         */
        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            LERROR << "Error parsing mount_point";
            goto err;
        }
        fstab->recs[cnt].mount_point = strdup(p);

        /***
         * 文件系统类型  
         */
        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            LERROR << "Error parsing fs_type";
            goto err;
        }
        fstab->recs[cnt].fs_type = strdup(p);

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            LERROR << "Error parsing mount_flags";
            goto err;
        }
        tmp_fs_options[0] = '\0';
        fstab->recs[cnt].flags = parse_flags(p, mount_flags, NULL,tmp_fs_options, FS_OPTIONS_LEN);

        /* fs_options are optional */
        if (tmp_fs_options[0]) {
            fstab->recs[cnt].fs_options = strdup(tmp_fs_options);
        } else {
            fstab->recs[cnt].fs_options = NULL;
        }

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            LERROR << "Error parsing fs_mgr_options";
            goto err;
        }
        fstab->recs[cnt].fs_mgr_flags = parse_flags(p, fs_mgr_flags,&flag_vals, NULL, 0);
        fstab->recs[cnt].key_loc = flag_vals.key_loc;
        fstab->recs[cnt].key_dir = flag_vals.key_dir;
        fstab->recs[cnt].verity_loc = flag_vals.verity_loc;
        fstab->recs[cnt].length = flag_vals.part_length;
        fstab->recs[cnt].label = flag_vals.label;
        fstab->recs[cnt].partnum = flag_vals.partnum;
        fstab->recs[cnt].swap_prio = flag_vals.swap_prio;
        fstab->recs[cnt].max_comp_streams = flag_vals.max_comp_streams;
        fstab->recs[cnt].zram_size = flag_vals.zram_size;
        fstab->recs[cnt].reserved_size = flag_vals.reserved_size;
        fstab->recs[cnt].file_contents_mode = flag_vals.file_contents_mode;
        fstab->recs[cnt].file_names_mode = flag_vals.file_names_mode;
        fstab->recs[cnt].erase_blk_size = flag_vals.erase_blk_size;
        fstab->recs[cnt].logical_blk_size = flag_vals.logical_blk_size;
        fstab->recs[cnt].sysfs_path = flag_vals.sysfs_path;
        cnt++;
    }

    /* If an A/B partition, modify block device to be the real block device */
    if (!fs_mgr_update_for_slotselect(fstab)) {
        LERROR << "Error updating for slotselect";
        goto err;
    }
    free(line);
    return fstab;

err:
    free(line);
    if (fstab)
        fs_mgr_free_fstab(fstab);
    return NULL;
}


static int parse_flags(char *flags, struct flag_list *fl,struct fs_mgr_flag_values *flag_vals,char *fs_options, int fs_options_len)
{
    int f = 0;
    int i;
    char *p;
    char *savep;

    /* initialize flag values.  If we find a relevant flag, we'll
     * update the value */
    if (flag_vals) {
        memset(flag_vals, 0, sizeof(*flag_vals));
        flag_vals->partnum = -1;
        flag_vals->swap_prio = -1; /* negative means it wasn't specified. */
    }

    /* initialize fs_options to the null string */
    if (fs_options && (fs_options_len > 0)) {
        fs_options[0] = '\0';
    }

    p = strtok_r(flags, ",", &savep);
    while (p) {
        /* Look for the flag "p" in the flag list "fl"
         * If not found, the loop exits with fl[i].name being null.
         */
        for (i = 0; fl[i].name; i++) {
            if (!strncmp(p, fl[i].name, strlen(fl[i].name))) {
                f |= fl[i].flag;
                if ((fl[i].flag == MF_CRYPT) && flag_vals) {
                    /* The encryptable flag is followed by an = and the
                     * location of the keys.  Get it and return it.
                     */
                    flag_vals->key_loc = strdup(strchr(p, '=') + 1);
                } else if ((fl[i].flag == MF_VERIFY) && flag_vals) {
                    /* If the verify flag is followed by an = and the
                     * location for the verity state,  get it and return it.
                     */
                    char *start = strchr(p, '=');
                    if (start) {
                        flag_vals->verity_loc = strdup(start + 1);
                    }
                } else if ((fl[i].flag == MF_FORCECRYPT) && flag_vals) {
                    /* The forceencrypt flag is followed by an = and the
                     * location of the keys.  Get it and return it.
                     */
                    flag_vals->key_loc = strdup(strchr(p, '=') + 1);
                } else if ((fl[i].flag == MF_FORCEFDEORFBE) && flag_vals) {
                    /* The forcefdeorfbe flag is followed by an = and the
                     * location of the keys.  Get it and return it.
                     */
                    flag_vals->key_loc = strdup(strchr(p, '=') + 1);
                    flag_vals->file_contents_mode = EM_AES_256_XTS;
                    flag_vals->file_names_mode = EM_AES_256_CTS;
                } else if ((fl[i].flag == MF_FILEENCRYPTION) && flag_vals) {
                    /* The fileencryption flag is followed by an = and
                     * the mode of contents encryption, then optionally a
                     * : and the mode of filenames encryption (defaults
                     * to aes-256-cts).  Get it and return it.
                     */
                    char *mode = strchr(p, '=') + 1;
                    char *colon = strchr(mode, ':');
                    if (colon) {
                        *colon = '\0';
                    }
                    flag_vals->file_contents_mode = encryption_mode_to_flag(file_contents_encryption_modes,mode, "file contents");
                    if (colon) {
                        flag_vals->file_names_mode = encryption_mode_to_flag(file_names_encryption_modes,colon + 1, "file names");
                    } else {
                        flag_vals->file_names_mode = EM_AES_256_CTS;
                    }
                } else if ((fl[i].flag == MF_KEYDIRECTORY) && flag_vals) {
                    /* The metadata flag is followed by an = and the
                     * directory for the keys.  Get it and return it.
                     */
                    flag_vals->key_dir = strdup(strchr(p, '=') + 1);
                } else if ((fl[i].flag == MF_LENGTH) && flag_vals) {
                    /* The length flag is followed by an = and the
                     * size of the partition.  Get it and return it.
                     */
                    flag_vals->part_length = strtoll(strchr(p, '=') + 1, NULL, 0);
                } else if ((fl[i].flag == MF_VOLDMANAGED) && flag_vals) {
                    /* The voldmanaged flag is followed by an = and the
                     * label, a colon and the partition number or the
                     * word "auto", e.g.
                     *   voldmanaged=sdcard:3
                     * Get and return them.
                     */
                    char *label_start;
                    char *label_end;
                    char *part_start;

                    label_start = strchr(p, '=') + 1;
                    label_end = strchr(p, ':');
                    if (label_end) {
                        flag_vals->label = strndup(label_start,(int) (label_end - label_start));
                        part_start = strchr(p, ':') + 1;
                        if (!strcmp(part_start, "auto")) {
                            flag_vals->partnum = -1;
                        } else {
                            flag_vals->partnum = strtol(part_start, NULL, 0);
                        }
                    } else {
                        LERROR << "Warning: voldmanaged= flag malformed";
                    }
                } else if ((fl[i].flag == MF_SWAPPRIO) && flag_vals) {
                    flag_vals->swap_prio = strtoll(strchr(p, '=') + 1, NULL, 0);
                } else if ((fl[i].flag == MF_MAX_COMP_STREAMS) && flag_vals) {
                    flag_vals->max_comp_streams = strtoll(strchr(p, '=') + 1, NULL, 0);
                } else if ((fl[i].flag == MF_ZRAMSIZE) && flag_vals) {
                    int is_percent = !!strrchr(p, '%');
                    unsigned int val = strtoll(strchr(p, '=') + 1, NULL, 0);
                    if (is_percent)
                        flag_vals->zram_size = calculate_zram_size(val);
                    else
                        flag_vals->zram_size = val;
                } else if ((fl[i].flag == MF_RESERVEDSIZE) && flag_vals) {
                    /* The reserved flag is followed by an = and the
                     * reserved size of the partition.  Get it and return it.
                     */
                    flag_vals->reserved_size = parse_size(strchr(p, '=') + 1);
                } else if ((fl[i].flag == MF_ERASEBLKSIZE) && flag_vals) {
                    /* The erase block size flag is followed by an = and the flash
                     * erase block size. Get it, check that it is a power of 2 and
                     * at least 4096, and return it.
                     */
                    unsigned int val = strtoul(strchr(p, '=') + 1, NULL, 0);
                    if (val >= 4096 && (val & (val - 1)) == 0)
                        flag_vals->erase_blk_size = val;
                } else if ((fl[i].flag == MF_LOGICALBLKSIZE) && flag_vals) {
                    /* The logical block size flag is followed by an = and the flash
                     * logical block size. Get it, check that it is a power of 2 and
                     * at least 4096, and return it.
                     */
                    unsigned int val = strtoul(strchr(p, '=') + 1, NULL, 0);
                    if (val >= 4096 && (val & (val - 1)) == 0)
                        flag_vals->logical_blk_size = val;
                } else if ((fl[i].flag == MF_SYSFS) && flag_vals) {
                    /* The path to trigger device gc by idle-maint of vold. */
                    flag_vals->sysfs_path = strdup(strchr(p, '=') + 1);
                }
                break;
            }
        }

        if (!fl[i].name) {
            if (fs_options) {
                /* It's not a known flag, so it must be a filesystem specific
                 * option.  Add it to fs_options if it was passed in.
                 */
                strlcat(fs_options, p, fs_options_len);
                strlcat(fs_options, ",", fs_options_len);
            } else {
                /* fs_options was not passed in, so if the flag is unknown
                 * it's an error.
                 */
                LERROR << "Warning: unknown flag " << p;
            }
        }
        p = strtok_r(NULL, ",", &savep);
    }

    if (fs_options && fs_options[0]) {
        /* remove the last trailing comma from the list of options */
        fs_options[strlen(fs_options) - 1] = '\0';
    }

    return f;
}
