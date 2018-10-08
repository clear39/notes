//	启动com.android.internal.os.ZygoteInit
//	frameworks/base/core/java/com/android/internal/os/ZygoteInit.java

/**
 * Startup class for the zygote process.
 *
 * Pre-initializes some classes, and then waits for commands on a UNIX domain
 * socket. Based on these commands, forks off child processes that inherit
 * the initial state of the VM.
 *
 * Please see {@link ZygoteConnection.Arguments} for documentation on the
 * client protocol.
 *
 * @hide
 */
public class ZygoteInit {

/**
# echo $BOOTCLASSPATH                                     
/system/framework/core-oj.jar:
/system/framework/core-libart.jar:
/system/framework/conscrypt.jar:
/system/framework/okhttp.jar:
/system/framework/legacy-test.jar:
/system/framework/bouncycastle.jar:
/system/framework/ext.jar:
/system/framework/framework.jar:
/system/framework/telephony-common.jar:
/system/framework/voip-common.jar:
/system/framework/ims-common.jar:
/system/framework/apache-xml.jar:
/system/framework/o
rg.apache.http.legacy.boot.jar:
/system/framework/android.hidl.base-V1.0-java.jar:
/system/framework/android.hidl.manager-V1.0-java.jar
*/

/*
:/system/framework/arm # ls
boot-android.hidl.base-V1.0-java.art     
boot-android.hidl.base-V1.0-java.oat     
boot-android.hidl.base-V1.0-java.vdex    
boot-android.hidl.manager-V1.0-java.art  
boot-android.hidl.manager-V1.0-java.oat  
boot-android.hidl.manager-V1.0-java.vdex 
boot-apache-xml.art                      
boot-apache-xml.oat                      
boot-apache-xml.vdex                     
boot-bouncycastle.art                    
boot-bouncycastle.oat                    
boot-bouncycastle.vdex                   
boot-conscrypt.art                       
boot-conscrypt.oat                       
boot-conscrypt.vdex                      
boot-core-libart.art                     
boot-core-libart.oat                     
boot-core-libart.vdex                    
boot-ext.art                             
boot-ext.oat                             
boot-ext.vdex                            
boot-framework.art                       
boot-framework.oat                       
boot-framework.vdex                      
boot-ims-common.art                      
boot-ims-common.oat                      
boot-ims-common.vdex                     
boot-legacy-test.art                     
boot-legacy-test.oat                     
boot-legacy-test.vdex                    
boot-okhttp.art                          
boot-okhttp.oat                          
boot-okhttp.vdex                         
boot-org.apache.http.legacy.boot.art     
boot-org.apache.http.legacy.boot.oat     
boot-org.apache.http.legacy.boot.vdex    
boot-telephony-common.art                
boot-telephony-common.oat                
boot-telephony-common.vdex               
boot-voip-common.art                     
boot-voip-common.oat                     
boot-voip-common.vdex                    
boot.art                                 
boot.oat                                 
boot.vdex      
*/

	//入口函数
    /**
    01-01 00:00:25.510   246   246 I Zygote  : arg[0]:com.android.internal.os.ZygoteInit
    01-01 00:00:25.511   246   246 I Zygote  : arg[1]:start-system-server
    01-01 00:00:25.511   246   246 I Zygote  : arg[2]:--abi-list=armeabi-v7a,armeabi
    */
	public static void main(String argv[]) {

        //构造函数空实现
        ZygoteServer zygoteServer = new ZygoteServer();

        // Mark zygote start. This ensures that thread creation will throw
        // an error.
        ZygoteHooks.startZygoteNoThreadCreation();

        // Zygote goes into its own process group.
        try {
            Os.setpgid(0, 0);
        } catch (ErrnoException ex) {
            throw new RuntimeException("Failed to setpgid(0,0)", ex);
        }

        try {
            // Report Zygote start time to tron unless it is a runtime restart
            if (!"1".equals(SystemProperties.get("sys.boot_completed"))) {
                MetricsLogger.histogram(null, "boot_zygote_init",(int) SystemClock.elapsedRealtime());
            }

            //bool is64BitMode = (sizeof(void*) == sizeof(uint64_t));
            String bootTimeTag = Process.is64Bit() ? "Zygote64Timing" : "Zygote32Timing";


            BootTimingsTraceLog bootTimingsTraceLog = new BootTimingsTraceLog(bootTimeTag,Trace.TRACE_TAG_DALVIK);
            bootTimingsTraceLog.traceBegin("ZygoteInit");
            RuntimeInit.enableDdms();   //  ?????????????????
            // Start profiling the zygote initialization.
            SamplingProfilerIntegration.start();//???????????????

            boolean startSystemServer = false;
            String socketName = "zygote";
            String abiList = null;
            boolean enableLazyPreload = false;
            for (int i = 1; i < argv.length; i++) {
                if ("start-system-server".equals(argv[i])) {
                    startSystemServer = true;
                } else if ("--enable-lazy-preload".equals(argv[i])) {//如果配置这个参数 则不进行preload动作
                    enableLazyPreload = true;
                } else if (argv[i].startsWith(ABI_LIST_ARG)) {//private static final String ABI_LIST_ARG = "--abi-list=";
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
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,SystemClock.uptimeMillis());
                bootTimingsTraceLog.traceEnd(); // ZygotePreload
            } else {
                Zygote.resetNicePriority();
            }

            // Finish profiling the zygote initialization.
            SamplingProfilerIntegration.writeZygoteSnapshot();

            // Do an initial gc to clean up after startup
            bootTimingsTraceLog.traceBegin("PostZygoteInitGC");
            gcAndFinalize();
            bootTimingsTraceLog.traceEnd(); // PostZygoteInitGC

            bootTimingsTraceLog.traceEnd(); // ZygoteInit
            // Disable tracing so that forked processes do not inherit(继承) stale(不新鲜的) tracing tags from
            // Zygote.
            Trace.setTracingEnabled(false, 0);//关闭trace功能

            // Zygote process unmounts root storage spaces.
            //这里涉及到Namespaces机制，后面再仔细分析？？？？？？？？？？
            Zygote.nativeUnmountStorageOnInit();  //zygote和fork的子进程共享root storage spaces.

            // Set seccomp policy
            Seccomp.setPolicy();//配合Selinux，系统接口直接调用限制

            ZygoteHooks.stopZygoteNoThreadCreation();

            if (startSystemServer) {
                startSystemServer(abiList, socketName, zygoteServer);
            }

            Log.i(TAG, "Accepting command socket connections");
            zygoteServer.runSelectLoop(abiList);//armeabi-v7a,armeabi

            zygoteServer.closeServerSocket();
        } catch (Zygote.MethodAndArgsCaller caller) {
            caller.run();
        } catch (Throwable ex) {
            Log.e(TAG, "System zygote died with exception", ex);
            zygoteServer.closeServerSocket();
            throw ex;
        }
    }


    /**
     * Runs several special GCs to try to clean up a few generations of
     * softly- and final-reachable objects, along with any other garbage.
     * This is only useful just before a fork().
     */
    /*package*/ static void gcAndFinalize() {
        final VMRuntime runtime = VMRuntime.getRuntime();

        /* runFinalizationSync() lets finalizers be called in Zygote,
         * which doesn't have a HeapWorker thread.
         */
        System.gc();
        runtime.runFinalizationSync(); // runFinalizationSync函数调用 System.runFinalization();
        System.gc();

        /*
         以上代码执行类似于以下效果
         Runtime.getRuntime().gc();
         Runtime.getRuntime().runFinalization();
         Runtime.getRuntime().gc();
        */

         //这里gc动作后面再做分析？？？？？？？？？？？？？？？？？？
    }


    



}