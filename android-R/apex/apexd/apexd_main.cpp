int main(int /*argc*/, char** argv) {
  android::base::InitLogging(argv, &android::base::KernelLogger);
  // TODO: add a -v flag or an external setting to change LogSeverity.
  android::base::SetMinimumLogSeverity(android::base::VERBOSE);

  const bool has_subcommand = argv[1] != nullptr;
  if (!android::sysprop::ApexProperties::updatable().value_or(false)) {
    LOG(INFO) << "This device does not support updatable APEX. Exiting";
    if (!has_subcommand) {
      // mark apexd as ready so that init can proceed
      android::apex::onAllPackagesReady();
      android::base::SetProperty("ctl.stop", "apexd");
    }
    return 0;
  }

  if (has_subcommand) {
    return HandleSubcommand(argv);
  }

  android::base::Result<android::apex::VoldCheckpointInterface> vold_service_st = android::apex::VoldCheckpointInterface::Create();
  android::apex::VoldCheckpointInterface* vold_service = nullptr;
  if (!vold_service_st.ok()) {
    LOG(ERROR) << "Could not retrieve vold service: " << vold_service_st.error();
  } else {
    vold_service = &*vold_service_st;
  }

  // @  system/apex/apexd/apexd.cpp
  android::apex::onStart(vold_service);

  // "apexservice"; 在 CreateAndRegisterService 注册服务 ApexService
  android::apex::binder::CreateAndRegisterService();
  // 
  android::apex::binder::StartThreadPool();

  // Notify other components (e.g. init) that all APEXs are correctly mounted
  // and are ready to be used. Note that it's important that the binder service
  // is registered at this point, since other system services might depend on
  // it.
  // @ system/apex/apexd/apexd.cpp
  android::apex::onAllPackagesReady();

  // @ system/apex/apexd/apexd_prop.cpp
  android::apex::waitForBootStatus(
      android::apex::rollbackActiveSessionAndReboot,
      android::apex::bootCompletedCleanup);

  android::apex::binder::JoinThreadPool();
  return 1;
}



int HandleSubcommand(char** argv) {
  if (strcmp("--pre-install", argv[1]) == 0) {
    LOG(INFO) << "Preinstall subcommand detected";
    return android::apex::RunPreInstall(argv);
  }

  if (strcmp("--post-install", argv[1]) == 0) {
    LOG(INFO) << "Postinstall subcommand detected";
    return android::apex::RunPostInstall(argv);
  }

  if (strcmp("--bootstrap", argv[1]) == 0) {
    LOG(INFO) << "Bootstrap subcommand detected";
    return android::apex::onBootstrap();
  }

  if (strcmp("--unmount-all", argv[1]) == 0) {
    LOG(INFO) << "Unmount all subcommand detected";
    return android::apex::unmountAll();
  }

  if (strcmp("--snapshotde", argv[1]) == 0) {
    LOG(INFO) << "Snapshot DE subcommand detected";
    return android::apex::snapshotOrRestoreDeUserData();
  }

  LOG(ERROR) << "Unknown subcommand: " << argv[1];
  return 1;
}
