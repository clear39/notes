//  @   vendor/nxp-opensource/imx/usb/UsbGadget.cpp
UsbGadget::UsbGadget()
    : mMonitorCreated(false), mCurrentUsbFunctionsApplied(false) {
  mCurrentUsbFunctions = static_cast<uint64_t>(GadgetFunction::NONE);
  /**
   * #define GADGET_PATH "/config/usb_gadget/g1/"
   * #define OS_DESC_PATH GADGET_PATH "os_desc/b.1"      # "/config/usb_gadget/g1/os_desc/b.1" 
  */
  if (access(OS_DESC_PATH, R_OK) != 0) ALOGE("configfs setup not done yet");
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * 
 * 
*/
Return<void> UsbGadget::setCurrentUsbFunctions(uint64_t functions, const sp<V1_0::IUsbGadgetCallback> &callback,uint64_t timeout) {
  std::unique_lock<std::mutex> lk(mLockSetCurrentFunction);

  mCurrentUsbFunctions = functions;
  mCurrentUsbFunctionsApplied = false;

  // Unlink the gadget and stop the monitor if running.
  V1_0::Status status = tearDownGadget();
  if (status != Status::SUCCESS) {
    goto error;
  }

  if ((functions & GadgetFunction::RNDIS) == 0) {
    /**
     * #define GADGET_PATH "/config/usb_gadget/g1/"
     * #define FUNCTIONS_PATH GADGET_PATH "functions/"
     * #define RNDIS_PATH FUNCTIONS_PATH "rndis.gs4"
     * 
     * "/config/usb_gadget/g1/functions/rndis.gs4"
     * 
    */
    if (access(RNDIS_PATH,F_OK) == 0) {
       if (rmdir(RNDIS_PATH)) ALOGE("Error remove %s",RNDIS_PATH);
    }
  } else if ((functions & GadgetFunction::RNDIS)) {
    if (mkdir(RNDIS_PATH,644)) goto error;
  }

  // Leave the gadget pulled down to give time for the host to sense disconnect.
  usleep(DISCONNECT_WAIT_US);

  if (functions == static_cast<uint64_t>(GadgetFunction::NONE)) {
    if (callback == NULL) return Void();
    Return<void> ret = callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS);
    if (!ret.isOk())
      ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.description().c_str());
    return Void();
  }

  status = validateAndSetVidPid(functions);

  if (status != Status::SUCCESS) {
    goto error;
  }

  status = setupFunctions(functions, callback, timeout);
  if (status != Status::SUCCESS) {
    goto error;
  }

  ALOGI("Usb Gadget setcurrent functions called successfully");
  return Void();

error:
  ALOGI("Usb Gadget setcurrent functions failed");
  if (callback == NULL) return Void();
  Return<void> ret = callback->setCurrentUsbFunctionsCb(functions, status);
  if (!ret.isOk())
    ALOGE("Error while calling setCurrentUsbFunctionsCb %s",ret.description().c_str());
  return Void();
}


V1_0::Status UsbGadget::tearDownGadget() {
  ALOGI("setCurrentUsbFunctions None");

  /**
   * /config/usb_gadget/g1/UDC
  */
  if (!WriteStringToFile("none", PULLUP_PATH))
    ALOGI("Gadget cannot be pulled down");

  SetProperty(CTL_STOP, ADBD);  //  SetProperty("ctl.stop", "adbd");
  SetProperty(USB_FFS, "0");  //  SetProperty("sys.usb.ffs.ready", "0");

  /**
   * "/config/usb_gadget/g1/bDeviceClass"
  */
  if (!WriteStringToFile("0", DEVICE_CLASS_PATH)) return Status::ERROR;
  /**
   * "/config/usb_gadget/g1/bDeviceSubClass"
  */
  if (!WriteStringToFile("0", DEVICE_SUB_CLASS_PATH)) return Status::ERROR;
  /**
   * "/config/usb_gadget/g1/bDeviceProtocol"
  */
  if (!WriteStringToFile("0", DEVICE_PROTOCOL_PATH)) return Status::ERROR;
  /**
   * "/config/usb_gadget/g1/os_desc/use"
  */
  if (!WriteStringToFile("0", DESC_USE_PATH)) return Status::ERROR;

  /**
   * "/config/usb_gadget/g1/configs/b.1/"
   * 
-rw-r--r-- 1 root root 4096 2019-11-13 13:43 MaxPower
-rw-r--r-- 1 root root 4096 2019-11-13 13:43 bmAttributes
lrwxrwxrwx 1 root root    0 2019-11-13 11:49 function0 -> ../../../../usb_gadget/g1/functions/ffs.adb
drwxr-xr-x 3 root root    0 1970-01-01 08:00 strings
  */
  if (unlinkFunctions(CONFIG_PATH)) return Status::ERROR;

  if (mMonitorCreated) {
    uint64_t flag = 100;
    // Stop the monitor thread by writing into signal fd.
    write(mEventFd, &flag, sizeof(flag));
    mMonitor->join();
    mMonitorCreated = false;
    ALOGI("mMonitor destroyed");
  } else {
    ALOGI("mMonitor not running");
  }

  mInotifyFd.reset(-1);
  mEventFd.reset(-1);
  mEpollFd.reset(-1);
  mEndpointList.clear();
  return Status::SUCCESS;
}


static int unlinkFunctions(const char *path) {
  DIR *config = opendir(path);
  struct dirent *function;
  char filepath[MAX_FILE_PATH_LENGTH];
  int ret = 0;

  if (config == NULL) return -1;

  // d_type does not seems to be supported in /config
  // so filtering by name.
  while (((function = readdir(config)) != NULL)) {
    /**
     * #define FUNCTION_NAME "function"
     * 
     * strstr 查找第一个 function 的字符串，
    */
    if ((strstr(function->d_name, FUNCTION_NAME) == NULL)) continue;
    // build the path for each file in the folder.
    /**
     * "/config/usb_gadget/g1/configs/b.1/function0"
    */
    sprintf(filepath, "%s/%s", path, function->d_name);
    ret = remove(filepath);
    if (ret) {
      ALOGE("Unable  remove file %s errno:%d", filepath, errno);
      break;
    }
  }

  closedir(config);
  return ret;
}

static V1_0::Status validateAndSetVidPid(uint64_t functions) {
  V1_0::Status ret = Status::SUCCESS;

  switch (functions) {
    case static_cast<uint64_t>(GadgetFunction::MTP):
        ret = setVidPid("0x18d1", "0x4ee1");
      break;
    case GadgetFunction::ADB | GadgetFunction::MTP:
        ret = setVidPid("0x18d1", "0x4ee2");
      break;
    case static_cast<uint64_t>(GadgetFunction::RNDIS):
        ret = setVidPid("0x18d1", "0x4ee3");
      break;
    case GadgetFunction::ADB | GadgetFunction::RNDIS:
        ret = setVidPid("0x18d1", "0x4ee4");
      break;
    case static_cast<uint64_t>(GadgetFunction::PTP):
      ret = setVidPid("0x18d1", "0x4ee5");
      break;
    case GadgetFunction::ADB | GadgetFunction::PTP:
      ret = setVidPid("0x18d1", "0x4ee6");
      break;
    case static_cast<uint64_t>(GadgetFunction::ADB):
        ret = setVidPid("0x18d1", "0x4ee7");
      break;
    case static_cast<uint64_t>(GadgetFunction::MIDI):
      ret = setVidPid("0x18d1", "0x4ee8");
      break;
    case GadgetFunction::ADB | GadgetFunction::MIDI:
      ret = setVidPid("0x18d1", "0x4ee9");
      break;
    case static_cast<uint64_t>(GadgetFunction::ACCESSORY):
      ret = setVidPid("0x18d1", "0x2d00");
      break;
    case GadgetFunction::ADB | GadgetFunction::ACCESSORY:
      ret = setVidPid("0x18d1", "0x2d01");
      break;
    case static_cast<uint64_t>(GadgetFunction::AUDIO_SOURCE):
      ret = setVidPid("0x18d1", "0x2d02");
      break;
    case GadgetFunction::ADB | GadgetFunction::AUDIO_SOURCE:
      ret = setVidPid("0x18d1", "0x2d03");
      break;
    case GadgetFunction::ACCESSORY | GadgetFunction::AUDIO_SOURCE:
      ret = setVidPid("0x18d1", "0x2d04");
      break;
    case GadgetFunction::ADB | GadgetFunction::ACCESSORY | GadgetFunction::AUDIO_SOURCE:
      ret = setVidPid("0x18d1", "0x2d05");
      break;
    default:
      ALOGE("Combination not supported");
      ret = Status::CONFIGURATION_NOT_SUPPORTED;
  }
  return ret;
}

static V1_0::Status setVidPid(const char *vid, const char *pid) {
  if (!WriteStringToFile(vid, VENDOR_ID_PATH)) return Status::ERROR;

  if (!WriteStringToFile(pid, PRODUCT_ID_PATH)) return Status::ERROR;

  return Status::SUCCESS;
}



V1_0::Status UsbGadget::setupFunctions(uint64_t functions, const sp<V1_0::IUsbGadgetCallback> &callback,uint64_t timeout) {
  std::unique_lock<std::mutex> lk(mLock);

  unique_fd inotifyFd(inotify_init());
  if (inotifyFd < 0) {
    ALOGE("inotify init failed");
    return Status::ERROR;
  }

  bool ffsEnabled = false;
  int i = 0;
  /**
   * #define PERSISTENT_BOOT_MODE "ro.bootmode"
  */
  std::string bootMode = GetProperty(PERSISTENT_BOOT_MODE, "");
   /**
     * GetProperty("sys.usb.ffs.ready", "");
    */
  if ((functions & GadgetFunction::ADB) != 0 && GetProperty(USB_FFS, "") != "1" ) {
    /**
     * SetProperty("ctl.start", "adbd");
    */
    SetProperty(CTL_START, ADBD);
  }

  for (int i = 0; i < 20; i++) {
    string value = GetProperty(USB_FFS, "");
    if ("1" == value)
      break;
    else
      usleep(20000);//20ms
  }

  if (((functions & GadgetFunction::MTP) != 0)) {
    ffsEnabled = true;
    ALOGI("setCurrentUsbFunctions mtp");
    if (!WriteStringToFile("1", DESC_USE_PATH)) return Status::ERROR;

    if (inotify_add_watch(inotifyFd, "/dev/usb-ffs/mtp/", IN_ALL_EVENTS) == -1)
      return Status::ERROR;

    if (linkFunction("ffs.mtp", i++)) return Status::ERROR;

    // Add endpoints to be monitored.
    mEndpointList.push_back("/dev/usb-ffs/mtp/ep1");
    mEndpointList.push_back("/dev/usb-ffs/mtp/ep2");
    mEndpointList.push_back("/dev/usb-ffs/mtp/ep3");
  } else if (((functions & GadgetFunction::PTP) != 0)) {
    ffsEnabled = true;
    ALOGI("setCurrentUsbFunctions ptp");
    if (!WriteStringToFile("1", DESC_USE_PATH)) return Status::ERROR;

    if (inotify_add_watch(inotifyFd, "/dev/usb-ffs/ptp/", IN_ALL_EVENTS) == -1)
      return Status::ERROR;


    if (linkFunction("ffs.ptp", i++)) return Status::ERROR;

    // Add endpoints to be monitored.
    mEndpointList.push_back("/dev/usb-ffs/ptp/ep1");
    mEndpointList.push_back("/dev/usb-ffs/ptp/ep2");
    mEndpointList.push_back("/dev/usb-ffs/ptp/ep3");
  }

  if ((functions & GadgetFunction::MIDI) != 0) {
    ALOGI("setCurrentUsbFunctions MIDI");
    if (linkFunction("midi.gs5", i++)) return Status::ERROR;
  }

  if ((functions & GadgetFunction::ACCESSORY) != 0) {
    ALOGI("setCurrentUsbFunctions Accessory");
    if (linkFunction("accessory.gs2", i++)) return Status::ERROR;
  }

  if ((functions & GadgetFunction::AUDIO_SOURCE) != 0) {
    ALOGI("setCurrentUsbFunctions Audio Source");
    if (linkFunction("audio_source.gs3", i++)) return Status::ERROR;
  }

  if ((functions & GadgetFunction::RNDIS) != 0) {
    ALOGI("setCurrentUsbFunctions rndis");
    if (linkFunction("rndis.gs4", i++)) return Status::ERROR;
  }

  if ((functions & GadgetFunction::ADB) != 0) {
    ffsEnabled = true;
    ALOGI("setCurrentUsbFunctions Adb");
    if (inotify_add_watch(inotifyFd, "/dev/usb-ffs/adb/", IN_ALL_EVENTS) == -1)
      return Status::ERROR;

    if (linkFunction("ffs.adb", i++)) return Status::ERROR;
    mEndpointList.push_back("/dev/usb-ffs/adb/ep1");
    mEndpointList.push_back("/dev/usb-ffs/adb/ep2");
    ALOGI("Service started");
  }

  // Pull up the gadget right away when there are no ffs functions.
  if (!ffsEnabled) {
    /**
     * #define USB_CONTROLLER "vendor.usb.config"
     * #define GADGET_NAME GetProperty(USB_CONTROLLER, "")   // GetProperty("vendor.usb.config", "")
     * 
     * /config/usb_gadget/g1/UDC
    */
    if (!WriteStringToFile(GADGET_NAME, PULLUP_PATH)) return Status::ERROR;
    mCurrentUsbFunctionsApplied = true;
    if (callback)
      callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS);
    return Status::SUCCESS;
  }

  unique_fd eventFd(eventfd(0, 0));
  if (eventFd == -1) {
    ALOGE("mEventFd failed to create %d", errno);
    return Status::ERROR;
  }

  unique_fd epollFd(epoll_create(2));
  if (epollFd == -1) {
    ALOGE("mEpollFd failed to create %d", errno);
    return Status::ERROR;
  }

  if (addEpollFd(epollFd, inotifyFd) == -1) return Status::ERROR;

  if (addEpollFd(epollFd, eventFd) == -1) return Status::ERROR;

  mEpollFd = move(epollFd);
  mInotifyFd = move(inotifyFd);
  mEventFd = move(eventFd);
  gadgetPullup = false;

  // Monitors the ffs paths to pull up the gadget when descriptors are written.
  // Also takes of the pulling up the gadget again if the userspace process
  // dies and restarts.
  mMonitor = unique_ptr<thread>(new thread(monitorFfs, this));
  mMonitorCreated = true;
  if (DEBUG) ALOGI("Mainthread in Cv");

  if (callback) {
    if (mCv.wait_for(lk, timeout * 1ms, [] { return gadgetPullup; })) {
      ALOGI("monitorFfs signalled true");
    } else {
      ALOGI("monitorFfs signalled error");
      // continue monitoring as the descriptors might be written at a later
      // point.
    }
    Return<void> ret = callback->setCurrentUsbFunctionsCb(
        functions, gadgetPullup ? Status::SUCCESS : Status::ERROR);
    if (!ret.isOk())
      ALOGE("setCurrentUsbFunctionsCb error %s", ret.description().c_str());
  }

  return Status::SUCCESS;
}

static int linkFunction(const char *function, int index) {
  char functionPath[MAX_FILE_PATH_LENGTH];
  char link[MAX_FILE_PATH_LENGTH];

  /**
   * #define GADGET_PATH "/config/usb_gadget/g1/"
   * #define FUNCTIONS_PATH GADGET_PATH "functions/"  # 
   * 
  */
  sprintf(functionPath, "%s%s", FUNCTIONS_PATH, function); // functionPath = "/config/usb_gadget/g1/functions/"
  /**
   * #define FUNCTION_PATH CONFIG_PATH FUNCTION_NAME
   * #define CONFIG_PATH GADGET_PATH "configs/b.1/"
   * #define FUNCTION_NAME "function"
   * link = "/config/usb_gadget/g1/configs/b.1/function" + index
  */
  sprintf(link, "%s%d", FUNCTION_PATH, index);  //  link
  if (symlink(functionPath, link)) {
    ALOGE("Cannot create symlink %s -> %s errno:%d", link, functionPath, errno);
    return -1;
  }
  return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> UsbGadget::getCurrentUsbFunctions(const sp<V1_0::IUsbGadgetCallback> &callback) {
  Return<void> ret = callback->getCurrentUsbFunctionsCb(mCurrentUsbFunctions, mCurrentUsbFunctionsApplied? Status::FUNCTIONS_APPLIED: Status::FUNCTIONS_NOT_APPLIED);
  if (!ret.isOk())
    ALOGE("Call to getCurrentUsbFunctionsCb failed %s",ret.description().c_str());

  return Void();
}