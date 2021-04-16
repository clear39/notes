

void onStart(CheckpointInterface* checkpoint_service) {
  LOG(INFO) << "Marking APEXd as starting";
  // 
  if (!android::base::SetProperty(kApexStatusSysprop, kApexStatusStarting)) {
    PLOG(ERROR) << "Failed to set " << kApexStatusSysprop << " to " << kApexStatusStarting;
  }

  if (checkpoint_service != nullptr) {
    gVoldService = checkpoint_service;
    Result<bool> supports_fs_checkpoints = gVoldService->SupportsFsCheckpoints();
    if (supports_fs_checkpoints.ok()) {
      gSupportsFsCheckpoints = *supports_fs_checkpoints;
    } else {
      LOG(ERROR) << "Failed to check if filesystem checkpoints are supported: " << supports_fs_checkpoints.error();
    }
    if (gSupportsFsCheckpoints) {
      Result<bool> needs_checkpoint = gVoldService->NeedsCheckpoint();
      if (needs_checkpoint.ok()) {
        gInFsCheckpointMode = *needs_checkpoint;
      } else {
        LOG(ERROR) << "Failed to check if we're in filesystem checkpoint mode: " << needs_checkpoint.error();
      }
    }
  }

  // Ask whether we should roll back any staged sessions; this can happen if
  // we've exceeded the retry count on a device that supports filesystem
  // checkpointing.
  if (gSupportsFsCheckpoints) {
    Result<bool> needs_rollback = gVoldService->NeedsRollback();
    if (!needs_rollback.ok()) {
      LOG(ERROR) << "Failed to check if we need a rollback: " << needs_rollback.error();
    } else if (*needs_rollback) {
      LOG(INFO) << "Exceeded number of session retries (" << kNumRetriesWhenCheckpointingEnabled << "). Starting a rollback";
      Result<void> status = rollbackStagedSessionIfAny();
      if (!status.ok()) {
        LOG(ERROR) << "Failed to roll back (as requested by fs checkpointing) : " << status.error();
      }
    }
  }

  Result<void> status = collectPreinstalledData(kApexPackageBuiltinDirs);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to collect APEX keys : " << status.error();
    return;
  }

  gMountedApexes.PopulateFromMounts();

  // Activate APEXes from /data/apex. If one in the directory is newer than the
  // system one, the new one will eclipse the old one.
  scanStagedSessionsDirAndStage();
  status = resumeRollbackIfNeeded();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to resume rollback : " << status.error();
  }

  std::vector<ApexFile> data_apex;
  if (auto scan = ScanApexFiles(kActiveApexPackagesDataDir); !scan.ok()) {
    LOG(ERROR) << "Failed to scan packages from " << kActiveApexPackagesDataDir << " : " << scan.error();
    if (auto rollback = rollbackActiveSessionAndReboot(""); !rollback.ok()) {
      LOG(ERROR) << "Failed to rollback : " << rollback.error();
    }
  } else {
    auto filter_fn = [](const ApexFile& apex) {
      if (!ShouldActivateApexOnData(apex)) {
        LOG(WARNING) << "Skipping " << apex.GetPath();
        return false;
      }
      return true;
    };
    std::copy_if(std::make_move_iterator(scan->begin()),
                 std::make_move_iterator(scan->end()),
                 std::back_inserter(data_apex), filter_fn);
  }

  if (auto ret = ActivateApexPackages(data_apex); !ret.ok()) {
    LOG(ERROR) << "Failed to activate packages from " << kActiveApexPackagesDataDir << " : " << ret.error();
    if (auto rollback = rollbackActiveSessionAndReboot(""); !rollback.ok()) {
      LOG(ERROR) << "Failed to rollback : " << rollback.error();
    }
  }

  // Now also scan and activate APEXes from pre-installed directories.
  for (const auto& dir : kApexPackageBuiltinDirs) {
    auto scan = ScanApexFiles(dir.c_str());
    if (!scan.ok()) {
      LOG(ERROR) << "Failed to scan APEX packages from " << dir << " : " << scan.error();
      if (auto rollback = rollbackActiveSessionAndReboot(""); !rollback.ok()) {
        LOG(ERROR) << "Failed to rollback : " << rollback.error();
      }
    }
    if (auto activate = ActivateApexPackages(*scan); !activate.ok()) {
      // This should never happen. Like **really** never.
      // TODO: should we kill apexd in this case?
      LOG(ERROR) << "Failed to activate packages from " << dir << " : " << activate.error();
    }
  }

  // Now that APEXes are mounted, snapshot or restore DE_sys data.
  snapshotOrRestoreDeSysData();

  if (android::base::GetBoolProperty("ro.debuggable", false)) {
    status = monitorBuiltinDirs();
    if (!status.ok()) {
      LOG(ERROR) << "cannot monitor built-in dirs: " << status.error();
    }
  }
}



void onAllPackagesReady() {
  // Set a system property to let other components to know that APEXs are
  // correctly mounted and ready to be used. Before using any file from APEXs,
  // they can query this system property to ensure that they are okay to
  // access. Or they may have a on-property trigger to delay a task until
  // APEXs become ready.
  LOG(INFO) << "Marking APEXd as ready";
  if (!android::base::SetProperty(kApexStatusSysprop, kApexStatusReady)) {
    PLOG(ERROR) << "Failed to set " << kApexStatusSysprop << " to " << kApexStatusReady;
  }
}