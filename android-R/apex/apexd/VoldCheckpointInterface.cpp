
class CheckpointInterface {
 public:
  virtual ~CheckpointInterface() {}

  virtual android::base::Result<bool> SupportsFsCheckpoints() = 0;

  virtual android::base::Result<bool> NeedsCheckpoint() = 0;
  virtual android::base::Result<bool> NeedsRollback() = 0;
  virtual android::base::Result<void> StartCheckpoint(int32_t numRetries) = 0;

  virtual android::base::Result<void> AbortChanges(const std::string& msg, bool retry) = 0;
};

class VoldCheckpointInterface : public CheckpointInterface {

};


Result<VoldCheckpointInterface> VoldCheckpointInterface::Create() {
  // 获取vold服务
  auto voldService = defaultServiceManager()->getService(android::String16("vold"));
  if (voldService != nullptr) {
    return VoldCheckpointInterface(android::interface_cast<android::os::IVold>(voldService));
  }
  return Errorf("Failed to retrieve vold service.");
}


VoldCheckpointInterface::VoldCheckpointInterface(sp<IVold>&& vold_service) {
  vold_service_ = vold_service;
  supports_fs_checkpoints_ = false;
  android::binder::Status status = vold_service_->supportsCheckpoint(&supports_fs_checkpoints_);
  if (!status.isOk()) {
    LOG(ERROR) << "Failed to check if filesystem checkpoints are supported: " << status.toString8().c_str();
  }
}
