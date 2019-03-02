// 这里不存在 
// @system/bt/vendor_libs/linux/interface/service.cc

int main(int /* argc */, char** /* argv */) {
  sp<IBluetoothHci> bluetooth = new BluetoothHci;
  configureRpcThreadpool(1, true);
  android::status_t status = bluetooth->registerAsService();
  if (status == android::OK)
    joinRpcThreadpool();
  else
    ALOGE("Could not register as a service!");
}



//  @system/bt/vendor_libs/linux/interface/bluetooth_hci.cc

