class ActionParser : public SectionParser {}


//  @/work/workcodes/aosp-p9.x-auto-ga/system/core/init/action_parser.cpp
ActionParser::ActionParser(ActionManager* action_manager, std::vector<Subcontext>* subcontexts)
    : action_manager_(action_manager), subcontexts_(subcontexts), action_(nullptr) {}


Result<Success> ParseTriggers(const std::vector<std::string>& args, Subcontext* subcontext,
                std::string* event_trigger,std::map<std::string, std::string>* property_triggers) {
    const static std::string prop_str("property:");
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i].empty()) {
            return Error() << "empty trigger is not valid";
        }

        /**
         * 可以在 system/core/rootdir/init.usb.rc 找到模本分析
         * 例如：   on property:sys.usb.config=adb && property:sys.usb.configfs=0
         * on 再该函数调用前已经去除，这里只用分析 property:sys.usb.config=adb && property:sys.usb.configfs=0
         */
        if (i % 2) {
            if (args[i] != "&&") {
                return Error() << "&& is the only symbol allowed to concatenate actions";
            } else {
                continue;
            }
        }

        if (!args[i].compare(0, prop_str.length(), prop_str)) {
            if (auto result = ParsePropertyTrigger(args[i], subcontext, property_triggers);
                !result) {
                return result;
            }
        } else {
            if (!event_trigger->empty()) {
                return Error() << "multiple event triggers are not allowed";
            }

            *event_trigger = args[i];
        }
    }

    return Success();
}


Result<Success> ParsePropertyTrigger(const std::string& trigger, Subcontext* subcontext,
                                     std::map<std::string, std::string>* property_triggers) {
    const static std::string prop_str("property:");
    std::string prop_name(trigger.substr(prop_str.length()));// 去除 "property:"
    size_t equal_pos = prop_name.find('=');
    if (equal_pos == std::string::npos) {
        return Error() << "property trigger found without matching '='";
    }

    std::string prop_value(prop_name.substr(equal_pos + 1));// 得到 prop 的值
    prop_name.erase(equal_pos);

    if (!IsActionableProperty(subcontext, prop_name)) {
        return Error() << "unexported property tigger found: " << prop_name;
    }

    if (auto [it, inserted] = property_triggers->emplace(prop_name, prop_value); !inserted) {
        return Error() << "multiple property triggers found for same property";
    }
    return Success();
}

bool IsActionableProperty(Subcontext* subcontext, const std::string& prop_name) {
    static bool enabled = GetBoolProperty("ro.actionable_compatible_property.enabled", false);  // true

    if (subcontext == nullptr || !enabled) {
        return true;
    }

    //  @   system/core/init/stable_properties.h
    if (kExportedActionableProperties.count(prop_name) == 1) {
        return true;
    }

    //  @   system/core/init/stable_properties.h
    for (const auto& prefix : kPartnerPrefixes) {
        if (android::base::StartsWith(prop_name, prefix)) {
            return true;
        }
    }
    return false;
}


Result<Success> ActionParser::ParseSection(std::vector<std::string>&& args,const std::string& filename, int line) {
    // 注意 这里args 加 1 了
    std::vector<std::string> triggers(args.begin() + 1, args.end());
    if (triggers.size() < 1) {
        return Error() << "Actions must have a trigger";
    }

    Subcontext* action_subcontext = nullptr;
    if (subcontexts_) {
        for (auto& subcontext : *subcontexts_) {
            /**
             * 如果 filename 是以 /vendor 或者 、/oem 开头
             */
            if (StartsWith(filename, subcontext.path_prefix())) {
                action_subcontext = &subcontext;
                break;
            }
        }
    }

    std::string event_trigger;
    std::map<std::string, std::string> property_triggers;

    if (auto result = ParseTriggers(triggers, action_subcontext, &event_trigger, &property_triggers);
        !result) {
        return Error() << "ParseTriggers() failed: " << result.error();
    }


    //  @   system/core/init/action.cpp
    auto action = std::make_unique<Action>(false, action_subcontext, filename, line, event_trigger,property_triggers);

    action_ = std::move(action);
    return Success();
}

//  @   system/core/init/action.cpp
Action::Action(bool oneshot, Subcontext* subcontext, const std::string& filename /*对应的文件名 */, int line /* 对应文件的行号 */,
               const std::string& event_trigger /* on 后面的字符名 */,
               const std::map<std::string, std::string>& property_triggers)
    : property_triggers_(property_triggers),
      event_trigger_(event_trigger),
      oneshot_(oneshot),
      subcontext_(subcontext),
      filename_(filename),
      line_(line) {}


Result<Success> ActionParser::ParseLineSection(std::vector<std::string>&& args, int line) {
    return action_ ? action_->AddCommand(std::move(args), line) : Success();
}

//  @   system/core/init/action.cpp
Result<Success> Action::AddCommand(const std::vector<std::string>& args, int line) {
    /**
     * function_map_ 实在 init.cpp 中 的main函数中静态初始化的
     */
    if (!function_map_) {
        return Error() << "no function map available";
    }

    auto function = function_map_->FindFunction(args);
    if (!function) return Error() << function.error();

    commands_.emplace_back(function->second, function->first, args, line);
    return Success();
}



Result<Success> ActionParser::EndSection() {
    if (action_ && action_->NumCommands() > 0) {
        action_manager_->AddAction(std::move(action_));
    }

    return Success();
}