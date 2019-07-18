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
     * /proc/device-tree/firmware/android/boot_devices
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
        default_fstab = get_fstab_path();
    }
    LOG(DEBUG) << "default_fstab :" + default_fstab;
    struct fstab* fstab = nullptr;
    if (!default_fstab.empty()) {
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
    for (auto fstab_rec : mount_fstab_recs_) {
        if (!SetUpDmVerity(fstab_rec)) { // false
            PLOG(ERROR) << "Failed to setup verity for '" << fstab_rec->mount_point << "'";
            return false;
        }
        if (fs_mgr_do_mount_one(fstab_rec)) {
            PLOG(ERROR) << "Failed to mount '" << fstab_rec->mount_point << "'";
            return false;
        }
    }
    return true;
}



