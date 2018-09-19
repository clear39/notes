//	system/core/base/properties.cpp:146
bool WaitForProperty(const std::string& key, const std::string& expected_value,std::chrono::milliseconds relative_timeout) {
  auto start_time = std::chrono::steady_clock::now();
  const prop_info* pi = WaitForPropertyCreation(key, relative_timeout, start_time);
  if (pi == nullptr) return false;

  WaitForPropertyData data;
  data.expected_value = &expected_value;
  data.done = false;
  while (true) {
    timespec ts;
    // Check whether the property has the value we're looking for?
    __system_property_read_callback(pi, WaitForPropertyCallback, &data);
    if (data.done) return true;

    // It didn't, so wait for the property to change before checking again.
    UpdateTimeSpec(ts, relative_timeout, start_time);
    uint32_t unused;
    if (!__system_property_wait(pi, data.last_read_serial, &unused, &ts)) return false;
  }
}

// Waits for the system property `key` to be created.
// Times out after `relative_timeout`.
// Sets absolute_timeout which represents absolute time for the timeout.
// Returns nullptr on timeout.
static const prop_info* WaitForPropertyCreation(const std::string& key,const std::chrono::milliseconds& relative_timeout,const AbsTime& start_time) {
  // Find the property's prop_info*.
  const prop_info* pi;
  unsigned global_serial = 0;
  while ((pi = __system_property_find(key.c_str())) == nullptr) {//	bionic/libc/bionic/system_properties.cpp:1151
    // The property doesn't even exist yet.
    // Wait for a global change and then look again.
    timespec ts;
    UpdateTimeSpec(ts, relative_timeout, start_time);
    if (!__system_property_wait(nullptr, global_serial, &global_serial, &ts)) return nullptr;
  }
  return pi;
}

const prop_info* __system_property_find(const char* name) {
  if (!__system_property_area__) {
    return nullptr;
  }

  prop_area* pa = get_prop_area_for_name(name);
  if (!pa) {
    async_safe_format_log(ANDROID_LOG_ERROR, "libc", "Access denied finding property \"%s\"", name);
    return nullptr;
  }

  return pa->find(name);
}


static prop_area* get_prop_area_for_name(const char* name) {
  auto entry = list_find(prefixes, [name](prefix_node* l) {
    return l->prefix[0] == '*' || !strncmp(l->prefix, name, l->prefix_len);
  });
  if (!entry) {
    return nullptr;
  }

  auto cnode = entry->context;
  if (!cnode->pa()) {
    /*
     * We explicitly do not check no_access_ in this case because unlike the
     * case of foreach(), we want to generate an selinux audit for each
     * non-permitted property access in this function.
     */
    cnode->open(false, nullptr);
  }
  return cnode->pa();
}



static void UpdateTimeSpec(timespec& ts, std::chrono::milliseconds relative_timeout,const AbsTime& start_time) {
  auto now = std::chrono::steady_clock::now();
  auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
  if (time_elapsed >= relative_timeout) {
    ts = { 0, 0 };
  } else {
    auto remaining_timeout = relative_timeout - time_elapsed;
    DurationToTimeSpec(ts, remaining_timeout);
  }
}


// TODO: chrono_utils?
static void DurationToTimeSpec(timespec& ts, const std::chrono::milliseconds d) {
  auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d - s);
  ts.tv_sec = s.count();
  ts.tv_nsec = ns.count();
}
