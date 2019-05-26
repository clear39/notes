//  @   /work/workcodes/aosp-p9.x-auto-alpha/system/connectivity/wificond/net/netlink_manager.cpp

class NetlinkManager {}


NetlinkManager::NetlinkManager(EventLoop* event_loop)
    : started_(false),
      event_loop_(event_loop),
      sequence_number_(0) {
}



bool NetlinkManager::Start() {
  if (started_) {
    LOG(DEBUG) << "NetlinkManager is already started";
    return true;
  }
  bool setup_rt = SetupSocket(&sync_netlink_fd_);
  if (!setup_rt) {
    LOG(ERROR) << "Failed to setup synchronous netlink socket";
    return false;
  }

  setup_rt = SetupSocket(&async_netlink_fd_);
  if (!setup_rt) {
    LOG(ERROR) << "Failed to setup asynchronous netlink socket";
    return false;
  }

  // Request family id for nl80211 messages.
  if (!DiscoverFamilyId()) {
    return false;
  }
  // Watch socket.
  if (!WatchSocket(&async_netlink_fd_)) {
    return false;
  }
  // Subscribe kernel NL80211 broadcast of regulatory changes.
  if (!SubscribeToEvents(NL80211_MULTICAST_GROUP_REG)) {
    return false;
  }
  // Subscribe kernel NL80211 broadcast of scanning events.
  if (!SubscribeToEvents(NL80211_MULTICAST_GROUP_SCAN)) {
    return false;
  }
  // Subscribe kernel NL80211 broadcast of MLME events.
  if (!SubscribeToEvents(NL80211_MULTICAST_GROUP_MLME)) {
    return false;
  }

  started_ = true;
  return true;
}

bool NetlinkManager::DiscoverFamilyId() {
  NL80211Packet get_family_request(GENL_ID_CTRL,CTRL_CMD_GETFAMILY,GetSequenceNumber(),getpid());
  NL80211Attr<string> family_name(CTRL_ATTR_FAMILY_NAME, NL80211_GENL_NAME);
  get_family_request.AddAttribute(family_name);
  unique_ptr<const NL80211Packet> response;
  if (!SendMessageAndGetSingleResponse(get_family_request, &response)) {
    LOG(ERROR) << "Failed to get NL80211 family info";
    return false;
  }
  OnNewFamily(std::move(response));
  //    ./net/kernel-header-latest/nl80211.h:44:#define NL80211_GENL_NAME "nl80211"
  if (message_types_.find(NL80211_GENL_NAME) == message_types_.end()) {
    LOG(ERROR) << "Failed to get NL80211 family id";
    return false;
  }
  return true;
}

bool NetlinkManager::SendMessageAndGetSingleResponse(
    const NL80211Packet& packet,
    unique_ptr<const NL80211Packet>* response) {
  unique_ptr<const NL80211Packet> response_or_error;
  if (!SendMessageAndGetSingleResponseOrError(packet, &response_or_error)) {
    return false;
  }
  if (response_or_error->GetMessageType() == NLMSG_ERROR) {
    // We use ERROR because we are not expecting to receive a ACK here.
    // In that case the caller should use |SendMessageAndGetAckOrError|.
    LOG(ERROR) << "Received error message: "
               << strerror(response_or_error->GetErrorCode());
    return false;
  }
  *response = std::move(response_or_error);
  return true;
}

bool NetlinkManager::SendMessageAndGetSingleResponseOrError(
    const NL80211Packet& packet,
    unique_ptr<const NL80211Packet>* response) {
  vector<unique_ptr<const NL80211Packet>> response_vec;
  if (!SendMessageAndGetResponses(packet, &response_vec)) {
    return false;
  }
  if (response_vec.size() != 1) {
    LOG(ERROR) << "Unexpected response size: " << response_vec.size();
    return false;
  }

  *response = std::move(response_vec[0]);
  return true;
}

bool NetlinkManager::WatchSocket(unique_fd* netlink_fd) {
  // Watch socket
  bool watch_fd_rt = event_loop_->WatchFileDescriptor(netlink_fd->get(),EventLoop::kModeInput,
      std::bind(&NetlinkManager::ReceivePacketAndRunHandler, this, _1));
  if (!watch_fd_rt) {
    LOG(ERROR) << "Failed to watch fd: " << netlink_fd->get();
    return false;
  }
  return true;
}

bool NetlinkManager::SubscribeToEvents(const string& group) {
  auto groups = message_types_[NL80211_GENL_NAME].groups;
  if (groups.find(group) == groups.end()) {
    LOG(ERROR) << "Failed to subscribe: group " << group << " doesn't exist";
    return false;
  }
  uint32_t group_id = groups[group];
  int err = setsockopt(async_netlink_fd_.get(),SOL_NETLINK,NETLINK_ADD_MEMBERSHIP,&group_id,sizeof(group_id));
  if (err < 0) {
    LOG(ERROR) << "Failed to setsockopt: " << strerror(errno);
    return false;
  }
  return true;
}








