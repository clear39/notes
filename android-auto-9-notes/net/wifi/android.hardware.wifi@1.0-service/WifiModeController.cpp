//  @   /work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/wifi/1.2/default/wifi_mode_controller.cpp

//  DriverTool @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/opt/net/wifi/libwifi_hal/driver_tool.cpp  
//  DriverTool 封装了驱动wifi驱动加载和卸载  
WifiModeController::WifiModeController() : driver_tool_(new DriverTool) {
    LOG(DEBUG) << "WifiModeController Create";
}

// 这里是在wifi类中的 startInternal-------> initializeModeControllerAndLegacyHal 中调用
bool WifiModeController::initialize() {
    LOG(DEBUG) << "initialize WifiModeController";
    // 驱动加载
    if (!driver_tool_->LoadDriver()) {
        LOG(ERROR) << "Failed to load WiFi driver";
        return false;
    }
    return true;
}