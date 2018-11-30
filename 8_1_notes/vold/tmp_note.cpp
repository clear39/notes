//	@system/core/fs_mgr/include_fstab/fstab/fstab.h
struct fstab {
    int num_entries;
    struct fstab_rec* recs;
    char* fstab_filename;
};

struct fstab_rec {
    char* blk_device;
    char* mount_point;
    char* fs_type;
    unsigned long flags;
    char* fs_options;
    int fs_mgr_flags;
    char* key_loc;
    char* key_dir;
    char* verity_loc;
    long long length;
    char* label;
    int partnum;
    int swap_prio;
    int max_comp_streams;
    unsigned int zram_size;
    uint64_t reserved_size;
    unsigned int file_contents_mode;
    unsigned int file_names_mode;
    unsigned int erase_blk_size;
    unsigned int logical_blk_size;
};


static int process_config(VolumeManager *vm, bool* has_adoptable, bool* has_quota) {
    fstab = fs_mgr_read_fstab_default();//	@system/core/fs_mgr/fs_mgr_fstab.cpp:727
    if (!fstab) {
        PLOG(ERROR) << "Failed to open default fstab";
        return -1;
    }

    /* Loop through entries looking for ones that vold manages */
    *has_adoptable = false;
    *has_quota = false;
    for (int i = 0; i < fstab->num_entries; i++) {
        if (fs_mgr_is_quota(&fstab->recs[i])) {
            *has_quota = true;
        }

        if (fs_mgr_is_voldmanaged(&fstab->recs[i])) {
            if (fs_mgr_is_nonremovable(&fstab->recs[i])) {
                LOG(WARNING) << "nonremovable no longer supported; ignoring volume";
                continue;
            }

            std::string sysPattern(fstab->recs[i].blk_device);
            std::string nickname(fstab->recs[i].label);
            int flags = 0;

            if (fs_mgr_is_encryptable(&fstab->recs[i])) {
                flags |= android::vold::Disk::Flags::kAdoptable;
                *has_adoptable = true;
            }
            if (fs_mgr_is_noemulatedsd(&fstab->recs[i])
                    || property_get_bool("vold.debug.default_primary", false)) {
                flags |= android::vold::Disk::Flags::kDefaultPrimary;
            }

            vm->addDiskSource(std::shared_ptr<VolumeManager::DiskSource>(
                    new VolumeManager::DiskSource(sysPattern, nickname, flags)));
        }
    }
    return 0;
}




/*
 * tries to load default fstab.<hardware> file from /odm/etc, /vendor/etc
 * or /. loads the first one found and also combines fstab entries passed
 * in from device tree.
 */
struct fstab *fs_mgr_read_fstab_default()
{
    std::string hw;
    std::string default_fstab;

    // Use different fstab paths for normal boot and recovery boot, respectively
    if (access("/sbin/recovery", F_OK) == 0) {
        default_fstab = "/etc/recovery.fstab";
    } else if (fs_mgr_get_boot_config("hardware", &hw)) {  // normal boot
        for (const char *prefix : {"/odm/etc/fstab.","/vendor/etc/fstab.", "/fstab."}) {
            default_fstab = prefix + hw;
            if (access(default_fstab.c_str(), F_OK) == 0) break;
        }
    } else {
        LWARNING << __FUNCTION__ << "(): failed to find device hardware name";
    }

    // combines fstab entries passed in from device tree with
    // the ones found from default_fstab file
    struct fstab *fstab_dt = fs_mgr_read_fstab_dt();//	@system/core/fs_mgr/fs_mgr_fstab.cpp:697
    struct fstab *fstab = fs_mgr_read_fstab(default_fstab.c_str());//	@system/core/fs_mgr/fs_mgr_fstab.cpp:672

    return in_place_merge(fstab_dt, fstab);
}


/* Returns fstab entries parsed from the device tree if they
 * exist
 */
struct fstab *fs_mgr_read_fstab_dt()
{
    std::string fstab_buf = read_fstab_from_dt();//	@system/core/fs_mgr/fs_mgr_fstab.cpp:414:
    if (fstab_buf.empty()) {
        LINFO << __FUNCTION__ << "(): failed to read fstab from dt";
        return nullptr;
    }

    //fmemopen 系统 api
    std::unique_ptr<FILE, decltype(&fclose)> fstab_file(fmemopen(static_cast<void*>(const_cast<char*>(fstab_buf.c_str())),fstab_buf.length(), "r"), fclose);
    if (!fstab_file) {
        PERROR << __FUNCTION__ << "(): failed to create a file stream for fstab dt";
        return nullptr;
    }

    struct fstab *fstab = fs_mgr_read_fstab_file(fstab_file.get());//	@system/core/fs_mgr/fs_mgr_fstab.cpp:499
    if (!fstab) {
        LERROR << __FUNCTION__ << "(): failed to load fstab from kernel:"
               << std::endl << fstab_buf;
    }

    return fstab;
}


static std::string read_fstab_from_dt() {
    std::string fstab;
    if (!is_dt_compatible() || !is_dt_fstab_compatible()) {
        return fstab;
    }

    std::string fstabdir_name = get_android_dt_dir() + "/fstab";
    std::unique_ptr<DIR, int (*)(DIR*)> fstabdir(opendir(fstabdir_name.c_str()), closedir);
    if (!fstabdir) return fstab;

    dirent* dp;
    while ((dp = readdir(fstabdir.get())) != NULL) {
        // skip over name, compatible and .
        if (dp->d_type != DT_DIR || dp->d_name[0] == '.') continue;

        // create <dev> <mnt_point>  <type>  <mnt_flags>  <fsmgr_flags>\n
        std::vector<std::string> fstab_entry;
        std::string file_name;
        std::string value;
        // skip a partition entry if the status property is present and not set to ok
        file_name = android::base::StringPrintf("%s/%s/status", fstabdir_name.c_str(), dp->d_name);
        if (read_dt_file(file_name, &value)) {
            if (value != "okay" && value != "ok") {
                LINFO << "dt_fstab: Skip disabled entry for partition " << dp->d_name;
                continue;
            }
        }

        file_name = android::base::StringPrintf("%s/%s/dev", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            std::string boot_type;
            boot_type = read_boot_type_from_cmdline();
            file_name = android::base::StringPrintf("%s/%s/dev_%s", fstabdir_name.c_str(), dp->d_name, boot_type.c_str());
            if (!read_dt_file(file_name, &value)) {
                LERROR << "dt_fstab: Failed to find device for partition " << dp->d_name;
                fstab.clear();
                break;
            }
        }
        fstab_entry.push_back(value);
        fstab_entry.push_back(android::base::StringPrintf("/%s", dp->d_name));

        file_name = android::base::StringPrintf("%s/%s/type", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            LERROR << "dt_fstab: Failed to find type for partition " << dp->d_name;
            fstab.clear();
            break;
        }
        fstab_entry.push_back(value);

        file_name = android::base::StringPrintf("%s/%s/mnt_flags", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            LERROR << "dt_fstab: Failed to find type for partition " << dp->d_name;
            fstab.clear();
            break;
        }
        fstab_entry.push_back(value);

        file_name = android::base::StringPrintf("%s/%s/fsmgr_flags", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            LERROR << "dt_fstab: Failed to find type for partition " << dp->d_name;
            fstab.clear();
            break;
        }
        fstab_entry.push_back(value);

        fstab += android::base::Join(fstab_entry, " ");
        fstab += '\n';
    }

    return fstab;
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

    fstab = fs_mgr_read_fstab_file(fstab_file);//	@system/core/fs_mgr/fs_mgr_fstab.cpp:499
    if (fstab) {
        fstab->fstab_filename = strdup(fstab_path);
    } else {
        LERROR << __FUNCTION__ << "(): failed to load fstab from : '" << fstab_path << "'";
    }

    fclose(fstab_file);
    return fstab;
}


static struct flag_list mount_flags[] = {
    { "noatime",    MS_NOATIME },
    { "noexec",     MS_NOEXEC },
    { "nosuid",     MS_NOSUID },
    { "nodev",      MS_NODEV },
    { "nodiratime", MS_NODIRATIME },
    { "ro",         MS_RDONLY },
    { "rw",         0 },
    { "remount",    MS_REMOUNT },
    { "bind",       MS_BIND },
    { "rec",        MS_REC },
    { "unbindable", MS_UNBINDABLE },
    { "private",    MS_PRIVATE },
    { "slave",      MS_SLAVE },
    { "shared",     MS_SHARED },
    { "defaults",   0 },
    { 0,            0 },
};

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

    /* Allocate and init the fstab structure */
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

        /* If a non-comment entry is greater than the size we allocated, give an
         * error and quit.  This can happen in the unlikely case the file changes
         * between the two reads.
         */
        if (cnt >= entries) {
            LERROR << "Tried to process more entries than counted";
            break;
        }

        if (!(p = strtok_r(line, delim, &save_ptr))) {
            LERROR << "Error parsing mount source";
            goto err;
        }
        fstab->recs[cnt].blk_device = strdup(p);

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            LERROR << "Error parsing mount_point";
            goto err;
        }
        fstab->recs[cnt].mount_point = strdup(p);

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
        cnt++;
    }
    /* If an A/B partition, modify block device to be the real block device */
    if (!fs_mgr_update_for_slotselect(fstab)) {//	@system/core/fs_mgr/fs_mgr_slotselect.cpp:41
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
                        flag_vals->label = strndup(label_start,
                                                   (int) (label_end - label_start));
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




// Updates |fstab| for slot_suffix. Returns true on success, false on error.
bool fs_mgr_update_for_slotselect(struct fstab *fstab) {
    int n;
    std::string ab_suffix;

    for (n = 0; n < fstab->num_entries; n++) {
        if (fstab->recs[n].fs_mgr_flags & MF_SLOTSELECT) {
            char *tmp;
            if (ab_suffix.empty()) {
                ab_suffix = fs_mgr_get_slot_suffix();
                // Returns false as non A/B devices should not have MF_SLOTSELECT.
                if (ab_suffix.empty()) return false;
            }
            if (asprintf(&tmp, "%s%s", fstab->recs[n].blk_device, ab_suffix.c_str()) > 0) {
                free(fstab->recs[n].blk_device);
                fstab->recs[n].blk_device = tmp;
            } else {
                return false;
            }
        }
    }
    return true;
}



// Returns "_a" or "_b" based on two possible values in kernel cmdline:
//   - androidboot.slot = a or b OR
//   - androidboot.slot_suffix = _a or _b
// TODO: remove slot_suffix once it's deprecated.
std::string fs_mgr_get_slot_suffix() {
    std::string slot;
    std::string ab_suffix;

    if (fs_mgr_get_boot_config("slot", &slot)) {
        ab_suffix = "_" + slot;
    } else if (!fs_mgr_get_boot_config("slot_suffix", &ab_suffix)) {
        ab_suffix = "";
    }
    return ab_suffix;
}




/* merges fstab entries from both a and b, then returns the merged result.
 * note that the caller should only manage the return pointer without
 * doing further memory management for the two inputs, i.e. only need to
 * frees up memory of the return value without touching a and b. */
static struct fstab *in_place_merge(struct fstab *a, struct fstab *b)
{
    if (!a) return b;
    if (!b) return a;

    int total_entries = a->num_entries + b->num_entries;
    a->recs = static_cast<struct fstab_rec *>(realloc(
        a->recs, total_entries * (sizeof(struct fstab_rec))));
    if (!a->recs) {
        LERROR << __FUNCTION__ << "(): failed to allocate fstab recs";
        // If realloc() fails the original block is left untouched;
        // it is not freed or moved. So we have to free both a and b here.
        fs_mgr_free_fstab(a);
        fs_mgr_free_fstab(b);
        return nullptr;
    }

    for (int i = a->num_entries, j = 0; i < total_entries; i++, j++) {
        // copy the pointer directly *without* malloc and memcpy
        a->recs[i] = b->recs[j];
    }

    // Frees up b, but don't free b->recs[X] to make sure they are
    // accessible through a->recs[X].
    free(b->fstab_filename);
    free(b);

    a->num_entries = total_entries;
    return a;
}

