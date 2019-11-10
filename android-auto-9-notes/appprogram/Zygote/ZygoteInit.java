public class ZygoteInit {

    public static void main(String argv[]) {
        /**
         * 简单构建 ZygoteServer
         */
        ZygoteServer zygoteServer = new ZygoteServer();

        // Mark zygote start. This ensures that thread creation will throw
        // an error.
        /**
         * 将  Runtime 中 zygote_no_threads_ 设置 为 true；
         * 不允许其他线程启动
         */
        ZygoteHooks.startZygoteNoThreadCreation();

        // Zygote goes into its own process group.
        try {
            Os.setpgid(0, 0);
        } catch (ErrnoException ex) {
            throw new RuntimeException("Failed to setpgid(0,0)", ex);
        }

        final Runnable caller;
        try {
            // Report Zygote start time to tron unless it is a runtime restart
            if (!"1".equals(SystemProperties.get("sys.boot_completed"))) {
                MetricsLogger.histogram(null, "boot_zygote_init",(int) SystemClock.elapsedRealtime());
            }

            /**
             * 通过 sizeof(void*) == sizeof(uint64_t) 判断是否为64位
             */
            String bootTimeTag = Process.is64Bit() ? "Zygote64Timing" : "Zygote32Timing";


            TimingsTraceLog bootTimingsTraceLog = new TimingsTraceLog(bootTimeTag,Trace.TRACE_TAG_DALVIK);
            bootTimingsTraceLog.traceBegin("ZygoteInit");


            RuntimeInit.enableDdms();

            boolean startSystemServer = false;
            String socketName = "zygote";
            String abiList = null;
            boolean enableLazyPreload = false;
            for (int i = 1; i < argv.length; i++) {
                if ("start-system-server".equals(argv[i])) {
                    startSystemServer = true;
                } else if ("--enable-lazy-preload".equals(argv[i])) {
                    enableLazyPreload = true;
                } else if (argv[i].startsWith(ABI_LIST_ARG)) { //虚拟机启动的时候 通过获取 ro.product.cpu.abilist64 进行参数传递
                    abiList = argv[i].substring(ABI_LIST_ARG.length());  // arm64-v8a 
                } else if (argv[i].startsWith(SOCKET_NAME_ARG)) {
                    socketName = argv[i].substring(SOCKET_NAME_ARG.length());
                } else {
                    throw new RuntimeException("Unknown command line argument: " + argv[i]);
                }
            }

            // 判断平台架构是否为空 
            if (abiList == null) {
                throw new RuntimeException("No ABI list supplied.");
            }

            /**
             * 从环境变量中获取 socket 描述符  
             */
            zygoteServer.registerServerSocketFromEnv(socketName);
            // In some configurations, we avoid preloading resources and classes eagerly.
            // In such cases, we will preload things prior to our first fork.
            if (!enableLazyPreload) { // enableLazyPreload 为true表示懒加载，不预先加载资源文件
                bootTimingsTraceLog.traceBegin("ZygotePreload");
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,SystemClock.uptimeMillis());
                /**
                 * 预加载
                 */
                preload(bootTimingsTraceLog);
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,SystemClock.uptimeMillis());
                bootTimingsTraceLog.traceEnd(); // ZygotePreload
            } else {
                Zygote.resetNicePriority();
            }

            // Do an initial gc to clean up after startup
            bootTimingsTraceLog.traceBegin("PostZygoteInitGC");
            gcAndFinalize();
            bootTimingsTraceLog.traceEnd(); // PostZygoteInitGC

            bootTimingsTraceLog.traceEnd(); // ZygoteInit
            // Disable tracing so that forked processes do not inherit stale tracing tags from
            // Zygote.
            Trace.setTracingEnabled(false, 0);

            /**
             * 
             */
            Zygote.nativeSecurityInit();

            // Zygote process unmounts root storage spaces.
            Zygote.nativeUnmountStorageOnInit();

             /**
             * 将  Runtime 中 zygote_no_threads_ 设置 为 false
             * 允许其他线程启动
             */
            ZygoteHooks.stopZygoteNoThreadCreation();


            /**
             * 启动 SystemServer
             */
            if (startSystemServer) {
                Runnable r = forkSystemServer(abiList, socketName, zygoteServer);

                // {@code r == null} in the parent (zygote) process, and {@code r != null} in the
                // child (system_server) process.
                if (r != null) {
                    r.run();
                    return;
                }
            }

            Log.i(TAG, "Accepting command socket connections");

            // The select loop returns early in the child process after a fork and
            // loops forever in the zygote.
            /***
             * 这里传递 abiList = “arm64-v8a” 方便应用程序请求获取 加载对应的架构 so 
             * 
             */
            caller = zygoteServer.runSelectLoop(abiList);

        } catch (Throwable ex) {
            Log.e(TAG, "System zygote died with exception", ex);
            throw ex;
        } finally {
            zygoteServer.closeServerSocket();
        }

        // We're in the child process and have exited the select loop. Proceed to execute the
        // command.
        if (caller != null) {
            caller.run();
        }
    }

}