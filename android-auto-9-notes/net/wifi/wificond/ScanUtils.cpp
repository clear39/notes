//  @ /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/scanning/scan_utils.cpp
ScanUtils::ScanUtils(NetlinkManager* netlink_manager)
    : netlink_manager_(netlink_manager) {
  if (!netlink_manager_->IsStarted()) {
    netlink_manager_->Start();
  }
}
