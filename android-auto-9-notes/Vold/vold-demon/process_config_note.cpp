static int process_config(VolumeManager* vm, bool* has_adoptable, bool* has_quota,
                          bool* has_reserved) {
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
        default_fstab = get_fstab_path();  //这里得到文件为  /fstab.freescale
    }

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

//    @system/core/fs_mgr/fs_mgr_fstab.cpp
static std::string get_fstab_path()
{
    for (const char* prop : {"hardware", "hardware.platform"}) {
        std::string hw;

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
