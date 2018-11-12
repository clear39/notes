/*
虚拟机的三个接口函数
JNI_GetDefaultJavaVMInitArgs   这个函数art不支持
JNI_CreateJavaVM
JNI_GetCreatedJavaVMs


art虚拟机测试程序路径 art/dalvikvm/dalvikvm.cc
*/





//  libnativehelper/include/nativehelper/jni.h
/*
 * JNI 1.2+ initialization.  (As of 1.6, the pre-1.2 structures are no
 * longer supported.)
 */
typedef struct JavaVMOption {
    const char* optionString;
    void*       extraInfo;
} JavaVMOption;

typedef struct JavaVMInitArgs {
    jint        version;    /* use JNI_VERSION_1_2 or later */

    jint        nOptions;
    JavaVMOption* options;
    jboolean    ignoreUnrecognized;
} JavaVMInitArgs;





//  art/runtime/java_vm_ext.cc

bool JavaVMExt::IsBadJniVersion(int version) {
  // We don't support JNI_VERSION_1_1. These are the only other valid versions.
  return version != JNI_VERSION_1_2 && version != JNI_VERSION_1_4 && version != JNI_VERSION_1_6;
}


//  vm_args传入虚拟机参数
extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
  ScopedTrace trace(__FUNCTION__);
  const JavaVMInitArgs* args = static_cast<JavaVMInitArgs*>(vm_args);
  if (JavaVMExt::IsBadJniVersion(args->version)) { //判断虚拟机版本JNI_VERSION_1_4
    LOG(ERROR) << "Bad JNI version passed to CreateJavaVM: " << args->version;
    return JNI_EVERSION;
  }

  //  art/runtime/runtime.h:98:typedef std::vector<std::pair<std::string, const void*>> RuntimeOptions;
  RuntimeOptions options;
  for (int i = 0; i < args->nOptions; ++i) {
    JavaVMOption* option = &args->options[i];
    options.push_back(std::make_pair(std::string(option->optionString), option->extraInfo));
  }
  bool ignore_unrecognized = args->ignoreUnrecognized; //JNI_FALSE
  if (!Runtime::Create(options, ignore_unrecognized)) {//Runtime创建
    return JNI_ERR;
  }

  // Initialize native loader. This step makes sure we have
  // everything set up before we start using JNI.
  //  system/core/libnativeloader/native_loader.cpp 
  android::InitializeNativeLoader();//????????????

  Runtime* runtime = Runtime::Current();
  bool started = runtime->Start();                  //Runtime启动
  if (!started) {
    delete Thread::Current()->GetJniEnv();
    delete runtime->GetJavaVM();
    LOG(WARNING) << "CreateJavaVM failed";
    return JNI_ERR;
  }

  *p_env = Thread::Current()->GetJniEnv();
  *p_vm = runtime->GetJavaVM();
  return JNI_OK;
}

extern "C" jint JNI_GetCreatedJavaVMs(JavaVM** vms_buf, jsize buf_len, jsize* vm_count) {
  Runtime* runtime = Runtime::Current();
  if (runtime == nullptr || buf_len == 0) {
    *vm_count = 0;
  } else {
    *vm_count = 1;
    vms_buf[0] = runtime->GetJavaVM();
  }
  return JNI_OK;
}

// Historically unsupported.
extern "C" jint JNI_GetDefaultJavaVMInitArgs(void* /*vm_args*/) {
  return JNI_ERR;
}