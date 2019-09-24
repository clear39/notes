

// BuiltinArguments @   /work/workcodes/aosp-p9.0.0_2.1.0-auto-ga/system/core/init/builtin_arguments.h
using BuiltinFunction = std::function<Result<Success>(const BuiltinArguments&)>;


//  KeywordMap  @   system/core/init/keyword_map.h
using KeywordFunctionMap = KeywordMap<std::pair<bool, BuiltinFunction>>;

class BuiltinFunctionMap : public KeywordFunctionMap {

}



//  @   system/core/init/builtins.cpp
BuiltinFunctionMap::BuiltinFunctionMap() {

}



const Result<Function> KeywordMap::FindFunction(const std::vector<std::string>& args) const {
    using android::base::StringPrintf;

    if (args.empty()) return Error() << "Keyword needed, but not provided";

    auto& keyword = args[0];
    auto num_args = args.size() - 1;

    /**
     * 这里 map()函数是在  BuiltinFunctionMap 中实现
    */
    auto function_info_it = map().find(keyword);
    if (function_info_it == map().end()) {
        return Error() << StringPrintf("Invalid keyword '%s'", keyword.c_str());
    }

    auto function_info = function_info_it->second;

    auto min_args = std::get<0>(function_info);
    auto max_args = std::get<1>(function_info);
    if (min_args == max_args && num_args != min_args) {
        return Error() << StringPrintf("%s requires %zu argument%s", keyword.c_str(), min_args, (min_args > 1 || min_args == 0) ? "s" : "");
    }

    if (num_args < min_args || num_args > max_args) {
        if (max_args == std::numeric_limits<decltype(max_args)>::max()) {
            return Error() << StringPrintf("%s requires at least %zu argument%s", keyword.c_str(), min_args, min_args > 1 ? "s" : "");
        } else {
            return Error() << StringPrintf("%s requires between %zu and %zu arguments", keyword.c_str(), min_args, max_args);
        }
    }

    return std::get<Function>(function_info);
}


const BuiltinFunctionMap::Map& BuiltinFunctionMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map builtin_functions = {
        {"bootchart",               {1,     1,    {false,  do_bootchart}}},
        {"chmod",                   {2,     2,    {true,   do_chmod}}},
        {"chown",                   {2,     3,    {true,   do_chown}}},
        {"class_reset",             {1,     1,    {false,  do_class_reset}}},
        {"class_restart",           {1,     1,    {false,  do_class_restart}}},
        {"class_start",             {1,     1,    {false,  do_class_start}}},
        {"class_stop",              {1,     1,    {false,  do_class_stop}}},
        {"copy",                    {2,     2,    {true,   do_copy}}},
        {"domainname",              {1,     1,    {true,   do_domainname}}},
        {"enable",                  {1,     1,    {false,  do_enable}}},
        {"exec",                    {1,     kMax, {false,  do_exec}}},
        {"exec_background",         {1,     kMax, {false,  do_exec_background}}},
        {"exec_start",              {1,     1,    {false,  do_exec_start}}},
        {"export",                  {2,     2,    {false,  do_export}}},
        {"hostname",                {1,     1,    {true,   do_hostname}}},
        {"ifup",                    {1,     1,    {true,   do_ifup}}},
        {"init_user0",              {0,     0,    {false,  do_init_user0}}},
        {"insmod",                  {1,     kMax, {true,   do_insmod}}},
        {"installkey",              {1,     1,    {false,  do_installkey}}},
        {"load_persist_props",      {0,     0,    {false,  do_load_persist_props}}},
        {"load_system_props",       {0,     0,    {false,  do_load_system_props}}},
        {"loglevel",                {1,     1,    {false,  do_loglevel}}},
        {"mkdir",                   {1,     4,    {true,   do_mkdir}}},
        // TODO: Do mount operations in vendor_init.
        // mount_all is currently too complex to run in vendor_init as it queues action triggers,
        // imports rc scripts, etc.  It should be simplified and run in vendor_init context.
        // mount and umount are run in the same context as mount_all for symmetry.
        {"mount_all",               {1,     kMax, {false,  do_mount_all}}},
        {"mount",                   {3,     kMax, {false,  do_mount}}},
        {"umount",                  {1,     1,    {false,  do_umount}}},
        {"readahead",               {1,     2,    {true,   do_readahead}}},
        {"restart",                 {1,     1,    {false,  do_restart}}},
        {"restorecon",              {1,     kMax, {true,   do_restorecon}}},
        {"restorecon_recursive",    {1,     kMax, {true,   do_restorecon_recursive}}},
        {"rm",                      {1,     1,    {true,   do_rm}}},
        {"rmdir",                   {1,     1,    {true,   do_rmdir}}},
        {"setprop",                 {2,     2,    {true,   do_setprop}}},
        {"setrlimit",               {3,     3,    {false,  do_setrlimit}}},
        {"start",                   {1,     1,    {false,  do_start}}},
        {"stop",                    {1,     1,    {false,  do_stop}}},
        {"swapon_all",              {1,     1,    {false,  do_swapon_all}}},
        {"symlink",                 {2,     2,    {true,   do_symlink}}},
        {"sysclktz",                {1,     1,    {false,  do_sysclktz}}},
        {"trigger",                 {1,     1,    {false,  do_trigger}}},
        {"verity_load_state",       {0,     0,    {false,  do_verity_load_state}}},
        {"verity_update_state",     {0,     0,    {false,  do_verity_update_state}}},
        {"wait",                    {1,     2,    {true,   do_wait}}},
        {"wait_for_prop",           {2,     2,    {false,  do_wait_for_prop}}},
        {"write",                   {2,     2,    {true,   do_write}}},
    };
    // clang-format on
    return builtin_functions;
}



