//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/native/services/thermalservice
/***
 * 
 * 用于Soc温度和相关参数
 */

/**
service thermalservice /system/bin/thermalserviced
    class core
    user system
    group system
*/


/*  thermal  
adj.
热的，保热的;温热的
n.
上升的暖气流
*/


//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/native/services/thermalservice/thermalserviced.cpp
int main(int /*argc*/, char** /*argv*/) {
    gThermalServiceDaemon = new ThermalServiceDaemon();
    gThermalServiceDaemon->thermalCallbackStartup();
    gThermalServiceDaemon->thermalServiceStartup();
    /* NOTREACHED */
}




void ThermalServiceDaemon::thermalCallbackStartup() {
    // HIDL IThermalCallback startup
    // Need at least 2 threads in thread pool since we wait for dead HAL
    // to come back on the binder death notification thread and we need
    // another thread for the incoming service now available call.
    configureRpcThreadpool(2, false /* callerWillJoin */);

    //  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/native/services/thermalservice/libthermalcallback/ThermalCallback.cpp
    mThermalCallback = new ThermalCallback();
    // Lookup Thermal HAL and register our ThermalCallback.
    getThermalHal();
}


// Lookup Thermal HAL, register death notifier, register our
// ThermalCallback with the Thermal HAL.
void ThermalServiceDaemon::getThermalHal() {
    //  @/work/workcodes/aosp-p9.x-auto-alpha/hardware/interfaces/thermal/1.0/IThermal.hal
    gThermalHal = IThermal::getService();
    if (gThermalHal == nullptr) {
        ALOGW("Unable to get Thermal HAL V1.1, vendor thermal event notification not available");
        return;
    }

    // Binder death notifier for Thermal HAL
    if (gThermalHalDied == nullptr)
        gThermalHalDied = new ThermalServiceDeathRecipient();

    if (gThermalHalDied != nullptr)
        gThermalHal->linkToDeath(gThermalHalDied, 0x451F /* cookie */);

    if (mThermalCallback != nullptr) {
        /**
         * 注意 这里 IThermal 为 1.1版本，1.0没有 registerThermalCallback 接口
         * */
        Return<void> ret = gThermalHal->registerThermalCallback(mThermalCallback);
        if (!ret.isOk())
            ALOGE("registerThermalCallback failed, status: %s",ret.description().c_str());
    }
}


void ThermalServiceDaemon::thermalServiceStartup() {
    // Binder IThermalService startup
    mThermalService = new android::os::ThermalService;
    mThermalService->publish(mThermalService);
    // Register IThermalService object with IThermalCallback
    if (mThermalCallback != nullptr)
        mThermalCallback->registerThermalService(mThermalService);
    IPCThreadState::self()->joinThreadPool();
}
