public class ActivityManagerService extends IActivityManager.Stub implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback {

	private final void startProcessLocked(ProcessRecord app, String hostingType,  String hostingNameStr, String abiOverride, String entryPoint, String[] entryPointArgs) {
        long startTime = SystemClock.elapsedRealtime();
        if (app.pid > 0 && app.pid != MY_PID) {
            checkTime(startTime, "startProcess: removing from pids map");
            synchronized (mPidsSelfLocked) {
                mPidsSelfLocked.remove(app.pid);
                mHandler.removeMessages(PROC_START_TIMEOUT_MSG, app);
            }
            checkTime(startTime, "startProcess: done removing from pids map");
            app.setPid(0);
        }

        if (DEBUG_PROCESSES && mProcessesOnHold.contains(app)) 
            Slog.v(TAG_PROCESSES, "startProcessLocked removing on hold: " + app);
        mProcessesOnHold.remove(app);

        checkTime(startTime, "startProcess: starting to update cpu stats");
        updateCpuStats();
        checkTime(startTime, "startProcess: done updating cpu stats");

        try {
            ......
            ProcessStartResult startResult;
            if (hostingType.equals("webview_service")) {
                startResult = startWebView(entryPoint,
                        app.processName, uid, uid, gids, debugFlags, mountExternal,
                        app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                        app.info.dataDir, null, entryPointArgs);
            } else {
                startResult = Process.start(entryPoint,
                        app.processName, uid, uid, gids, debugFlags, mountExternal,
                        app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                        app.info.dataDir, invokeWith, entryPointArgs);
            }
           ......
        } catch (RuntimeException e) {
            Slog.e(TAG, "Failure starting process " + app.processName, e);

            // Something went very wrong while trying to start this process; one
            // common case is when the package is frozen due to an active
            // upgrade. To recover, clean up any active bookkeeping related to
            // starting this process. (We already invoked this method once when
            // the package was initially frozen through KILL_APPLICATION_MSG, so
            // it doesn't hurt to use it again.)
            forceStopPackageLocked(app.info.packageName, UserHandle.getAppId(app.uid), false,
                    false, true, false, false, UserHandle.getUserId(app.userId), "start failure");
        }
    }
}












//  @frameworks/base/core/jni/com_android_internal_os_Zygote.cpp
static jint com_android_internal_os_Zygote_nativeForkAndSpecialize(JNIEnv* env, jclass, jint uid, jint gid, jintArray gids, jint debug_flags, jobjectArray rlimits,
        jint mount_external, jstring se_info, jstring se_name,jintArray fdsToClose,jintArray fdsToIgnore,jstring instructionSet, jstring appDataDir) {
    ......

    return ForkAndSpecializeCommon(env, uid, gid, gids, debug_flags,
            rlimits, capabilities, capabilities, mount_external, se_info,se_name, false, fdsToClose, fdsToIgnore, instructionSet, appDataDir);
}


static pid_t ForkAndSpecializeCommon(JNIEnv* env, uid_t uid, gid_t gid, jintArray javaGids,jint debug_flags, jobjectArray javaRlimits,jlong permittedCapabilities, jlong effectiveCapabilities,
                                     jint mount_external,jstring java_se_info, jstring java_se_name,bool is_system_server, jintArray fdsToClose,jintArray fdsToIgnore,string instructionSet, jstring dataDir) {
    ......

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
    //  @external/selinux/libselinux/src/android/android_platform.c:1054
    rc = selinux_android_setcontext(uid, is_system_server, se_info_c_str, se_name_c_str);
    if (rc == -1) {
      ALOGE("selinux_android_setcontext(%d, %d, \"%s\", \"%s\") failed", uid, is_system_server, se_info_c_str, se_name_c_str);
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

    env->CallStaticVoidMethod(gZygoteClass, gCallPostForkChildHooks, debug_flags,is_system_server, instructionSet);
    if (env->ExceptionCheck()) {
      RuntimeAbort(env, __LINE__, "Error calling post fork hooks.");
    }
  } else if (pid > 0) {
    ......
  }
  return pid;
}




int selinux_android_setcontext(uid_t uid,bool isSystemServer,const char *seinfo,const char *pkgname)
{
    char *orig_ctx_str = NULL, *ctx_str;
    context_t ctx = NULL;
    int rc = -1;

    if (is_selinux_enabled() <= 0)
        return 0;

    rc = getcon(&ctx_str);
    if (rc)
        goto err;

    ctx = context_new(ctx_str);
    orig_ctx_str = ctx_str;
    if (!ctx)
        goto oom;

    rc = seapp_context_lookup(SEAPP_DOMAIN, uid, isSystemServer, seinfo, pkgname, NULL, ctx);
    if (rc == -1)
        goto err;
    else if (rc == -2)
        goto oom;

    ctx_str = context_str(ctx);
    if (!ctx_str)
        goto oom;

    rc = security_check_context(ctx_str);
    if (rc < 0)
        goto err;

    if (strcmp(ctx_str, orig_ctx_str)) {
        rc = selinux_android_setcon(ctx_str);
        if (rc < 0)
            goto err;
    }

    rc = 0;
out:
    freecon(orig_ctx_str);
    context_free(ctx);
    avc_netlink_close();
    return rc;
err:
    if (isSystemServer)
        selinux_log(SELINUX_ERROR, "%s:  Error setting context for system server: %s\n", __FUNCTION__, strerror(errno));
    else
        selinux_log(SELINUX_ERROR, "%s:  Error setting context for app with uid %d, seinfo %s: %s\n",  __FUNCTION__, uid, seinfo, strerror(errno));

    rc = -1;
    goto out;
oom:
    selinux_log(SELINUX_ERROR, "%s:  Out of memory\n", __FUNCTION__);
    rc = -1;
    goto out;
}