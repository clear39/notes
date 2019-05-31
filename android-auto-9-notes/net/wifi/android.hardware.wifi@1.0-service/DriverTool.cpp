//  @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/opt/net/wifi/libwifi_hal/driver_tool.cpp  


/***
 * 其中wifi接口定义路劲为 @ frameworks/opt/net/wifi/libwifi_hal/wifi_hal_common.cpp
 */ 

const int DriverTool::kFirmwareModeSta = WIFI_GET_FW_PATH_STA;
const int DriverTool::kFirmwareModeAp = WIFI_GET_FW_PATH_AP;
const int DriverTool::kFirmwareModeP2p = WIFI_GET_FW_PATH_P2P;

bool DriverTool::TakeOwnershipOfFirmwareReload() {
  if (!wifi_get_fw_path(kFirmwareModeSta) &&
      !wifi_get_fw_path(kFirmwareModeAp) &&
      !wifi_get_fw_path(kFirmwareModeP2p)) {
    return true;  // HAL doesn't think we need to load firmware for any mode.
  }

  if (chown(WIFI_DRIVER_FW_PATH_PARAM, AID_WIFI, AID_WIFI) != 0) {
    PLOG(ERROR) << "Error changing ownership of '" << WIFI_DRIVER_FW_PATH_PARAM << "' to wifi:wifi";
    return false;
  }

  return true;
}

bool DriverTool::LoadDriver() {
  return ::wifi_load_driver() == 0;
}

int wifi_load_driver() {
#ifdef WIFI_DRIVER_MODULE_PATH
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0) return -1;
#endif

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (wifi_change_driver_state(WIFI_DRIVER_STATE_ON) < 0) return -1;
#endif

  property_set(DRIVER_PROP_NAME, "ok"); //  static const char DRIVER_PROP_NAME[] = "wlan.driver.status";
  return 0;
}

bool DriverTool::UnloadDriver() {
  return ::wifi_unload_driver() == 0;
}

int wifi_unload_driver() {
  if (!is_wifi_driver_loaded()) {
    return 0;
  }
  usleep(200000); /* allow to finish interface down */
#ifdef WIFI_DRIVER_MODULE_PATH
  if (rmmod(DRIVER_MODULE_NAME) == 0) {
    int count = 20; /* wait at most 10 seconds for completion */
    while (count-- > 0) {
      if (!is_wifi_driver_loaded()) break;
      usleep(500000);
    }
    usleep(500000); /* allow card removal */
    if (count) {
      return 0;
    }
    return -1;
  } else
    return -1;
#else
#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    if (wifi_change_driver_state(WIFI_DRIVER_STATE_OFF) < 0) return -1;
  }
#endif
  property_set(DRIVER_PROP_NAME, "unloaded");//  static const char DRIVER_PROP_NAME[] = "wlan.driver.status";
  return 0;
#endif
}

bool DriverTool::IsDriverLoaded() {
  return ::wifi_unload_driver() != 0;
}

bool DriverTool::IsFirmwareModeChangeNeeded(int mode) {
  return (wifi_get_fw_path(mode) != nullptr);
}

const char *wifi_get_fw_path(int fw_type) {
  switch (fw_type) {
    case WIFI_GET_FW_PATH_STA:
      return WIFI_DRIVER_FW_PATH_STA;
    case WIFI_GET_FW_PATH_AP:
      return WIFI_DRIVER_FW_PATH_AP;
    case WIFI_GET_FW_PATH_P2P:
      return WIFI_DRIVER_FW_PATH_P2P;
  }
  return NULL;
}

bool DriverTool::ChangeFirmwareMode(int mode) {
  const char* fwpath = wifi_get_fw_path(mode);
  if (!fwpath) {
    return true;  // HAL doesn't think we need to load firmware for this mode.
  }
  if (wifi_change_fw_path(fwpath) != 0) {
    // Not all devices actually require firmware reloads, but
    // failure to change the firmware path when it is defined is an error.
    return false;
  }
  return true;
}

int wifi_change_fw_path(const char *fwpath) {
  int len;
  int fd;
  int ret = 0;

  if (!fwpath) return ret;
  fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_FW_PATH_PARAM, O_WRONLY));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open wlan fw path param";
    return -1;
  }
  len = strlen(fwpath) + 1;
  if (TEMP_FAILURE_RETRY(write(fd, fwpath, len)) != len) {
    PLOG(ERROR) << "Failed to write wlan fw path param";
    ret = -1;
  }
  close(fd);
  return ret;
}
