//  @ system/core/init/property_service.cpp

void property_init() {
    mkdir("/dev/__properties__", S_IRWXU | S_IXGRP | S_IXOTH);
    /**
     * 
     */
    CreateSerializedPropertyInfo();
    if (__system_property_area_init()) {
        LOG(FATAL) << "Failed to initialize property area";
    }
    if (!property_info_area.LoadDefaultPath()) {
        LOG(FATAL) << "Failed to load serialized property info file";
    }
}

void property_load_boot_defaults() {
    if (!load_properties_from_file("/system/etc/prop.default", NULL)) {  // 存在
        // Try recovery path
        if (!load_properties_from_file("/prop.default", NULL)) {
            // Try legacy path
            load_properties_from_file("/default.prop", NULL);
        }
    }
    load_properties_from_file("/product/build.prop", NULL);  // 存在
    load_properties_from_file("/odm/default.prop", NULL);
    load_properties_from_file("/vendor/default.prop", NULL);  //存在

    update_sys_usb_config();
}

// persist.sys.usb.config values can't be combined on build-time when property
// files are split into each partition.
// So we need to apply the same rule of build/make/tools/post_process_props.py
// on runtime.
static void update_sys_usb_config() {
    bool is_debuggable = android::base::GetBoolProperty("ro.debuggable", false);
    std::string config = android::base::GetProperty("persist.sys.usb.config", "");
    if (config.empty()) {
        property_set("persist.sys.usb.config", is_debuggable ? "adb" : "none");
    } else if (is_debuggable && config.find("adb") == std::string::npos && config.length() + 4 < PROP_VALUE_MAX) {
        config.append(",adb");
        property_set("persist.sys.usb.config", config);
    }
}



void start_property_service() {
    selinux_callback cb;
    cb.func_audit = SelinuxAuditCallback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);

    property_set("ro.property_service.version", "2");

    property_set_fd = CreateSocket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,false, 0666, 0, 0, nullptr);
    if (property_set_fd == -1) {
        PLOG(FATAL) << "start_property_service socket creation failed";
    }

    listen(property_set_fd, 8);

    /**
     * @   system/core/init/property_service.cpp
     * 添加 epoll 监视器中
     */  
    register_epoll_handler(property_set_fd, handle_property_set_fd);
}



static void handle_property_set_fd() {
    static constexpr uint32_t kDefaultSocketTimeout = 2000; /* ms */

    int s = accept4(property_set_fd, nullptr, nullptr, SOCK_CLOEXEC);
    if (s == -1) {
        return;
    }

    ucred cr;
    socklen_t cr_size = sizeof(cr);
    if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cr, &cr_size) < 0) {
        close(s);
        PLOG(ERROR) << "sys_prop: unable to get SO_PEERCRED";
        return;
    }

    SocketConnection socket(s, cr);
    uint32_t timeout_ms = kDefaultSocketTimeout;

    uint32_t cmd = 0;
    if (!socket.RecvUint32(&cmd, &timeout_ms)) {
        PLOG(ERROR) << "sys_prop: error while reading command from the socket";
        socket.SendUint32(PROP_ERROR_READ_CMD);
        return;
    }

    switch (cmd) {
    case PROP_MSG_SETPROP: {
        char prop_name[PROP_NAME_MAX];
        char prop_value[PROP_VALUE_MAX];

        if (!socket.RecvChars(prop_name, PROP_NAME_MAX, &timeout_ms) ||
            !socket.RecvChars(prop_value, PROP_VALUE_MAX, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP): error while reading name/value from the socket";
          return;
        }

        prop_name[PROP_NAME_MAX-1] = 0;
        prop_value[PROP_VALUE_MAX-1] = 0;

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result = HandlePropertySet(prop_name, prop_value, socket.source_context(), cr, &error);
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << prop_name << "' to '" << prop_value
                       << "' from uid:" << cr.uid << " gid:" << cr.gid << " pid:" << cr.pid << ": "
                       << error;
        }

        break;
      }

    case PROP_MSG_SETPROP2: {
        std::string name;
        std::string value;
        if (!socket.RecvString(&name, &timeout_ms) ||
            !socket.RecvString(&value, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP2): error while reading name/value from the socket";
          socket.SendUint32(PROP_ERROR_READ_DATA);
          return;
        }

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result = HandlePropertySet(name, value, socket.source_context(), cr, &error);
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << name << "' to '" << value
                       << "' from uid:" << cr.uid << " gid:" << cr.gid << " pid:" << cr.pid << ": "
                       << error;
        }
        socket.SendUint32(result);
        break;
      }

    default:
        LOG(ERROR) << "sys_prop: invalid command " << cmd;
        socket.SendUint32(PROP_ERROR_INVALID_CMD);
        break;
    }
}



// This returns one of the enum of PROP_SUCCESS or PROP_ERROR*.
uint32_t HandlePropertySet(const std::string& name, const std::string& value,
                           const std::string& source_context, const ucred& cr, std::string* error) {
    if (!IsLegalPropertyName(name)) {
        *error = "Illegal property name";
        return PROP_ERROR_INVALID_NAME;
    }

    if (StartsWith(name, "ctl.")) {
        if (!CheckControlPropertyPerms(name, value, source_context, cr)) {
            *error = StringPrintf("Invalid permissions to perform '%s' on '%s'", name.c_str() + 4,
                                  value.c_str());
            return PROP_ERROR_HANDLE_CONTROL_MESSAGE;
        }

        HandleControlMessage(name.c_str() + 4, value, cr.pid);
        return PROP_SUCCESS;
    }

    const char* target_context = nullptr;
    const char* type = nullptr;
    property_info_area->GetPropertyInfo(name.c_str(), &target_context, &type);

    if (!CheckMacPerms(name, target_context, source_context.c_str(), cr)) {
        *error = "SELinux permission check failed";
        return PROP_ERROR_PERMISSION_DENIED;
    }

    if (type == nullptr || !CheckType(type, value)) {
        *error = StringPrintf("Property type check failed, value doesn't match expected type '%s'", (type ?: "(null)"));
        return PROP_ERROR_INVALID_VALUE;
    }

    // sys.powerctl is a special property that is used to make the device reboot.  We want to log
    // any process that sets this property to be able to accurately blame the cause of a shutdown.
    if (name == "sys.powerctl") {
        std::string cmdline_path = StringPrintf("proc/%d/cmdline", cr.pid);
        std::string process_cmdline;
        std::string process_log_string;
        if (ReadFileToString(cmdline_path, &process_cmdline)) {
            // Since cmdline is null deliminated, .c_str() conveniently gives us just the process
            // path.
            process_log_string = StringPrintf(" (%s)", process_cmdline.c_str());
        }
        LOG(INFO) << "Received sys.powerctl='" << value << "' from pid: " << cr.pid << process_log_string;
    }

    if (name == "selinux.restorecon_recursive") {
        return PropertySetAsync(name, value, RestoreconRecursiveAsync, error);
    }

    return PropertySet(name, value, error);
}


static uint32_t PropertySet(const std::string& name, const std::string& value, std::string* error) {
    size_t valuelen = value.size();
    /**
     * 校验名字是否符合规则
     */
    if (!IsLegalPropertyName(name)) {
        *error = "Illegal property name";
        return PROP_ERROR_INVALID_NAME;
    }

    /**
     * 值太长或者ro.开头的直接return，不允许修改
     */
    if (valuelen >= PROP_VALUE_MAX && !StartsWith(name, "ro.")) {
        *error = "Property value too long";
        return PROP_ERROR_INVALID_VALUE;
    }

    if (mbstowcs(nullptr, value.data(), 0) == static_cast<std::size_t>(-1)) {
        *error = "Value is not a UTF8 encoded string";
        return PROP_ERROR_INVALID_VALUE;
    }

    prop_info* pi = (prop_info*) __system_property_find(name.c_str());
    if (pi != nullptr) {
        // ro.* properties are actually "write-once".
        if (StartsWith(name, "ro.")) {
            *error = "Read-only property was already set";
            return PROP_ERROR_READ_ONLY_PROPERTY;
        }

        __system_property_update(pi, value.c_str(), valuelen);
    } else {
        int rc = __system_property_add(name.c_str(), name.size(), value.c_str(), valuelen);
        if (rc < 0) {
            *error = "__system_property_add failed";
            return PROP_ERROR_SET_FAILED;
        }
    }

    // Don't write properties to disk until after we have read all default
    // properties to prevent them from being overwritten by default values.
    
    if (persistent_properties_loaded && StartsWith(name, "persist.")) {
        /**
         *  @   system/core/init/persistent_properties.cpp
         * 
         */
        WritePersistentProperty(name, value);
    }
    property_changed(name, value);
    return PROP_SUCCESS;
}


// Persistent properties are not written often, so we rather not keep any data in memory and read
// then rewrite the persistent property file for each update.
void WritePersistentProperty(const std::string& name, const std::string& value) {
    auto persistent_properties = LoadPersistentPropertyFile();

    if (!persistent_properties) {
        LOG(ERROR) << "Recovering persistent properties from memory: " << persistent_properties.error();
        persistent_properties = LoadPersistentPropertiesFromMemory();
    }
    auto it = std::find_if(persistent_properties->mutable_properties()->begin(),
                           persistent_properties->mutable_properties()->end(),
                           [&name](const auto& record) { return record.name() == name; });
    if (it != persistent_properties->mutable_properties()->end()) {
        it->set_name(name);
        it->set_value(value);
    } else {
        AddPersistentProperty(name, value, &persistent_properties.value());
    }

    if (auto result = WritePersistentPropertyFile(*persistent_properties); !result) {
        LOG(ERROR) << "Could not store persistent property: " << result.error();
    }
}


Result<PersistentProperties> LoadPersistentPropertyFile() {
    //  "/data/property/persistent_properties"
    auto file_contents = ReadPersistentPropertyFile();
    if (!file_contents) return file_contents.error();

    PersistentProperties persistent_properties;
    if (persistent_properties.ParseFromString(*file_contents)) return persistent_properties;

    // If the file cannot be parsed in either format, then we don't have any recovery
    // mechanisms, so we delete it to allow for future writes to take place successfully.
    unlink(persistent_property_filename.c_str());
    return Error() << "Unable to parse persistent property file: Could not parse protobuf";
}

Result<std::string> ReadPersistentPropertyFile() {
    //  std::string persistent_property_filename = "/data/property/persistent_properties";
    const std::string temp_filename = persistent_property_filename + ".tmp";
    if (access(temp_filename.c_str(), F_OK) == 0) {
        LOG(INFO) << "Found temporary property file while attempting to persistent system properties" " a previous persistent property write may have failed";
        unlink(temp_filename.c_str());
    }
    auto file_contents = ReadFile(persistent_property_filename);
    if (!file_contents) {
        return Error() << "Unable to read persistent property file: " << file_contents.error();
    }
    return *file_contents;
}


PersistentProperties LoadPersistentPropertiesFromMemory() {
    PersistentProperties persistent_properties;

    //  @ bionic/libc/bionic/system_property_api.cpp
    __system_property_foreach(
        [](const prop_info* pi, void* cookie) {
            __system_property_read_callback(
                pi,
                [](void* cookie, const char* name, const char* value, unsigned serial) {
                    if (StartsWith(name, "persist.")) {
                        auto properties = reinterpret_cast<PersistentProperties*>(cookie);
                        AddPersistentProperty(name, value, properties);
                    }
                },
                cookie);
        },
        &persistent_properties);
    return persistent_properties;
}

