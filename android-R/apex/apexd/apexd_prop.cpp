

// 
void waitForBootStatus(Result<void> (&revert_fn)(const std::string&),void (&complete_fn)()) {
  while (true) {
    // Check for change in either crashing property or sys.boot_completed
    // Wait for updatable_crashing property change for most of the time
    // (arbitrary 30s), briefly check if boot has completed successfully,
    // if not continue waiting for updatable_crashing.
    // We use this strategy so that we can quickly detect if an updatable
    // process is crashing.
    if (WaitForProperty("sys.init.updatable_crashing", "1", std::chrono::seconds(30))) {
      auto name = GetProperty("sys.init.updatable_crashing_process_name", "");
      LOG(ERROR) << "Native process '" << (name.empty() ? "[unknown]" : name)  << "' is crashing. Attempting a revert";
      auto result = revert_fn(name);
      if (!result.ok()) {
        LOG(ERROR) << "Revert failed : " << result.error();
      } else {
        LOG(INFO) << "Successfuly reverted update. Rebooting device";
        Reboot();
      }
      return;
    }
    if (GetProperty("sys.boot_completed", "") == "1") {
      // Boot completed we can return
      complete_fn();
      return;
    }
  }
}