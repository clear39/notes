//  @ system/core/init/subcontext.cpp


const std::string kInitContext = "u:r:init:s0";
const std::string kVendorContext = "u:r:vendor_init:s0";

const char* const paths_and_secontexts[2][2] = {
    {"/vendor", kVendorContext.c_str()},
    {"/odm", kVendorContext.c_str()},
};

std::vector<Subcontext>* InitializeSubcontexts() {
    //  SelinuxHasVendorInit @  system/core/init/selinux.cpp
    if (SelinuxHasVendorInit()) { // true
        for (const auto& [path_prefix, secontext] : paths_and_secontexts) {
            /**
             * static std::vector<Subcontext> subcontexts;
             * 这里构造　Subcontext　并且压入　 subcontexts　集合中
             * 构造函数在   system/core/init/subcontext.h
             */
            subcontexts.emplace_back(path_prefix, secontext);
        }
    }
    return &subcontexts;
}

//  @ system/core/init/selinux.cpp
//constexpr const char plat_policy_cil_file[] = "/system/etc/selinux/plat_sepolicy.cil";
bool IsSplitPolicyDevice() {
    return access(plat_policy_cil_file, R_OK) != -1;
}

bool GetVendorMappingVersion(std::string* plat_vers) {
    //  28.0
    if (!ReadFirstLine("/vendor/etc/selinux/plat_sepolicy_vers.txt", plat_vers)) {
        PLOG(ERROR) << "Failed to read /vendor/etc/selinux/plat_sepolicy_vers.txt";
        return false;
    }
    if (plat_vers->empty()) {
        LOG(ERROR) << "No version present in plat_sepolicy_vers.txt";
        return false;
    }
    return true;
}

// This function checks whether the sepolicy supports vendor init.
bool SelinuxHasVendorInit() {
    if (!IsSplitPolicyDevice()) {
        // If this device does not split sepolicy files, vendor_init will be available in the latest
        // monolithic sepolicy file.
        return true;
    }

    std::string version;
    if (!GetVendorMappingVersion(&version)) {
        // Return true as the default if we failed to load the vendor sepolicy version.
        return true;
    }

    int major_version;
    std::string major_version_str(version, 0, version.find('.'));　// 28
    if (!ParseInt(major_version_str, &major_version)) {
        PLOG(ERROR) << "Failed to parse the vendor sepolicy major version " << major_version_str;
        // Return true as the default if we failed to parse the major version.
        return true;
    }

    return major_version >= 28;// true
}


Subcontext::Subcontext(std::string path_prefix, std::string context)
    : path_prefix_(std::move(path_prefix)), context_(std::move(context)), pid_(0) {
    LOG(INFO) << "Subcontext Constructor [" << path_prefix_ << ","<< context_ << "]";
    /**
     * 这里进行子进程创建
     */
    Fork();
}


void Subcontext::Fork() {
    LOG(INFO) << "Subcontext::Fork()..." ;
    /**
     * 
     * android::base::unique_fd socket_;  // 由父进程　init(pid=1)控制
     * subcontext_socket　传递给子进程
     * 
     */
    unique_fd subcontext_socket;
    //Socketpair @  system/core/base/include/android-base/unique_fd.h
    if (!Socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, &socket_, &subcontext_socket)) {
        LOG(FATAL) << "Could not create socket pair to communicate to subcontext";
        return;
    }

    auto result = fork();

    if (result == -1) {
        LOG(FATAL) << "Could not fork subcontext";
    } else if (result == 0) {// son process
        socket_.reset();

        // We explicitly do not use O_CLOEXEC here, such that we can reference this FD by number
        // in the subcontext process after we exec.
        int child_fd = dup(subcontext_socket);
        if (child_fd < 0) {
            PLOG(FATAL) << "Could not dup child_fd";
        }

        /**
         * 设置selinux标签
         */
        if (setexeccon(context_.c_str()) < 0) {
            PLOG(FATAL) << "Could not set execcon for '" << context_ << "'";
        }

        auto init_path = GetExecutablePath();
        auto child_fd_string = std::to_string(child_fd);
        const char* args[] = {init_path.c_str(), "subcontext", context_.c_str(),
                              child_fd_string.c_str(), nullptr};

        execv(init_path.data(), const_cast<char**>(args));

        PLOG(FATAL) << "Could not execv subcontext init";
    } else {
        subcontext_socket.reset();
        pid_ = result;
        LOG(INFO) << "Forked subcontext for '" << context_ << "' with pid " << pid_;
    }
}




