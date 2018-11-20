//  @art/runtime/java_vm_ext.cc



// Checking "globals" and "weak_globals" usually requires locks, but we
// don't need the locks to check for validity when constructing the
// object. Use NO_THREAD_SAFETY_ANALYSIS for this.
std::unique_ptr<JavaVMExt> JavaVMExt::Create(Runtime* runtime,const RuntimeArgumentMap& runtime_options,std::string* error_msg) NO_THREAD_SAFETY_ANALYSIS {
  std::unique_ptr<JavaVMExt> java_vm(new JavaVMExt(runtime, runtime_options, error_msg));
  if (java_vm && java_vm->globals_.IsValid() && java_vm->weak_globals_.IsValid()) {
    return java_vm;
  }
  return nullptr;
}


const JNIInvokeInterface gJniInvokeInterface = {
  nullptr,  // reserved0
  nullptr,  // reserved1
  nullptr,  // reserved2
  JII::DestroyJavaVM,
  JII::AttachCurrentThread,
  JII::DetachCurrentThread,
  JII::GetEnv,
  JII::AttachCurrentThreadAsDaemon
};


JavaVMExt::JavaVMExt(Runtime* runtime,const RuntimeArgumentMap& runtime_options,std::string* error_msg)
    : runtime_(runtime),
      check_jni_abort_hook_(nullptr),
      check_jni_abort_hook_data_(nullptr),
      check_jni_(false),  // Initialized properly in the constructor body below.
      force_copy_(runtime_options.Exists(RuntimeArgumentMap::JniOptsForceCopy)),
      tracing_enabled_(runtime_options.Exists(RuntimeArgumentMap::JniTrace) || VLOG_IS_ON(third_party_jni)),
      trace_(runtime_options.GetOrDefault(RuntimeArgumentMap::JniTrace)),
      globals_(kGlobalsMax, kGlobal, IndirectReferenceTable::ResizableCapacity::kNo, error_msg),
      libraries_(new Libraries),
      unchecked_functions_(&gJniInvokeInterface),
      weak_globals_(kWeakGlobalsMax,kWeakGlobal,IndirectReferenceTable::ResizableCapacity::kNo,error_msg),
      allow_accessing_weak_globals_(true),
      weak_globals_add_condition_("weak globals add condition",(CHECK(Locks::jni_weak_globals_lock_ != nullptr),*Locks::jni_weak_globals_lock_)),
      env_hooks_() {
  functions = unchecked_functions_;
  SetCheckJniEnabled(runtime_options.Exists(RuntimeArgumentMap::CheckJni));//从传入的参数看,这里找不到这一项
}

bool JavaVMExt::SetCheckJniEnabled(bool enabled) {
  bool old_check_jni = check_jni_;
  check_jni_ = enabled;
  functions = enabled ? GetCheckJniInvokeInterface() : unchecked_functions_;			//GetCheckJniInvokeInterface	@art/runtime/check_jni.cc
  MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
  runtime_->GetThreadList()->ForEach(ThreadEnableCheckJni, &check_jni_);
  return old_check_jni;
}

const JNIInvokeInterface gCheckInvokeInterface = {
  nullptr,  // reserved0
  nullptr,  // reserved1
  nullptr,  // reserved2
  CheckJII::DestroyJavaVM,
  CheckJII::AttachCurrentThread,
  CheckJII::DetachCurrentThread,
  CheckJII::GetEnv,
  CheckJII::AttachCurrentThreadAsDaemon
};

const JNIInvokeInterface* GetCheckJniInvokeInterface() {
  return &gCheckInvokeInterface;
}