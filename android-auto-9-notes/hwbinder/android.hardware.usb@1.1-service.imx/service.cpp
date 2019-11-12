//  @   vendor/nxp-opensource/imx/usb/service.cpp
int main() {
  android::sp<IUsb> service = new Usb();
  android::sp<IUsbGadget> service2 = new UsbGadget();

  configureRpcThreadpool(2, true /*callerWillJoin*/);
  
  status_t status = service->registerAsService();

  if (status != OK) {
    ALOGE("Cannot register USB HAL service");
    return 1;
  }

  status = service2->registerAsService();

  if (status != OK) {
    ALOGE("Cannot register USB Gadget HAL service");
    return 1;
  }

  ALOGI("USB HAL Ready.");

  joinRpcThreadpool();

  // Under noraml cases, execution will not reach this line.
  ALOGI("USB HAL failed to join thread pool.");

  return 1;
}
