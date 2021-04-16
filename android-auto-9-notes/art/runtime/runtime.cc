


bool Runtime::Create(const RuntimeOptions& raw_options, bool ignore_unrecognized) {
  RuntimeArgumentMap runtime_options;
  return ParseOptions(raw_options, ignore_unrecognized, &runtime_options) && 
            Create(std::move(runtime_options));
}



bool Runtime::ParseOptions(const RuntimeOptions& raw_options,bool ignore_unrecognized,RuntimeArgumentMap* runtime_options) {
  Locks::Init();
  InitLogging(/* argv */ nullptr, Abort);  // Calls Locks::Init() as a side effect.
  bool parsed = ParsedOptions::Parse(raw_options, ignore_unrecognized, runtime_options);                                                                                                                       
  if (!parsed) {
    LOG(ERROR) << "Failed to parse options";
    return false;
  }
  return true;
}


bool Runtime::Create(RuntimeArgumentMap&& runtime_options) {
  // TODO: acquire a static mutex on Runtime to avoid racing.
  if (Runtime::instance_ != nullptr) {
    return false;
  }
  /**
   * 构建Runtime
  */
  instance_ = new Runtime;

  Locks::SetClientCallback(IsSafeToCallAbort);
  if (!instance_->Init(std::move(runtime_options))) {
    // TODO: Currently deleting the instance will abort the runtime on destruction. Now This will
    // leak memory, instead. Fix the destructor. b/19100793.
    // delete instance_;
    instance_ = nullptr;
    return false;
  }
  return true;
}