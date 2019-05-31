//  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/health/2.0/default/Health.cpp
Health::Health(struct healthd_config* c) {
    // TODO(b/69268160): remove when libhealthd is removed.
    healthd_board_init(c);   //???????????????
    battery_monitor_ = std::make_unique<BatteryMonitor>();
    battery_monitor_->init(c);
}









Return<Result> Health::update() {
    if (!healthd_mode_ops || !healthd_mode_ops->battery_update) {
        LOG(WARNING) << "health@2.0: update: not initialized. "　<< "update() should not be called in charger / recovery.";
        return Result::UNKNOWN;
    }

    // Retrieve all information and call healthd_mode_ops->battery_update, which calls
    // notifyListeners.
    bool chargerOnline = battery_monitor_->update();

    // adjust uevent / wakealarm periods
    //  @hardware/interfaces/health/2.0/default/healthd_common.cpp
    healthd_battery_update_internal(chargerOnline);

    return Result::SUCCESS;
}



