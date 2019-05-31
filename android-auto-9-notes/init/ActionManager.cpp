//  @/work/workcodes/aosp-p9.x-auto-beta/system/core/init/action_manager.cpp
ActionManager::ActionManager() : current_command_(0) {}

ActionManager& ActionManager::GetInstance() {
    static ActionManager instance;
    return instance;
}


