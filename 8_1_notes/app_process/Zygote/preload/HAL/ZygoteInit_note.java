public class ZygoteInit {
	 public static void main(String argv[]) {
    	
    	.......

        try {
           	......

            boolean startSystemServer = false;
            String socketName = "zygote";
            String abiList = null;
            boolean enableLazyPreload = false;
            for (int i = 1; i < argv.length; i++) {
                if ("start-system-server".equals(argv[i])) {
                    startSystemServer = true;
                } else if ("--enable-lazy-preload".equals(argv[i])) {
                    enableLazyPreload = true;
                } else if (argv[i].startsWith(ABI_LIST_ARG)) {
                    abiList = argv[i].substring(ABI_LIST_ARG.length());
                } else if (argv[i].startsWith(SOCKET_NAME_ARG)) {
                    socketName = argv[i].substring(SOCKET_NAME_ARG.length());
                } else {
                    throw new RuntimeException("Unknown command line argument: " + argv[i]);
                }
            }

            if (abiList == null) {
                throw new RuntimeException("No ABI list supplied.");
            }

            zygoteServer.registerServerSocket(socketName);
            // In some configurations, we avoid preloading resources and classes eagerly.
            // In such cases, we will preload things prior to our first fork.
            if (!enableLazyPreload) {
                bootTimingsTraceLog.traceBegin("ZygotePreload");
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,SystemClock.uptimeMillis());
                preload(bootTimingsTraceLog);
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END, SystemClock.uptimeMillis());
                bootTimingsTraceLog.traceEnd(); // ZygotePreload
            } else {
                Zygote.resetNicePriority();
            }

            ......
        } catch (Zygote.MethodAndArgsCaller caller) {
            caller.run();
        } catch (Throwable ex) {
            Log.e(TAG, "System zygote died with exception", ex);
            zygoteServer.closeServerSocket();
            throw ex;
        }
    }




    static void preload(BootTimingsTraceLog bootTimingsTraceLog) {
        Log.d(TAG, "begin preload");
        ......
        Trace.traceBegin(Trace.TRACE_TAG_DALVIK, "PreloadAppProcessHALs");
        nativePreloadAppProcessHALs();
        Trace.traceEnd(Trace.TRACE_TAG_DALVIK);
        .....
        Log.d(TAG, "end preload");

        sPreloadComplete = true;
    }


    native private static void nativePreloadAppProcessHALs();

    
    
}


//  @frameworks/base/core/jni/com_android_internal_os_ZygoteInit.cpp
void android_internal_os_ZygoteInit_nativePreloadAppProcessHALs(JNIEnv* env, jclass) {
    //
    android::GraphicBufferMapper::preloadHal();
    // Add preloading here for other HALs that are (a) always passthrough, and
    // (b) loaded by most app processes.
}

