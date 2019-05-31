//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/base/core/java/com/android/internal/os/WebViewZygoteInit.java
class WebViewZygoteInit {

    /**
     * 
     * 这里是在WebViewZygote的connectToZygoteIfNeededLocked方法中调用
     *  05-24 17:45:11.780  2141  2141 I WebViewZygoteInit: Starting WebViewZygoteInit
        05-24 17:45:11.781  2141  2141 I WebViewZygoteInit: --zygote-socket=com.android.internal.os.WebViewZygoteInit/9f78cade-6645-40a9-b1f4-7f5ace27e0e6
        05-24 17:45:12.779  2141  2141 I WebViewZygoteInit: Beginning package preload
        05-24 17:45:12.925  2141  2141 I WebViewZygoteInit: Package preload done

     */
    public static void main(String argv[]) {
        Log.i(TAG, "Starting WebViewZygoteInit");

        String socketName = null;
        for (String arg : argv) {
            Log.i(TAG, arg);
            if (arg.startsWith(Zygote.CHILD_ZYGOTE_SOCKET_NAME_ARG)) {
                //  com.android.internal.os.WebViewZygoteInit/9f78cade-6645-40a9-b1f4-7f5ace27e0e6
                socketName = arg.substring(Zygote.CHILD_ZYGOTE_SOCKET_NAME_ARG.length());
            }
        }
        if (socketName == null) {
            throw new RuntimeException("No " + Zygote.CHILD_ZYGOTE_SOCKET_NAME_ARG + " specified");
        }

        try {
            Os.prctl(OsConstants.PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
        } catch (ErrnoException ex) {
            throw new RuntimeException("Failed to set PR_SET_NO_NEW_PRIVS", ex);
        }

        // 内部
        sServer = new WebViewZygoteServer();

        final Runnable caller;
        try {
            //传入 socketName 创建 LocalServerSocket 并且存在 mServerSocket中
            sServer.registerServerSocketAtAbstractName(socketName);

            // Add the abstract socket to the FD whitelist so that the native zygote code
            // can properly detach it after forking.
            //  @ /work/workcodes/aosp-p9.x-auto-alpha/frameworks/base/core/jni/com_android_internal_os_Zygote.cpp
            Zygote.nativeAllowFileAcrossFork("ABSTRACT/" + socketName);

            // The select loop returns early in the child process after a fork and
            // loops forever in the zygote.
            caller = sServer.runSelectLoop(TextUtils.join(",", Build.SUPPORTED_ABIS));
        } catch (RuntimeException e) {
            Log.e(TAG, "Fatal exception:", e);
            throw e;
        } finally {
            
            sServer.closeServerSocket();
        }

        // We're in the child process and have exited the select loop. Proceed to execute the
        // command.
        if (caller != null) {
            caller.run();
        }
    }

}


private static class WebViewZygoteServer extends ZygoteServer {
    @Override
    protected ZygoteConnection createNewConnection(LocalSocket socket, String abiList) throws IOException {
        return new WebViewZygoteConnection(socket, abiList);
    }
}

// path = "ABSTRACT/com.android.internal.os.WebViewZygoteInit/9f78cade-6645-40a9-b1f4-7f5ace27e0e6"
static void com_android_internal_os_Zygote_nativeAllowFileAcrossFork(JNIEnv* env, jclass, jstring path) {
    ScopedUtfChars path_native(env, path);
    const char* path_cstr = path_native.c_str();
    if (!path_cstr) {
        RuntimeAbort(env, __LINE__, "path_cstr == NULL");
    }
    FileDescriptorWhitelist::Get()->Allow(path_cstr); //    @   frameworks/base/core/jni/fd_utils.cpp
}