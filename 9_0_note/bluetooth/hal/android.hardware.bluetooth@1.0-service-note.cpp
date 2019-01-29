//  @hardware/interfaces/bluetooth/1.0/default/service.cpp
int main() {
    return defaultPassthroughServiceImplementation<IBluetoothHci>(kMaxThreads);
}

/**
 * defaultPassthroughServiceImplementation 就是IBluetoothHci当so加载，
 * 并且入口为HIDL_FETCH_IBluetoothHci @hardware/interfaces/bluetooth/1.0/default/bluetooth_hci.cc
 */ 
IBluetoothHci* HIDL_FETCH_IBluetoothHci(const char* /* name */) {
  return new BluetoothHci();
}


//  @hardware/interfaces/bluetooth/1.0/default/bluetooth_hci.cc




Return<void> BluetoothHci::close() {
  ALOGI("BluetoothHci::close()");
  unlink_cb_(death_recipient_);
  VendorInterface::Shutdown();
  return Void();
}



//  @hardware/interfaces/bluetooth/1.0/default/vendor_interface.cc
void VendorInterface::Shutdown() {
  LOG_ALWAYS_FATAL_IF(!g_vendor_interface, "%s: No Vendor interface!",__func__);
  g_vendor_interface->Close();
  delete g_vendor_interface;
  g_vendor_interface = nullptr;
}