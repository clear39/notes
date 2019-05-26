//  @ /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/net/netlink_utils.cpp
NetlinkUtils::NetlinkUtils(NetlinkManager* netlink_manager)
    : netlink_manager_(netlink_manager) {
  if (!netlink_manager_->IsStarted()) {
    netlink_manager_->Start();
  }
  uint32_t protocol_features = 0;
  supports_split_wiphy_dump_ = GetProtocolFeatures(&protocol_features) &&
      (protocol_features & NL80211_PROTOCOL_FEATURE_SPLIT_WIPHY_DUMP);
}