

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/init/init_first_stage.cpp
bool DoFirstStageMount() {
    // Skips first stage mount if we're in recovery mode.
    /*
    判断 /sbin/recovery 进程文件是否存在
    */
    if (IsRecoveryMode()) {
        LOG(INFO) << "First stage mount skipped (recovery mode)";
        return true;
    }

    // Firstly checks if device tree fstab entries are compatible.
    if (!is_android_dt_value_expected("fstab/compatible", "android,fstab")) {
        LOG(INFO) << "First stage mount skipped (missing/incompatible fstab in device tree)";
        return true;
    }

    std::unique_ptr<FirstStageMount> handle = FirstStageMount::Create();
    if (!handle) {
        LOG(ERROR) << "Failed to create FirstStageMount";
        return false;
    }
    return handle->DoFirstStageMount();
}


std::unique_ptr<FirstStageMount> FirstStageMount::Create() {
    /*
    读取 /proc/device-tree/firmware/android/vbmeta/compatible 文件中的值是否为 android,vbmeta
    这里有文本 且等于 android,vbmeta，所以返回true
    */
    if (IsDtVbmetaCompatible()) {
        return std::make_unique<FirstStageMountVBootV2>();
    } else {
        return std::make_unique<FirstStageMountVBootV1>();
    }
}

static inline bool IsDtVbmetaCompatible() {
    return is_android_dt_value_expected("vbmeta/compatible", "android,vbmeta");
}

//  @ system/core/init/util.cpp
bool is_android_dt_value_expected(const std::string& sub_path, const std::string& expected_content) {
    std::string dt_content;
    if (read_android_dt_file(sub_path, &dt_content)) {
        if (dt_content == expected_content) {
            return true;
        }
    }
    return false;
}

// Reads the content of device tree file under the platform's Android DT directory.
// Returns true if the read is success, false otherwise.
bool read_android_dt_file(const std::string& sub_path, std::string* dt_content) {
    const std::string file_name = get_android_dt_dir() + sub_path;
    if (android::base::ReadFileToString(file_name, dt_content)) {
        if (!dt_content->empty()) {
            dt_content->pop_back();  // Trims the trailing '\0' out.
            return true;
        }
    }
    return false;
}

// FIXME: The same logic is duplicated in system/core/fs_mgr/
const std::string& get_android_dt_dir() {
    // Set once and saves time for subsequent calls to this function
    static const std::string kAndroidDtDir = init_android_dt_dir();
    return kAndroidDtDir;
}

static std::string init_android_dt_dir() {
    //  system/core/fs_mgr/fs_mgr_fstab.cpp:36:const std::string kDefaultAndroidDtDir("/proc/device-tree/firmware/android");
    // Use the standard procfs-based path by default
    std::string android_dt_dir = kDefaultAndroidDtDir;
    // The platform may specify a custom Android DT path in kernel cmdline
    import_kernel_cmdline(false,
                          [&](const std::string& key, const std::string& value, bool in_qemu) {
                              if (key == "androidboot.android_dt_dir") {
                                  android_dt_dir = value;
                              }
                          });
    LOG(INFO) << "Using Android DT directory " << android_dt_dir;
    return android_dt_dir;
}



//  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/init/init_first_stage.cpp
class FirstStageMountVBootV2 : public FirstStageMount {}


// Class Definitions
// -----------------
FirstStageMount::FirstStageMount()
    //  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/fs_mgr/fs_mgr_fstab.cpp
    //  std::unique_ptr<fstab, decltype(&fs_mgr_free_fstab)> device_tree_fstab_;
    //  
    : need_dm_verity_(false), device_tree_fstab_(fs_mgr_read_fstab_dt(), fs_mgr_free_fstab) {
    if (!device_tree_fstab_) {
        LOG(INFO) << "Failed to read fstab from device tree";
        return;
    }
    // Stores device_tree_fstab_->recs[] into mount_fstab_recs_ (vector<fstab_rec*>)
    // for easier manipulation later, e.g., range-base for loop.
    for (int i = 0; i < device_tree_fstab_->num_entries; i++) {
        mount_fstab_recs_.push_back(&device_tree_fstab_->recs[i]);
    }

    //  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/fs_mgr/fs_mgr_fstab.cpp  
    auto boot_devices = fs_mgr_get_boot_devices();

    /***
     * DeviceHandler    @   system/core/init/devices.cpp
     */
    device_handler_ =　std::make_unique<DeviceHandler>(std::vector<Permissions>{}, std::vector<SysfsPermissions>{},
                                        std::vector<Subsystem>{}, std::move(boot_devices), false);
}


/* Returns fstab entries parsed from the device tree if they
 * exist
 */
struct fstab *fs_mgr_read_fstab_dt()
{
    std::string fstab_buf = read_fstab_from_dt();
    if (fstab_buf.empty()) {
        LINFO << __FUNCTION__ << "(): failed to read fstab from dt";
        return nullptr;
    }

    std::unique_ptr<FILE, decltype(&fclose)> fstab_file(
        fmemopen(static_cast<void*>(const_cast<char*>(fstab_buf.c_str())),
                 fstab_buf.length(), "r"), fclose);
    if (!fstab_file) {
        PERROR << __FUNCTION__ << "(): failed to create a file stream for fstab dt";
        return nullptr;
    }

    struct fstab *fstab = fs_mgr_read_fstab_file(fstab_file.get());
    if (!fstab) {
        LERROR << __FUNCTION__ << "(): failed to load fstab from kernel:" << std::endl << fstab_buf;
    }

    return fstab;
}

static std::string read_fstab_from_dt() {
    /**
     * is_dt_compatible 判断　/proc/device-tree/firmware/android/compatible　文件是否为　"android,firmware"　　返回true
     * is_dt_fstab_compatible   判断　/proc/device-tree/firmware/android/fstab/compatible　文件是否为　"android,fstab"  返回true　　
     */
    if (!is_dt_compatible() || !is_dt_fstab_compatible()) {
        return {};
    }

    std::string fstabdir_name = get_android_dt_dir() + "/fstab"; // /proc/device-tree/firmware/android/fstab/
    std::unique_ptr<DIR, int (*)(DIR*)> fstabdir(opendir(fstabdir_name.c_str()), closedir);
    if (!fstabdir) return {};



/**
 * $    ls /proc/device-tree/firmware/android/fstab/ -al       
    -r--r--r-- 1 root root 14 1970-01-04 02:06 compatible
    -r--r--r-- 1 root root  6 1970-01-04 02:17 name
    drwxr-xr-x 2 root root  0 1970-01-04 02:06 vendor
 */
    dirent* dp;
    // Each element in fstab_dt_entries is <mount point, the line format in fstab file>.
    std::vector<std::pair<std::string, std::string>> fstab_dt_entries;
    while ((dp = readdir(fstabdir.get())) != NULL) {
        /**
         * 这里必须是目录才能往下执行，这里只有　vendor
         */
        // skip over name, compatible and .
        if (dp->d_type != DT_DIR || dp->d_name[0] == '.') continue;

        // create <dev> <mnt_point>  <type>  <mnt_flags>  <fsmgr_flags>\n
        std::vector<std::string> fstab_entry;
        std::string file_name;
        std::string value;
        /**
         * /proc/device-tree/firmware/android/fstab/vendor/status  该文件不存在
         */
        // skip a partition entry if the status property is present and not set to ok
        file_name = android::base::StringPrintf("%s/%s/status", fstabdir_name.c_str(), dp->d_name);
        //  @   system/core/fs_mgr/fs_mgr_fstab.cpp
        if (read_dt_file(file_name, &value)) {
            if (value != "okay" && value != "ok") {
                LINFO << "dt_fstab: Skip disabled entry for partition " << dp->d_name;
                continue;
            }
        }
         /**
         * /proc/device-tree/firmware/android/fstab/vendor/dev  该文件不存在
         */
        file_name = android::base::StringPrintf("%s/%s/dev", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            std::string boot_type;
            /**
             * 读取　""/proc/cmdline" 中　androidboot.storage_type=emmc
             */
            boot_type = read_boot_type_from_cmdline();　　// emmc
            /**
             * /proc/device-tree/firmware/android/fstab/vendor/dev_emmc 值为　/dev/block/platform/5b010000.usdhc/by-name/vendor
             */
            file_name = android::base::StringPrintf("%s/%s/dev_%s", fstabdir_name.c_str(), dp->d_name, boot_type.c_str());
            if (!read_dt_file(file_name, &value)) {
                LERROR << "dt_fstab: Failed to find device for partition " << dp->d_name;
                return {};
            }
        }
        fstab_entry.push_back(value);　　// /dev/block/platform/5b010000.usdhc/by-name/vendor

         /**
         * /proc/device-tree/firmware/android/fstab/vendor/mnt_point　不存在
         */
        std::string mount_point;
        file_name =  android::base::StringPrintf("%s/%s/mnt_point", fstabdir_name.c_str(), dp->d_name);
        if (read_dt_file(file_name, &value)) {
            LINFO << "dt_fstab: Using a specified mount point " << value << " for " << dp->d_name;
            mount_point = value;
        } else {
            /**
             * /vender
             */
            mount_point = android::base::StringPrintf("/%s", dp->d_name);
        }
        fstab_entry.push_back(mount_point);  // /vender

        /**
         * /proc/device-tree/firmware/android/fstab/vendor/type 值为　ext4
         */
        file_name = android::base::StringPrintf("%s/%s/type", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            LERROR << "dt_fstab: Failed to find type for partition " << dp->d_name;
            return {};
        }
        fstab_entry.push_back(value);

        /**
         * /proc/device-tree/firmware/android/fstab/vendor/mnt_flags 值为　ro,barrier=1,inode_readahead_blks=8
         */
        file_name = android::base::StringPrintf("%s/%s/mnt_flags", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            LERROR << "dt_fstab: Failed to find type for partition " << dp->d_name;
            return {};
        }
        fstab_entry.push_back(value);
        /**
         * /proc/device-tree/firmware/android/fstab/vendor/fsmgr_flags 值为　wait,slotselect,avb
         */
        file_name = android::base::StringPrintf("%s/%s/fsmgr_flags", fstabdir_name.c_str(), dp->d_name);
        if (!read_dt_file(file_name, &value)) {
            LERROR << "dt_fstab: Failed to find type for partition " << dp->d_name;
            return {};
        }
        fstab_entry.push_back(value);
        // Adds a fstab_entry to fstab_dt_entries, to be sorted by mount_point later.
        fstab_dt_entries.emplace_back(mount_point, android::base::Join(fstab_entry, " "));
    }

    // Sort fstab_dt entries, to ensure /vendor is mounted before /vendor/abc is attempted.
    std::sort(fstab_dt_entries.begin(), fstab_dt_entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string fstab_result;
    for (const auto& [_, dt_entry] : fstab_dt_entries) {
        fstab_result += dt_entry + "\n";
    }
    return fstab_result;
}


//  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/fs_mgr/fs_mgr_fstab.cpp
std::set<std::string> fs_mgr_get_boot_devices() {
    // boot_devices can be specified in device tree.
    std::string dt_value;
     /**
     * /proc/device-tree/firmware/android/boot_devices 该文件不存在
     */
    std::string file_name = get_android_dt_dir() + "/boot_devices";
    if (read_dt_file(file_name, &dt_value)) {
        auto boot_devices = android::base::Split(dt_value, ",");
        return std::set<std::string>(boot_devices.begin(), boot_devices.end());
    }

    // Fallback to extract boot devices from fstab.
    std::unique_ptr<fstab, decltype(&fs_mgr_free_fstab)> fstab(fs_mgr_read_fstab_default(),fs_mgr_free_fstab);
    /**
     * @    system/core/fs_mgr/fs_mgr_fstab.cpp
     */
    if (fstab) return extract_boot_devices(*fstab);

    return {};
}


/* Extracts <device>s from the by-name symlinks specified in a fstab:
 *   /dev/block/<type>/<device>/by-name/<partition>
 *
 * <type> can be: platform, pci or vbd.
 *
 * For example, given the following entries in the input fstab:
 *   /dev/block/platform/soc/1da4000.ufshc/by-name/system
 *   /dev/block/pci/soc.0/f9824900.sdhci/by-name/vendor
 * it returns a set { "soc/1da4000.ufshc", "soc.0/f9824900.sdhci" }.
 * 
 * @    system/core/fs_mgr/fs_mgr_fstab.cpp
 */
static std::set<std::string> extract_boot_devices(const fstab& fstab) {
    std::set<std::string> boot_devices;

    for (int i = 0; i < fstab.num_entries; i++) {
        /**
         * 
         * /dev/block/platform/5b010000.usdhc/by-name/vendor
         */
        std::string blk_device(fstab.recs[i].blk_device);
        // Skips blk_device that doesn't conform to the format.
        if (!android::base::StartsWith(blk_device, "/dev/block") ||
            android::base::StartsWith(blk_device, "/dev/block/by-name") ||
            android::base::StartsWith(blk_device, "/dev/block/bootdevice/by-name")) {
            continue;
        }
        // Skips non-by_name blk_device.
        // /dev/block/<type>/<device>/by-name/<partition>
        //                           ^ slash_by_name
        auto slash_by_name = blk_device.find("/by-name");
        if (slash_by_name == std::string::npos) continue;
        /**
         * /dev/block/platform/5b010000.usdhc
         */
        blk_device.erase(slash_by_name);  // erases /by-name/<partition>

        // Erases /dev/block/, now we have <type>/<device>  
        /**
         * platform/5b010000.usdhc
         */
        blk_device.erase(0, std::string("/dev/block/").size());

        // <type>/<device>
        //       ^ first_slash
        auto first_slash = blk_device.find('/');
        if (first_slash == std::string::npos) continue;

        auto boot_device = blk_device.substr(first_slash + 1);  //  5b010000.usdhc
        if (!boot_device.empty()) boot_devices.insert(std::move(boot_device));
    }

    return boot_devices;
}


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
        //正常执行这里
        default_fstab = get_fstab_path(); //这里得到路径 /vendor/etc/fstab.freescale
    }
    LOG(DEBUG) << "default_fstab :" + default_fstab;
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



/*
 * Identify path to fstab file. Lookup is based on pattern
 * fstab.<hardware>, fstab.<hardware.platform> in folders
   /odm/etc, vendor/etc, or /.
 */
static std::string get_fstab_path()
{
    for (const char* prop : {"hardware", "hardware.platform"}) {
        std::string hw;
        /**
         * @    system/core/fs_mgr/fs_mgr_boot_config.cpp
         */
        if (!fs_mgr_get_boot_config(prop, &hw)) continue;

        for (const char* prefix : {"/odm/etc/fstab.", "/vendor/etc/fstab.", "/fstab."}) {
            std::string fstab_path = prefix + hw;  //   /vendor/etc/fstab.freescale
            if (access(fstab_path.c_str(), F_OK) == 0) {
                return fstab_path;
            }
        }
    }

    return std::string();
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/fs_mgr/fs_mgr_fstab.cpp
struct fstab *fs_mgr_read_fstab(const char *fstab_path)
{
    FILE *fstab_file;
    struct fstab *fstab;

    fstab_file = fopen(fstab_path, "r");
    if (!fstab_file) {
        PERROR << __FUNCTION__<< "(): cannot open file: '" << fstab_path << "'";
        return nullptr;
    }

    //  @   system/core/fs_mgr/fs_mgr_fstab.cpp
    fstab = fs_mgr_read_fstab_file(fstab_file);
    if (fstab) {
        fstab->fstab_filename = strdup(fstab_path);
    } else {
        LERROR << __FUNCTION__ << "(): failed to load fstab from : '" << fstab_path << "'";
    }

    fclose(fstab_file);
    return fstab;
}

//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/fs_mgr/fs_mgr_fstab.cpp
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
    fstab->recs = static_cast<struct fstab_rec *>(
        calloc(fstab->num_entries, sizeof(struct fstab_rec)));

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
        fstab->recs[cnt].fs_mgr_flags = parse_flags(p, fs_mgr_flags,
                                                    &flag_vals, NULL, 0);
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

//  @   system/core/fs_mgr/fs_mgr_slotselect.cpp
// Updates |fstab| for slot_suffix. Returns true on success, false on error.
bool fs_mgr_update_for_slotselect(struct fstab *fstab) {
    int n;
    std::string ab_suffix;

    for (n = 0; n < fstab->num_entries; n++) {
        if (fstab->recs[n].fs_mgr_flags & MF_SLOTSELECT) {
            char *tmp;
            if (ab_suffix.empty()) {
                ab_suffix = fs_mgr_get_slot_suffix();// _a
                // Return false if failed to get ab_suffix when MF_SLOTSELECT is specified.
                if (ab_suffix.empty()) return false;
            }
            // 
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

std::string fs_mgr_get_slot_suffix() {
    std::string ab_suffix;

    //  @   system/core/fs_mgr/fs_mgr_boot_config.cpp
    fs_mgr_get_boot_config("slot_suffix", &ab_suffix);
    return ab_suffix;
}


//  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/fs_mgr/fs_mgr_boot_config.cpp
// Tries to get the boot config value in properties, kernel cmdline and
// device tree (in that order).  returns 'true' if successfully found, 'false'
// otherwise
bool fs_mgr_get_boot_config(const std::string& key, std::string* out_val) {
    FS_MGR_CHECK(out_val != nullptr);

    // first check if we have "ro.boot" property already
    //  ro.boot.slot_suffix
    *out_val = android::base::GetProperty("ro.boot." + key, "");  // _a
    if (!out_val->empty()) {
        return true;
    }

    // fallback to kernel cmdline, properties may not be ready yet
    //  androidboot.slot_suffix=_a
    if (fs_mgr_get_boot_config_from_kernel_cmdline(key, out_val)) {
        return true;
    }

    // lastly, check the device tree
    //  is_dt_compatible 判断　/proc/device-tree/firmware/android/compatible　文件是否为　"android,firmware"　　返回true
    if (is_dt_compatible()) {
        
        // /proc/device-tree/firmware/android/slot_suffix  这个文件不存在

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


FirstStageMountVBootV2::FirstStageMountVBootV2() : avb_handle_(nullptr) {
    /**
     * @    system/core/init/init_first_stage.cpp
     * "/proc/device-tree/firmware/android/vbmeta/parts" 中的内容为　vbmeta,boot,system,vendor
     */
    if (!read_android_dt_file("vbmeta/parts", &device_tree_vbmeta_parts_)) {
        PLOG(ERROR) << "Failed to read vbmeta/parts from device tree";
        return;
    }
}

bool FirstStageMount::DoFirstStageMount() {
    // Nothing to mount.
    if (mount_fstab_recs_.empty()) return true;

    if (!InitDevices()) return false;

    if (!MountPartitions()) return false;

    return true;
}


bool FirstStageMount::InitDevices() {
    return GetRequiredDevices() && InitRequiredDevices();
}


bool FirstStageMountVBootV1::GetRequiredDevices() {
    std::string verity_loc_device;
    need_dm_verity_ = false;

    for (auto fstab_rec : mount_fstab_recs_) {
        // Don't allow verifyatboot in the first stage.
        if (fs_mgr_is_verifyatboot(fstab_rec)) {　// flag 不能有　verifyatboot　属性
            LOG(ERROR) << "Partitions can't be verified at boot";
            return false;
        }
        // Checks for verified partitions.
        if (fs_mgr_is_verified(fstab_rec)) {// flag　是否有　verify　属性
            need_dm_verity_ = true;
        }
        // Checks if verity metadata is on a separate partition. Note that it is
        // not partition specific, so there must be only one additional partition
        // that carries verity state.
        if (fstab_rec->verity_loc) {
            if (verity_loc_device.empty()) {
                verity_loc_device = fstab_rec->verity_loc;
            } else if (verity_loc_device != fstab_rec->verity_loc) {
                LOG(ERROR) << "More than one verity_loc found: " << verity_loc_device << ", "　<< fstab_rec->verity_loc;
                return false;
            }
        }
    }

    // Includes the partition names of fstab records and verity_loc_device (if any).
    // Notes that fstab_rec->blk_device has A/B suffix updated by fs_mgr when A/B is used.
    for (auto fstab_rec : mount_fstab_recs_) {
        required_devices_partition_names_.emplace(basename(fstab_rec->blk_device));
    }

    if (!verity_loc_device.empty()) {
        required_devices_partition_names_.emplace(basename(verity_loc_device.c_str()));
    }

    return true;
}


// Creates devices with uevent->partition_name matching one in the member variable
// required_devices_partition_names_. Found partitions will then be removed from it
// for the subsequent member function to check which devices are NOT created.
bool FirstStageMount::InitRequiredDevices() {
    if (required_devices_partition_names_.empty()) {
        return true;
    }

    if (need_dm_verity_) {　// false
        ......
    }

    auto uevent_callback = [this](const Uevent& uevent) { return UeventCallback(uevent); };
    uevent_listener_.RegenerateUevents(uevent_callback);

    // UeventCallback() will remove found partitions from required_devices_partition_names_.
    // So if it isn't empty here, it means some partitions are not found.
    if (!required_devices_partition_names_.empty()) {
        LOG(INFO) << __PRETTY_FUNCTION__
                  << ": partition(s) not found in /sys, waiting for their uevent(s): "
                  << android::base::Join(required_devices_partition_names_, ", ");
        Timer t;
        uevent_listener_.Poll(uevent_callback, 10s);
        LOG(INFO) << "Wait for partitions returned after " << t;
    }

    if (!required_devices_partition_names_.empty()) {
        LOG(ERROR) << __PRETTY_FUNCTION__ << ": partition(s) not found after polling timeout: "
                   << android::base::Join(required_devices_partition_names_, ", ");
        return false;
    }

    return true;
}


bool FirstStageMount::MountPartitions() {
    //  @   system/core/init/init_first_stage.cpp
    for (auto fstab_rec : mount_fstab_recs_) {
        if (!SetUpDmVerity(fstab_rec)) { // false
            PLOG(ERROR) << "Failed to setup verity for '" << fstab_rec->mount_point << "'";
            return false;
        }

        //  @    /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/fs_mgr/fs_mgr.cpp
        if (fs_mgr_do_mount_one(fstab_rec)) {
            PLOG(ERROR) << "Failed to mount '" << fstab_rec->mount_point << "'";
            return false;
        }
    }
    return true;
}

//  @   system/core/init/init_first_stage.cpp
bool FirstStageMountVBootV2::SetUpDmVerity(fstab_rec* fstab_rec) {
    if (fs_mgr_is_avb(fstab_rec)) {
        if (!InitAvbHandle()) return false;
        SetUpAvbHashtreeResult hashtree_result = avb_handle_->SetUpAvbHashtree(fstab_rec, false /* wait_for_verity_dev */);
        switch (hashtree_result) {
            case SetUpAvbHashtreeResult::kDisabled:
                return true;  // Returns true to mount the partition.
            case SetUpAvbHashtreeResult::kSuccess:
                // The exact block device name (fstab_rec->blk_device) is changed to
                // "/dev/block/dm-XX". Needs to create it because ueventd isn't started in init
                // first stage.
                return InitVerityDevice(fstab_rec->blk_device);
            default:
                return false;
        }
    }
    return true;  // Returns true to mount the partition.
}

int fs_mgr_do_mount_one(struct fstab_rec *rec)
{
    if (!rec) {
        return FS_MGR_DOMNT_FAILED;
    }

    //  @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/fs_mgr/fs_mgr.cpp
    int ret = __mount(rec->blk_device, rec->mount_point, rec);
    if (ret) {
      ret = (errno == EBUSY) ? FS_MGR_DOMNT_BUSY : FS_MGR_DOMNT_FAILED;
    }

    return ret;
}


/*
 * __mount(): wrapper around the mount() system call which also
 * sets the underlying block device to read-only if the mount is read-only.
 * See "man 2 mount" for return values.
 */
static int __mount(const char *source, const char *target, const struct fstab_rec *rec)
{
    unsigned long mountflags = rec->flags;
    int ret;
    int save_errno;

    /* We need this because sometimes we have legacy symlinks
     * that are lingering around and need cleaning up.
     */
    struct stat info;
    if (!lstat(target, &info))
        if ((info.st_mode & S_IFMT) == S_IFLNK)
            unlink(target);
    mkdir(target, 0755);
    errno = 0;
    ret = mount(source, target, rec->fs_type, mountflags, rec->fs_options);
    save_errno = errno;
    PINFO << __FUNCTION__ << "(source=" << source << ",target=" << target << ",type=" << rec->fs_type << ")=" << ret;
    if ((ret == 0) && (mountflags & MS_RDONLY) != 0) {
        fs_mgr_set_blk_ro(source);
    }
    errno = save_errno;
    return ret;
}



