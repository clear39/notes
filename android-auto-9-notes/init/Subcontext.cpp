//  @/work/workcodes/aosp-p9.x-auto-beta/system/core/init/subcontext.cpp


const std::string kInitContext = "u:r:init:s0";
const std::string kVendorContext = "u:r:vendor_init:s0";

const char* const paths_and_secontexts[2][2] = {
    {"/vendor", kVendorContext.c_str()},
    {"/odm", kVendorContext.c_str()},
};

std::vector<Subcontext>* InitializeSubcontexts() {
    //  SelinuxHasVendorInit @/work/workcodes/aosp-p9.x-auto-beta/system/core/init/host_init_stubs.cpp
    if (SelinuxHasVendorInit()) { // true
        for (const auto& [path_prefix, secontext] : paths_and_secontexts) {
            subcontexts.emplace_back(path_prefix, secontext);
        }
    }
    return &subcontexts;
}