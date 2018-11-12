/** @hide */

//本地代码jni接口 frameworks/base/core/jni/com_android_internal_os_Zygote.cpp

public final class Zygote {


    public static int forkSystemServer(int uid, int gid, int[] gids, int debugFlags,int[][] rlimits, long permittedCapabilities, long effectiveCapabilities) {
        VM_HOOKS.preFork();
        // Resets nice priority for zygote process.
        resetNicePriority();//设置线程等级
        int pid = nativeForkSystemServer(uid, gid, gids, debugFlags, rlimits, permittedCapabilities, effectiveCapabilities);
        // Enable tracing as soon as we enter the system_server.
        if (pid == 0) {
            Trace.setTracingEnabled(true, debugFlags);
        }
        VM_HOOKS.postForkCommon();
        return pid;
    }
    native private static int nativeForkSystemServer(int uid, int gid, int[] gids, int debugFlags,int[][] rlimits, long permittedCapabilities, long effectiveCapabilities);

    /**
     * Resets the calling thread priority to the default value (Thread.NORM_PRIORITY
     * or nice value 0). This updates both the priority value in java.lang.Thread and
     * the nice value (setpriority).
     */
    static void resetNicePriority() {
        Thread.currentThread().setPriority(Thread.NORM_PRIORITY);
    }

}

static jint com_android_internal_os_Zygote_nativeForkSystemServer(JNIEnv* env, jclass, uid_t uid, gid_t gid, jintArray gids,jint debug_flags, jobjectArray rlimits, jlong permittedCapabilities,jlong effectiveCapabilities) {
  pid_t pid = ForkAndSpecializeCommon(env, uid, gid, gids,debug_flags, rlimits,permittedCapabilities, effectiveCapabilities,MOUNT_EXTERNAL_DEFAULT, NULL, NULL, true, NULL,NULL, NULL, NULL);
  if (pid > 0) {
      // The zygote process checks whether the child process has died or not.
      ALOGI("System server process %d has been created", pid);
      gSystemServerPid = pid;
      // There is a slight window that the system server process has crashed
      // but it went unnoticed because we haven't published its pid yet. So
      // we recheck here just to make sure that all is well.
      int status;
      if (waitpid(pid, &status, WNOHANG) == pid) {// WNOHANG位，与那么该系统调用就是非阻塞的，也就是说会立刻返回而不是等待子进程的状态发生变化
          ALOGE("System server process %d has died. Restarting Zygote!", pid);
          RuntimeAbort(env, __LINE__, "System server process has died. Restarting Zygote!");
      }
  }
  return pid;
}


// Utility routine to fork zygote and specialize the child process.
static pid_t ForkAndSpecializeCommon(JNIEnv* env, uid_t uid, gid_t gid, jintArray javaGids,jint debug_flags, jobjectArray javaRlimits,jlong permittedCapabilities, jlong effectiveCapabilities,jint mount_external,jstring java_se_info, jstring java_se_name,bool is_system_server, jintArray fdsToClose,jintArray fdsToIgnore,jstring instructionSet, jstring dataDir) {
  SetSigChldHandler();

  sigset_t sigchld;
  sigemptyset(&sigchld);
  sigaddset(&sigchld, SIGCHLD);

  // Temporarily block SIGCHLD during forks. The SIGCHLD handler might
  // log, which would result in the logging FDs we close being reopened.
  // This would cause failures because the FDs are not whitelisted.
  //
  // Note that the zygote process is single threaded at this point.
  if (sigprocmask(SIG_BLOCK, &sigchld, nullptr) == -1) {
    ALOGE("sigprocmask(SIG_SETMASK, { SIGCHLD }) failed: %s", strerror(errno));
    RuntimeAbort(env, __LINE__, "Call to sigprocmask(SIG_BLOCK, { SIGCHLD }) failed.");
  }

  // Close any logging related FDs before we start evaluating the list of
  // file descriptors.
  __android_log_close();

  // If this is the first fork for this zygote, create the open FD table.
  // If it isn't, we just need to check whether the list of open files has
  // changed (and it shouldn't in the normal case).
  std::vector<int> fds_to_ignore;
  FillFileDescriptorVector(env, fdsToIgnore, &fds_to_ignore);
  if (gOpenFdTable == NULL) {
    gOpenFdTable = FileDescriptorTable::Create(fds_to_ignore);
    if (gOpenFdTable == NULL) {
      RuntimeAbort(env, __LINE__, "Unable to construct file descriptor table.");
    }
  } else if (!gOpenFdTable->Restat(fds_to_ignore)) {
    RuntimeAbort(env, __LINE__, "Unable to restat file descriptor table.");
  }

  pid_t pid = fork();

  if (pid == 0) {
    // The child process.
    gMallocLeakZygoteChild = 1;

    // Set the jemalloc decay time to 1.
    mallopt(M_DECAY_TIME, 1);

    // Clean up any descriptors which must be closed immediately
    DetachDescriptors(env, fdsToClose);

    // Re-open all remaining open file descriptors so that they aren't shared
    // with the zygote across a fork.
    if (!gOpenFdTable->ReopenOrDetach()) {
      RuntimeAbort(env, __LINE__, "Unable to reopen whitelisted descriptors.");
    }

    if (sigprocmask(SIG_UNBLOCK, &sigchld, nullptr) == -1) {
      ALOGE("sigprocmask(SIG_SETMASK, { SIGCHLD }) failed: %s", strerror(errno));
      RuntimeAbort(env, __LINE__, "Call to sigprocmask(SIG_UNBLOCK, { SIGCHLD }) failed.");
    }

    // Keep capabilities across UID change, unless we're staying root.
    if (uid != 0) {
      EnableKeepCapabilities(env);
    }

    SetInheritable(env, permittedCapabilities);
    DropCapabilitiesBoundingSet(env);

    bool use_native_bridge = !is_system_server && (instructionSet != NULL) && android::NativeBridgeAvailable();  // use_native_bridge == false
    if (use_native_bridge) {
        。。。。。。
    }

    if (use_native_bridge && dataDir == NULL) {
      。。。。。。
    }

    if (!MountEmulatedStorage(uid, mount_external, use_native_bridge)) {
      ALOGW("Failed to mount emulated storage: %s", strerror(errno));
      if (errno == ENOTCONN || errno == EROFS) {
        // When device is actively encrypting, we get ENOTCONN here
        // since FUSE was mounted before the framework restarted.
        // When encrypted device is booting, we get EROFS since
        // FUSE hasn't been created yet by init.
        // In either case, continue without external storage.
      } else {
        RuntimeAbort(env, __LINE__, "Cannot continue without emulated storage");
      }
    }

    if (!is_system_server) {//false
      。。。。。。
    }

    SetGids(env, javaGids);

    SetRLimits(env, javaRlimits);

    if (use_native_bridge) {
    。。。。。。
    }

    int rc = setresgid(gid, gid, gid);
    if (rc == -1) {
      ALOGE("setresgid(%d) failed: %s", gid, strerror(errno));
      RuntimeAbort(env, __LINE__, "setresgid failed");
    }

    rc = setresuid(uid, uid, uid);
    if (rc == -1) {
      ALOGE("setresuid(%d) failed: %s", uid, strerror(errno));
      RuntimeAbort(env, __LINE__, "setresuid failed");
    }

    if (NeedsNoRandomizeWorkaround()) {
        // Work around ARM kernel ASLR lossage (http://b/5817320).
        int old_personality = personality(0xffffffff);
        int new_personality = personality(old_personality | ADDR_NO_RANDOMIZE);
        if (new_personality == -1) {
            ALOGW("personality(%d) failed: %s", new_personality, strerror(errno));
        }
    }

    SetCapabilities(env, permittedCapabilities, effectiveCapabilities, permittedCapabilities);

    SetSchedulerPolicy(env);

    const char* se_info_c_str = NULL;
    ScopedUtfChars* se_info = NULL;
    if (java_se_info != NULL) {
        se_info = new ScopedUtfChars(env, java_se_info);
        se_info_c_str = se_info->c_str();
        if (se_info_c_str == NULL) {
          RuntimeAbort(env, __LINE__, "se_info_c_str == NULL");
        }
    }
    const char* se_name_c_str = NULL;
    ScopedUtfChars* se_name = NULL;
    if (java_se_name != NULL) {
        se_name = new ScopedUtfChars(env, java_se_name);
        se_name_c_str = se_name->c_str();
        if (se_name_c_str == NULL) {
          RuntimeAbort(env, __LINE__, "se_name_c_str == NULL");
        }
    }
    rc = selinux_android_setcontext(uid, is_system_server, se_info_c_str, se_name_c_str);     //     
    if (rc == -1) {
      ALOGE("selinux_android_setcontext(%d, %d, \"%s\", \"%s\") failed", uid,is_system_server, se_info_c_str, se_name_c_str);
      RuntimeAbort(env, __LINE__, "selinux_android_setcontext failed");
    }

    // Make it easier to debug audit logs by setting the main thread's name to the
    // nice name rather than "app_process".
    if (se_info_c_str == NULL && is_system_server) {
      se_name_c_str = "system_server";
    }
    if (se_info_c_str != NULL) {
      SetThreadName(se_name_c_str);
    }

    delete se_info;
    delete se_name;

    UnsetSigChldHandler();

    //gZygoteClass   == com/android/internal/os/Zygote
    //      gCallPostForkChildHooks                  == callPostForkChildHooks
    env->CallStaticVoidMethod(gZygoteClass, gCallPostForkChildHooks, debug_flags,is_system_server == true, instructionSet);
    if (env->ExceptionCheck()) {
      RuntimeAbort(env, __LINE__, "Error calling post fork hooks.");
    }


  } else if (pid > 0) {
    // the parent process

    // We blocked SIGCHLD prior to a fork, we unblock it here.
    if (sigprocmask(SIG_UNBLOCK, &sigchld, nullptr) == -1) {
      ALOGE("sigprocmask(SIG_SETMASK, { SIGCHLD }) failed: %s", strerror(errno));
      RuntimeAbort(env, __LINE__, "Call to sigprocmask(SIG_UNBLOCK, { SIGCHLD }) failed.");
    }


  }
  return pid;
}