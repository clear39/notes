/***
 * Subcontext 初始化：
 * main() ---> InitializeSubcontexts() 得到 subcontexts
 * 在进行 init.rc 文件解析时 main() --> LoadBootScripts() -->  CreateParser() 中 将 subcontexts 作为参数传递给 ServiceParser 和 ActionParser
 * 在通过 init.rc文件的路径来判断是否需要设置 subcontexts （如果是/vendor或者/oem开头，则进行设置）
 * 
 * 
 * 调用时：
 *    每个 Action(Action在解析构建时，会将 subcontexts 传递进来)的自命令Command执行时，
 *    在Command::InvokeFunc(Subcontext* subcontext)中，先去判断subcontext是否为空，
 *    如果为空则使用 kInitContext 代替
 * **/



//  @ system/core/init/subcontext.cpp


const std::string kInitContext = "u:r:init:s0";
const std::string kVendorContext = "u:r:vendor_init:s0";

const char* const paths_and_secontexts[2][2] = {
    {"/vendor", kVendorContext.c_str()},
    {"/odm", kVendorContext.c_str()},
};


/**
 * @ system/core/init/init.cpp:712:    subcontexts = InitializeSubcontexts();
*/
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


// This function checks whether the sepolicy supports vendor init.
bool SelinuxHasVendorInit() {
    /**
     * IsSplitPolicyDevice 判断 /system/etc/selinux/plat_sepolicy.cil 文件是否存在
     * 这里存在，IsSplitPolicyDevice 返回true
    */
    if (!IsSplitPolicyDevice()) {
        // If this device does not split sepolicy files, vendor_init will be available in the latest
        // monolithic sepolicy file.
        return true;
    }

    std::string version;
    /**
     * /vendor/etc/selinux/plat_sepolicy_vers.txt 读取版本 存入 version中
     * 这里为 28.0
    */
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

//  @ system/core/init/selinux.cpp
//constexpr const char plat_policy_cil_file[] = "/system/etc/selinux/plat_sepolicy.cil";
bool IsSplitPolicyDevice() {
    /**
     * 该文件存在 
    */
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







///////////////////////////////////////////////////////////////////////////////////////////////////









//  @   system/core/init/subcontext.cpp
/**
 * 
 *      在init --> main中调用， 
 *      const BuiltinFunctionMap function_map;  //  system/core/init/builtins.cpp
        return SubcontextMain(argc, argv, &function_map);
 * 
*/
int SubcontextMain(int argc, char** argv, const KeywordFunctionMap* function_map) {
    LOG(DEBUG) << "SubcontextMain..." ;
    if (argc < 4) LOG(FATAL) << "Fewer than 4 args specified to subcontext (" << argc << ")";

    auto context = std::string(argv[2]);
    auto init_fd = std::atoi(argv[3]);

    SelabelInitialize();

    property_set = SubcontextPropertySet;

    // SubcontextProcess @ system/core/init/subcontext.cpp
    auto subcontext_process = SubcontextProcess(function_map, context, init_fd);
    subcontext_process.MainLoop();
    return 0;
}

//  @   system/core/init/subcontext.cpp

SubcontextProcess::SubcontextProcess(const KeywordFunctionMap* function_map, std::string context, int init_fd)
        : function_map_(function_map), context_(std::move(context)), init_fd_(init_fd){
        
};


void SubcontextProcess::MainLoop() {

    LOG(DEBUG) << "SubcontextProcess::MainLoop()...";


    pollfd ufd[1];
    ufd[0].events = POLLIN;
    ufd[0].fd = init_fd_;

    while (true) {
        ufd[0].revents = 0;
        int nr = TEMP_FAILURE_RETRY(poll(ufd, arraysize(ufd), -1));
        if (nr == 0) continue;
        if (nr < 0) {
            PLOG(FATAL) << "poll() of subcontext socket failed, continuing";
        }

        auto init_message = ReadMessage(init_fd_);
        if (!init_message) {
            LOG(FATAL) << "Could not read message from init: " << init_message.error();
        }

        //  @   system/core/init/subcontext.proto
        auto subcontext_command = SubcontextCommand();
        if (!subcontext_command.ParseFromString(*init_message)) {
            LOG(FATAL) << "Unable to parse message from init";
        }

        //  @   system/core/init/subcontext.proto
        auto reply = SubcontextReply();
        switch (subcontext_command.command_case()) {
            case SubcontextCommand::kExecuteCommand: {
                RunCommand(subcontext_command.execute_command(), &reply);
                break;
            }
            case SubcontextCommand::kExpandArgsCommand: {
                ExpandArgs(subcontext_command.expand_args_command(), &reply);
                break;
            }
            default:
                LOG(FATAL) << "Unknown message type from init: "  << subcontext_command.command_case();
        }

        if (auto result = SendMessage(init_fd_, reply); !result) {
            LOG(FATAL) << "Failed to send message to init: " << result.error();
        }
    }
}

//  @   system/core/init/subcontext.cpp
Result<std::string> ReadMessage(int socket) {
    char buffer[kBufferSize] = {};  //  kBufferSize = 4096;
    auto result = TEMP_FAILURE_RETRY(recv(socket, buffer, sizeof(buffer), 0));
    if (result <= 0) {
        return ErrnoError();
    }
    return std::string(buffer, result);
}


void SubcontextProcess::RunCommand(const SubcontextCommand::ExecuteCommand& execute_command,SubcontextReply* reply) const {
    LOG(DEBUG) << "SubcontextProcess::RunCommand ...";
    // Need to use ArraySplice instead of this code.
    auto args = std::vector<std::string>();
    for (const auto& string : execute_command.args()) {
        args.emplace_back(string);
    }

    /**
     * 在 函数  BuiltinFunctionMap::map() 中构造的 map 中查找对应名称的 接口函数
    */
    auto map_result = function_map_->FindFunction(args);
    Result<Success> result;
    if (!map_result) {
        result = Error() << "Cannot find command: " << map_result.error();
    } else {
        /***
         * @    system/core/init/action.cpp
        */
        result = RunBuiltinFunction(map_result->second, args, context_);
    }

    for (const auto& [name, value] : properties_to_set) {
        auto property = reply->add_properties_to_set();
        property->set_name(name);
        property->set_value(value);
    }

    properties_to_set.clear();

    if (result) {
        reply->set_success(true);
    } else {
        auto* failure = reply->mutable_failure();
        failure->set_error_string(result.error_string());
        failure->set_error_errno(result.error_errno());
    }
}

//  @  system/core/init/action.cpp 
Result<Success> RunBuiltinFunction(const BuiltinFunction& function,
                                   const std::vector<std::string>& args,
                                   const std::string& context) {
    auto builtin_arguments = BuiltinArguments(context);

    builtin_arguments.args.resize(args.size());
    builtin_arguments.args[0] = args[0];
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!expand_props(args[i], &builtin_arguments.args[i])) {
            return Error() << "cannot expand '" << args[i] << "'";
        }
    }

    return function(builtin_arguments);
}







////////////////////////////////////////////////
void SubcontextProcess::ExpandArgs(const SubcontextCommand::ExpandArgsCommand& expand_args_command,SubcontextReply* reply) const {
    LOG(DEBUG) << "SubcontextProcess::ExpandArgs ...";
    for (const auto& arg : expand_args_command.args()) {
        auto expanded_prop = std::string{};
        //  @   system/core/init/util.cpp
        if (!expand_props(arg, &expanded_prop)) {
            auto* failure = reply->mutable_failure();
            failure->set_error_string("Failed to expand '" + arg + "'");
            failure->set_error_errno(0);
            return;
        } else {
            auto* expand_args_reply = reply->mutable_expand_args_reply();
            expand_args_reply->add_expanded_args(expanded_prop);
        }
    }
}


bool expand_props(const std::string& src, std::string* dst) {
    const char* src_ptr = src.c_str();

    if (!dst) {
        return false;
    }

    /* - variables can either be $x.y or ${x.y}, in case they are only part
     *   of the string.
     * - will accept $$ as a literal $.
     * - no nested property expansion, i.e. ${foo.${bar}} is not supported,
     *   bad things will happen
     * - ${x.y:-default} will return default value if property empty.
     */
    while (*src_ptr) {
        const char* c;

        c = strchr(src_ptr, '$');
        if (!c) {
            dst->append(src_ptr);
            return true;
        }

        dst->append(src_ptr, c);
        c++;

        if (*c == '$') {
            dst->push_back(*(c++));
            src_ptr = c;
            continue;
        } else if (*c == '\0') {
            return true;
        }

        std::string prop_name;
        std::string def_val;
        if (*c == '{') {
            c++;
            const char* end = strchr(c, '}');
            if (!end) {
                // failed to find closing brace, abort.
                LOG(ERROR) << "unexpected end of string in '" << src << "', looking for }";
                return false;
            }
            prop_name = std::string(c, end);
            c = end + 1;
            size_t def = prop_name.find(":-");
            if (def < prop_name.size()) {
                def_val = prop_name.substr(def + 2);
                prop_name = prop_name.substr(0, def);
            }
        } else {
            prop_name = c;
            LOG(ERROR) << "using deprecated syntax for specifying property '" << c << "', use ${name} instead";
            c += prop_name.size();
        }

        if (prop_name.empty()) {
            LOG(ERROR) << "invalid zero-length property name in '" << src << "'";
            return false;
        }

        std::string prop_val = android::base::GetProperty(prop_name, "");
        if (prop_val.empty()) {
            if (def_val.empty()) {
                LOG(ERROR) << "property '" << prop_name << "' doesn't exist while expanding '" << src << "'";
                return false;
            }
            prop_val = def_val;
        }

        dst->append(prop_val);
        src_ptr = c;
    }

    return true;
}
                                      