//	@system/connectivity/wificond/wificond.rc
service wificond /system/bin/wificond
    class main
    user wifi
    group wifi net_raw net_admin
    capabilities NET_RAW NET_ADMIN




//	@system/connectivity/wificond/main.cpp
int main(int argc, char** argv) {
  android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));
  LOG(INFO) << "wificond is starting up...";

  unique_ptr<android::wificond::LooperBackedEventLoop> event_dispatcher(new android::wificond::LooperBackedEventLoop());

  //设置SIGINT和SIGTERM
  ScopedSignalHandler scoped_signal_handler(event_dispatcher.get());

  int binder_fd = SetupBinderOrCrash();
  CHECK(event_dispatcher->WatchFileDescriptor(binder_fd,android::wificond::EventLoop::kModeInput,&OnBinderReadReady)) << "Failed to watch binder FD";

  int hw_binder_fd = SetupHwBinderOrCrash();
  CHECK(event_dispatcher->WatchFileDescriptor(hw_binder_fd, android::wificond::EventLoop::kModeInput,&OnHwBinderReadReady)) << "Failed to watch Hw Binder FD";

  android::wificond::NetlinkManager netlink_manager(event_dispatcher.get());
  if (!netlink_manager.Start()) {
    LOG(ERROR) << "Failed to start netlink manager";
  }
  android::wificond::NetlinkUtils netlink_utils(&netlink_manager);
  android::wificond::ScanUtils scan_utils(&netlink_manager);

  unique_ptr<android::wificond::Server> server(new android::wificond::Server(unique_ptr<InterfaceTool>(new InterfaceTool),unique_ptr<SupplicantManager>(new SupplicantManager()),unique_ptr<HostapdManager>(new HostapdManager()),&netlink_utils,&scan_utils));
  server->CleanUpSystemState();
  RegisterServiceOrCrash(server.get());

  event_dispatcher->Poll();
  LOG(INFO) << "wificond is about to exit";
  return 0;
}

class ScopedSignalHandler final {
 public:
  ScopedSignalHandler(android::wificond::LooperBackedEventLoop* event_loop) {
    if (s_event_loop_ != nullptr) {
      LOG(FATAL) << "Only instantiate one signal handler per process!";
    }
    s_event_loop_ = event_loop;
    std::signal(SIGINT, &ScopedSignalHandler::LeaveLoop);
    std::signal(SIGTERM, &ScopedSignalHandler::LeaveLoop);
  }

  ~ScopedSignalHandler() {
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    s_event_loop_ = nullptr;
  }

 private:
  static android::wificond::LooperBackedEventLoop* s_event_loop_;
  static void LeaveLoop(int signal) {
    if (s_event_loop_ != nullptr) {
      s_event_loop_->TriggerExit();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ScopedSignalHandler);
};

android::wificond::LooperBackedEventLoop*
    ScopedSignalHandler::s_event_loop_ = nullptr;


// Setup our interface to the Binder driver or die trying.
int SetupBinderOrCrash() {
  int binder_fd = -1;
  android::ProcessState::self()->setThreadPoolMaxThreadCount(0);
  android::IPCThreadState::self()->disableBackgroundScheduling(true);
  int err = android::IPCThreadState::self()->setupPolling(&binder_fd);
  CHECK_EQ(err, 0) << "Error setting up binder polling: " << strerror(-err);
  CHECK_GE(binder_fd, 0) << "Invalid binder FD: " << binder_fd;
  return binder_fd;
}


int IPCThreadState::setupPolling(int* fd)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }

    mOut.writeInt32(BC_ENTER_LOOPER);
    *fd = mProcess->mDriverFD;
    return 0;
}