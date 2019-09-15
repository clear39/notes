//  @/work/workcodes/aosp-p9.x-auto-beta/system/core/init/action_manager.cpp
ActionManager::ActionManager() : current_command_(0) {}

ActionManager& ActionManager::GetInstance() {
    static ActionManager instance;
    return instance;
}

/**
 * 这里在 ActionParser::EndSection() 调用
 */
void ActionManager::AddAction(std::unique_ptr<Action> action) {
    actions_.emplace_back(std::move(action));
}



/**
 * am.QueueEventTrigger("early-init");
 * am.QueueEventTrigger("init");
 * am.QueueEventTrigger("late-init");
 */
void ActionManager::QueueEventTrigger(const std::string& trigger) {
    /**
     * std::queue<std::variant<EventTrigger, PropertyChange, BuiltinAction>> event_queue_; 
     * std::variant 相当于 union 类型
     * 
     * system/core/init/action.h:54:using EventTrigger = std::string;
     * system/core/init/action.h:55:using PropertyChange = std::pair<std::string, std::string>;
     * system/core/init/action.h:56:using BuiltinAction = class Action*;
     * 
     *  */  
    event_queue_.emplace(trigger);
}

/**
 * func 处理的命令回调函数
 * name 为 event_trigger 
*/
void ActionManager::QueueBuiltinAction(BuiltinFunction func, const std::string& name) {
    auto action = std::make_unique<Action>(true, nullptr, "<Builtin Action>", 0, name,std::map<std::string, std::string>{});
    std::vector<std::string> name_vector{name};

    action->AddCommand(func, name_vector, 0);

    event_queue_.emplace(action.get());

    actions_.emplace_back(std::move(action));
}

//  @   /work/workcodes/aosp-p9.x-auto-ga/system/core/init/action.cpp
Action::Action(bool oneshot, Subcontext* subcontext, const std::string& filename, int line,
               const std::string& event_trigger,
               const std::map<std::string, std::string>& property_triggers)
    : property_triggers_(property_triggers),
      event_trigger_(event_trigger),
      oneshot_(oneshot),
      subcontext_(subcontext),
      filename_(filename),
      line_(line) {}

void Action::AddCommand(BuiltinFunction f, const std::vector<std::string>& args, int line) {
    commands_.emplace_back(f, false, args, line);
}

/***
 * 这里在 init main函数的while循环中调用，用来判断当前是否有未处理的消息
 * */
bool ActionManager::HasMoreCommands() const {
    return !current_executing_actions_.empty() || !event_queue_.empty();
}

void ActionManager::ExecuteOneCommand() {
    LOG(DEBUG) << "ExecuteOneCommand ...";
    // Loop through the event queue until we have an action to execute
    /***
     * system/core/init/action_manager.h:52:    std::queue<const Action*> current_executing_actions_;
     */
    while (current_executing_actions_.empty() && !event_queue_.empty()) {
        for (const auto& action : actions_) {
            /**
             * std::visit 将后面第二个开始作为参数传入第一个参数（指针函数）中进行调用
             * 由于 event_queue_队列中每个成员都是union类型，所以CheckEvent会有三种调用函数
             * action->CheckEvent(event)
             * 如果 event_queue_ 队列中的 成员 和当前action 匹配，这将当前action压入 current_executing_actions_ 中
             */
            if (std::visit([&action](const auto& event) { return action->CheckEvent(event); },event_queue_.front())) {
                current_executing_actions_.emplace(action.get());
            }
        }
        event_queue_.pop();
    }

    if (current_executing_actions_.empty()) {
        return;
    }

    auto action = current_executing_actions_.front();

    if (current_command_ == 0) {
        std::string trigger_name = action->BuildTriggersString();
        LOG(INFO) << "processing action (" << trigger_name << ") from (" << action->filename() << ":" << action->line() << ")";
    }

    action->ExecuteOneCommand(current_command_);

    // If this was the last command in the current action, then remove
    // the action from the executing list.
    // If this action was oneshot, then also remove it from actions_.
    ++current_command_;
    if (current_command_ == action->NumCommands()) {
        current_executing_actions_.pop();
        current_command_ = 0;
        /**
         * 启动一次之后，将oneshot 命令移除
         */
        if (action->oneshot()) {
            auto eraser = [&action](std::unique_ptr<Action>& a) { return a.get() == action; };
            actions_.erase(std::remove_if(actions_.begin(), actions_.end(), eraser));
        }
    }
}


bool Action::CheckEvent(const EventTrigger& event_trigger) const {
    return event_trigger == event_trigger_ && CheckPropertyTriggers();
}

bool Action::CheckEvent(const PropertyChange& property_change) const {
    const auto& [name, value] = property_change;
    return event_trigger_.empty() && CheckPropertyTriggers(name, value);
}

bool Action::CheckEvent(const BuiltinAction& builtin_action) const {
    return this == builtin_action;
}


void Action::ExecuteOneCommand(std::size_t command) const {
    // We need a copy here since some Command execution may result in
    // changing commands_ vector by importing .rc files through parser
    Command cmd = commands_[command];
    ExecuteCommand(cmd);
}

void Action::ExecuteCommand(const Command& command) const {
    android::base::Timer t;
    /**
     * 函数调用
     */
    auto result = command.InvokeFunc(subcontext_);
    auto duration = t.duration();

    // There are many legacy paths in rootdir/init.rc that will virtually never exist on a new
    // device, such as '/sys/class/leds/jogball-backlight/brightness'.  As of this writing, there
    // are 198 such failures on bullhead.  Instead of spamming the log reporting them, we do not
    // report such failures unless we're running at the DEBUG log level.
    bool report_failure = !result.has_value();
    if (report_failure && android::base::GetMinimumLogSeverity() > android::base::DEBUG &&
        result.error_errno() == ENOENT) {
        report_failure = false;
    }

    // Any action longer than 50ms will be warned to user as slow operation
    if (report_failure || duration > 50ms ||
        android::base::GetMinimumLogSeverity() <= android::base::DEBUG) {
        std::string trigger_name = BuildTriggersString();
        std::string cmd_str = command.BuildCommandString();

        LOG(INFO) << "Command '" << cmd_str << "' action=" << trigger_name << " (" << filename_
                  << ":" << command.line() << ") took " << duration.count() << "ms and "
                  << (result ? "succeeded" : "failed: " + result.error_string());
    }
}


std::size_t Action::NumCommands() const {
    return commands_.size();
}

bool Action::oneshot() const { return oneshot_; }