//  @/work/workcodes/aosp-p9.x-auto-alpha/system/core/healthd/BatteryMonitor.cpp

BatteryMonitor::BatteryMonitor()
    : mHealthdConfig(nullptr),
      mBatteryDevicePresent(false),
      mBatteryFixedCapacity(0),
      mBatteryFixedTemperature(0) {
    initBatteryProperties(&props);
}


static void initBatteryProperties(BatteryProperties* props) {
    props->chargerAcOnline = false;
    props->chargerUsbOnline = false;
    props->chargerWirelessOnline = false;
    props->maxChargingCurrent = 0;
    props->maxChargingVoltage = 0;
    props->batteryStatus = BATTERY_STATUS_UNKNOWN;
    props->batteryHealth = BATTERY_HEALTH_UNKNOWN;
    props->batteryPresent = false;
    props->batteryLevel = 0;
    props->batteryVoltage = 0;
    props->batteryTemperature = 0;
    props->batteryCurrent = 0;
    props->batteryCycleCount = 0;
    props->batteryFullCharge = 0;
    props->batteryChargeCounter = 0;
    props->batteryTechnology.clear();
}



void BatteryMonitor::init(struct healthd_config *hc) {
    String8 path;
    char pval[PROPERTY_VALUE_MAX];

    mHealthdConfig = hc;
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(POWER_SUPPLY_SYSFS_PATH), closedir); // @/sys/class/power_supply
    if (dir == NULL) {
        KLOG_ERROR(LOG_TAG, "Could not open %s\n", POWER_SUPPLY_SYSFS_PATH);
    } else {
        struct dirent* entry;

        while ((entry = readdir(dir.get()))) {
            const char* name = entry->d_name;

            if (!strcmp(name, ".") || !strcmp(name, ".."))
                continue;

            // Look for "type" file in each subdirectory
            path.clear();
            path.appendFormat("%s/%s/type", POWER_SUPPLY_SYSFS_PATH, name);  // @/sys/class/power_supply/usb/type  { "Mains", ANDROID_POWER_SUPPLY_TYPE_AC },
            switch(readPowerSupplyType(path)) {
            case ANDROID_POWER_SUPPLY_TYPE_AC:   // 返回这里
            case ANDROID_POWER_SUPPLY_TYPE_USB:
            case ANDROID_POWER_SUPPLY_TYPE_WIRELESS:
                path.clear();
                path.appendFormat("%s/%s/online", POWER_SUPPLY_SYSFS_PATH, name);  // @/sys/class/power_supply/usb/online
                if (access(path.string(), R_OK) == 0)
                    mChargerNames.add(String8(name));　　// usb
                break;

            ......
            }
        }
    }

    // Typically the case for devices which do not have a battery and
    // and are always plugged into AC mains.
    if (!mBatteryDevicePresent) {  // mBatteryDevicePresent = fasle
        KLOG_WARNING(LOG_TAG, "No battery devices found \n");
        hc->periodic_chores_interval_fast = -1;
        hc->periodic_chores_interval_slow = -1;
    } else {
       .....
    }

    if (property_get("ro.boot.fake_battery", pval, NULL) > 0 && strtol(pval, NULL, 10) != 0) {
       ......
    }
}




bool BatteryMonitor::update(void) {
    bool logthis;

    initBatteryProperties(&props);

    if (!mHealthdConfig->batteryPresentPath.isEmpty())
        props.batteryPresent = getBooleanField(mHealthdConfig->batteryPresentPath);
    else
        props.batteryPresent = mBatteryDevicePresent;

    props.batteryLevel = mBatteryFixedCapacity ? mBatteryFixedCapacity : getIntField(mHealthdConfig->batteryCapacityPath);
    props.batteryVoltage = getIntField(mHealthdConfig->batteryVoltagePath) / 1000;

    if (!mHealthdConfig->batteryCurrentNowPath.isEmpty())
        props.batteryCurrent = getIntField(mHealthdConfig->batteryCurrentNowPath) / 1000;

    if (!mHealthdConfig->batteryFullChargePath.isEmpty())
        props.batteryFullCharge = getIntField(mHealthdConfig->batteryFullChargePath);

    if (!mHealthdConfig->batteryCycleCountPath.isEmpty())
        props.batteryCycleCount = getIntField(mHealthdConfig->batteryCycleCountPath);

    if (!mHealthdConfig->batteryChargeCounterPath.isEmpty())
        props.batteryChargeCounter = getIntField(mHealthdConfig->batteryChargeCounterPath);

    props.batteryTemperature = mBatteryFixedTemperature ? mBatteryFixedTemperature : getIntField(mHealthdConfig->batteryTemperaturePath);

    std::string buf;

    if (readFromFile(mHealthdConfig->batteryStatusPath, &buf) > 0)
        props.batteryStatus = getBatteryStatus(buf.c_str());

    if (readFromFile(mHealthdConfig->batteryHealthPath, &buf) > 0)
        props.batteryHealth = getBatteryHealth(buf.c_str());

    if (readFromFile(mHealthdConfig->batteryTechnologyPath, &buf) > 0)
        props.batteryTechnology = String8(buf.c_str());

    unsigned int i;
    double MaxPower = 0;

    for (i = 0; i < mChargerNames.size(); i++) {
        String8 path;
        path.appendFormat("%s/%s/online", POWER_SUPPLY_SYSFS_PATH, mChargerNames[i].string());
        if (getIntField(path)) {
            path.clear();
            path.appendFormat("%s/%s/type", POWER_SUPPLY_SYSFS_PATH,mChargerNames[i].string());
            switch(readPowerSupplyType(path)) {
            case ANDROID_POWER_SUPPLY_TYPE_AC:
                props.chargerAcOnline = true;
                break;
            case ANDROID_POWER_SUPPLY_TYPE_USB:
                props.chargerUsbOnline = true;
                break;
            case ANDROID_POWER_SUPPLY_TYPE_WIRELESS:
                props.chargerWirelessOnline = true;
                break;
            default:
                KLOG_WARNING(LOG_TAG, "%s: Unknown power supply type\n",mChargerNames[i].string());
            }
            path.clear();
            path.appendFormat("%s/%s/current_max", POWER_SUPPLY_SYSFS_PATH,mChargerNames[i].string());
            int ChargingCurrent = (access(path.string(), R_OK) == 0) ? getIntField(path) : 0;

            path.clear();
            path.appendFormat("%s/%s/voltage_max", POWER_SUPPLY_SYSFS_PATH,mChargerNames[i].string());

            int ChargingVoltage = (access(path.string(), R_OK) == 0) ? getIntField(path) : DEFAULT_VBUS_VOLTAGE;

            double power = ((double)ChargingCurrent / MILLION) * ((double)ChargingVoltage / MILLION);
            if (MaxPower < power) {
                props.maxChargingCurrent = ChargingCurrent;
                props.maxChargingVoltage = ChargingVoltage;
                MaxPower = power;
            }
        }
    }

    logthis = !healthd_board_battery_update(&props);

    if (logthis) {
        char dmesgline[256];
        size_t len;
        if (props.batteryPresent) {
            snprintf(dmesgline, sizeof(dmesgline),
                 "battery l=%d v=%d t=%s%d.%d h=%d st=%d",
                 props.batteryLevel, props.batteryVoltage,
                 props.batteryTemperature < 0 ? "-" : "",
                 abs(props.batteryTemperature / 10),
                 abs(props.batteryTemperature % 10), props.batteryHealth,
                 props.batteryStatus);

            len = strlen(dmesgline);
            if (!mHealthdConfig->batteryCurrentNowPath.isEmpty()) {
                len += snprintf(dmesgline + len, sizeof(dmesgline) - len, " c=%d", props.batteryCurrent);
            }

            if (!mHealthdConfig->batteryFullChargePath.isEmpty()) {
                len += snprintf(dmesgline + len, sizeof(dmesgline) - len, " fc=%d", props.batteryFullCharge);
            }

            if (!mHealthdConfig->batteryCycleCountPath.isEmpty()) {
                len += snprintf(dmesgline + len, sizeof(dmesgline) - len, " cc=%d", props.batteryCycleCount);
            }
        } else {
            len = snprintf(dmesgline, sizeof(dmesgline), "battery none");
        }

        snprintf(dmesgline + len, sizeof(dmesgline) - len, " chg=%s%s%s",
                 props.chargerAcOnline ? "a" : "",
                 props.chargerUsbOnline ? "u" : "",
                 props.chargerWirelessOnline ? "w" : "");

        KLOG_WARNING(LOG_TAG, "%s\n", dmesgline);
    }

    healthd_mode_ops->battery_update(&props);
    return props.chargerAcOnline | props.chargerUsbOnline | props.chargerWirelessOnline;
}