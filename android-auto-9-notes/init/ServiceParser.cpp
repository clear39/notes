class ServiceParser : public SectionParser {}

//  @/work/workcodes/aosp-p9.x-auto-ga/system/core/init/service.cpp
ServiceParser::ServiceParser(ServiceList* service_list, std::vector<Subcontext>* subcontexts)
        : service_list_(service_list), subcontexts_(subcontexts), service_(nullptr) {}


/**
 * 例： service zygote /system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server --socket-name=zygote
 */
Result<Success> ServiceParser::ParseSection(std::vector<std::string>&& args,const std::string& filename, int line) {
    if (args.size() < 3) {
        return Error() << "services must have a name and a program";
    }

    const std::string& name = args[1];
    if (!IsValidName(name)) {
        return Error() << "invalid service name '" << name << "'";
    }

    Subcontext* restart_action_subcontext = nullptr;
    if (subcontexts_) {
        for (auto& subcontext : *subcontexts_) {
            if (StartsWith(filename, subcontext.path_prefix())) {
                restart_action_subcontext = &subcontext;
                break;
            }
        }
    }

    std::vector<std::string> str_args(args.begin() + 2, args.end());
    service_ = std::make_unique<Service>(name, restart_action_subcontext, str_args);
    return Success();
}

bool ServiceParser::IsValidName(const std::string& name) const {
    // Property names can be any length, but may only contain certain characters.
    // Property values can contain any characters, but may only be a certain length.
    // (The latter restriction is needed because `start` and `stop` work by writing
    // the service name to the "ctl.start" and "ctl.stop" properties.)
    //  @   system/core/init/util.cpp
    return IsLegalPropertyName("init.svc." + name) && name.size() <= PROP_VALUE_MAX;
}

Service::Service(const std::string& name, Subcontext* subcontext_for_restart_commands,const std::vector<std::string>& args)
    : Service(name, 0, 0, 0, {}, 0, 0, "", subcontext_for_restart_commands, args) {}


Service::Service(const std::string& name, unsigned flags, uid_t uid, gid_t gid,
                 const std::vector<gid_t>& supp_gids, const CapSet& capabilities,
                 unsigned namespace_flags, const std::string& seclabel,
                 Subcontext* subcontext_for_restart_commands, const std::vector<std::string>& args)
    : name_(name),
      classnames_({"default"}),
      flags_(flags),
      pid_(0),
      crash_count_(0),
      uid_(uid),
      gid_(gid),
      supp_gids_(supp_gids),
      capabilities_(capabilities),
      namespace_flags_(namespace_flags),
      seclabel_(seclabel),
      onrestart_(false, subcontext_for_restart_commands, "<Service '" + name + "' onrestart>", 0,"onrestart", {}),
      keychord_id_(0),
      ioprio_class_(IoSchedClass_NONE),
      ioprio_pri_(0),
      priority_(0),
      oom_score_adjust_(-1000),
      swappiness_(-1),
      soft_limit_in_bytes_(-1),
      limit_in_bytes_(-1),
      start_order_(0),
      args_(args) {}



Result<Success> ServiceParser::ParseLineSection(std::vector<std::string>&& args, int line) {
    return service_ ? service_->ParseLine(std::move(args)) : Success();
}


Result<Success> Service::ParseLine(const std::vector<std::string>& args) {
    static const OptionParserMap parser_map;
    auto parser = parser_map.FindFunction(args);

    if (!parser) return parser.error();

    return std::invoke(*parser, this, args);
}


Result<Success> ServiceParser::EndSection() {
    if (service_) {
        Service* old_service = service_list_->FindService(service_->name());
        if (old_service) {
            if (!service_->is_override()) {
                return Error() << "ignored duplicate definition of service '" << service_->name()
                               << "'";
            }

            service_list_->RemoveService(*old_service);
            old_service = nullptr;
        }

        service_list_->AddService(std::move(service_));
    }

    return Success();
}