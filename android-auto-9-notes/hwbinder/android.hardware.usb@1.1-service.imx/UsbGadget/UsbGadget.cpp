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
  SetProperty(USB_FFS, "0");  //  SetProperty("ctl.stop", "adbd");

  if (!WriteStringToFile("0", DEVICE_CLASS_PATH)) return Status::ERROR;

  if (!WriteStringToFile("0", DEVICE_SUB_CLASS_PATH)) return Status::ERROR;

  if (!WriteStringToFile("0", DEVICE_PROTOCOL_PATH)) return Status::ERROR;

  if (!WriteStringToFile("0", DESC_USE_PATH)) return Status::ERROR;

  /**
   * "/config/usb_gadget/g1/configs/b.1/"
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



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Return<void> UsbGadget::getCurrentUsbFunctions(const sp<V1_0::IUsbGadgetCallback> &callback) {
  Return<void> ret = callback->getCurrentUsbFunctionsCb(mCurrentUsbFunctions, mCurrentUsbFunctionsApplied? Status::FUNCTIONS_APPLIED: Status::FUNCTIONS_NOT_APPLIED);
  if (!ret.isOk())
    ALOGE("Call to getCurrentUsbFunctionsCb failed %s",ret.description().c_str());

  return Void();
}