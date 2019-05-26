//  @ /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/net/netlink_utils.h
struct InterfaceInfo {
  InterfaceInfo() = default; //
  InterfaceInfo(uint32_t index_,
                const std::string name_,
                const std::vector<uint8_t> mac_address_)
      : index(index_),
        name(name_),
        mac_address(mac_address_) {}
  // Index of this interface.
  uint32_t index;
  // Name of this interface.
  std::string name;
  // MAC address of this interface.
  std::vector<uint8_t> mac_address;
};



//  @   /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/server.cpp
/***
*   IWificond.aidl 服务端
*   InterfaceTool 网卡信息
*   SupplicantManager 用于启动和停止wpa_supplicant进程，init.svc.wpa_supplicant 属性相应被设置
*   HostapdManager 用于启动和停止hostapd进程
*/
Server::Server(unique_ptr<InterfaceTool> if_tool,  //   unique_ptr<InterfaceTool>(new InterfaceTool)  /
               unique_ptr<SupplicantManager> supplicant_manager,        // unique_ptr<SupplicantManager>(new SupplicantManager()
               unique_ptr<HostapdManager> hostapd_manager,              //  unique_ptr<HostapdManager>(new HostapdManager()),
               NetlinkUtils* netlink_utils,             //  &netlink_utils,
               ScanUtils* scan_utils)                  //    &scan_utils
    : if_tool_(std::move(if_tool)),
      supplicant_manager_(std::move(supplicant_manager)),
      hostapd_manager_(std::move(hostapd_manager)),
      netlink_utils_(netlink_utils),
      scan_utils_(scan_utils) {
}


