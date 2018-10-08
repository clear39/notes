在ZygoteInit的main函数中
public class ZygoteInit {
	public static void main(String argv[]) {

	 		。。。。。。//忽略的部分请参考Zygote启动

	        try {
	           。。。。。。//忽略的部分请参考Zygote启动

	            if (startSystemServer) {
	                startSystemServer(abiList, socketName, zygoteServer);
	            }

	            Log.i(TAG, "Accepting command socket connections");
	            zygoteServer.runSelectLoop(abiList);

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
	 * Prepare the arguments and fork for the system server process.
	 */
	private static boolean startSystemServer(String abiList=“armeabi-v7a,armeabi”, String socketName="zygote", ZygoteServer zygoteServer) throws Zygote.MethodAndArgsCaller, RuntimeException {
	    long capabilities = posixCapabilitiesAsBits(
	        OsConstants.CAP_IPC_LOCK,
	        OsConstants.CAP_KILL,
	        OsConstants.CAP_NET_ADMIN,
	        OsConstants.CAP_NET_BIND_SERVICE,
	        OsConstants.CAP_NET_BROADCAST,
	        OsConstants.CAP_NET_RAW,
	        OsConstants.CAP_SYS_MODULE,
	        OsConstants.CAP_SYS_NICE,
	        OsConstants.CAP_SYS_PTRACE,
	        OsConstants.CAP_SYS_TIME,
	        OsConstants.CAP_SYS_TTY_CONFIG,
	        OsConstants.CAP_WAKE_ALARM
	    );
	    /* Containers run without this capability, so avoid setting it in that case */
	    if (!SystemProperties.getBoolean(PROPERTY_RUNNING_IN_CONTAINER, false)) { //PROPERTY_RUNNING_IN_CONTAINER = "ro.boot.container"
	        capabilities |= posixCapabilitiesAsBits(OsConstants.CAP_BLOCK_SUSPEND);//执行
	    }
	    /* Hardcoded command line to start the system server */
	    String args[] = {
	        "--setuid=1000",
	        "--setgid=1000",
	        "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,1032,3001,3002,3003,3006,3007,3009,3010",
	        "--capabilities=" + capabilities + "," + capabilities, //parsedArgs.permittedCapabilities
	        "--nice-name=system_server",
	        "--runtime-args",
	        "com.android.server.SystemServer",
	    };
	    ZygoteConnection.Arguments parsedArgs = null;

	    int pid;

	    try {
	        parsedArgs = new ZygoteConnection.Arguments(args);//解析参数，并且存入对应变量中
	        ZygoteConnection.applyDebuggerSystemProperty(parsedArgs);
	        ZygoteConnection.applyInvokeWithSystemProperty(parsedArgs);

	        /* Request to fork the system server process */
	        //
	        pid = Zygote.forkSystemServer(parsedArgs.uid, parsedArgs.gid,parsedArgs.gids,parsedArgs.debugFlags,null,parsedArgs.permittedCapabilities,parsedArgs.effectiveCapabilities);

	    } catch (IllegalArgumentException ex) {
	        throw new RuntimeException(ex);
	    }

	    /* For child process */
	    if (pid == 0) {
	        if (hasSecondZygote(abiList)) {//	“armeabi-v7a,armeabi”
	            waitForSecondaryZygote(socketName);
	        }

	        zygoteServer.closeServerSocket();
	        handleSystemServerProcess(parsedArgs);
	    }
	    return true;
	}


	/**
     * Return {@code true} if this device configuration has another zygote.
     *
     * We determine this by comparing the device ABI list with this zygotes
     * list. If this zygote supports all ABIs this device supports, there won't
     * be another zygote.
     */
    private static boolean hasSecondZygote(String abiList) {
        return !SystemProperties.get("ro.product.cpu.abilist").equals(abiList);
    }

    private static void waitForSecondaryZygote(String socketName) {
        String otherZygoteName = Process.ZYGOTE_SOCKET.equals(socketName) ? Process.SECONDARY_ZYGOTE_SOCKET : Process.ZYGOTE_SOCKET;
        ZygoteProcess.waitForConnectionToZygote(otherZygoteName);
    }

      /**
     * Finish remaining work for the newly forked system server process.
     */
    private static void handleSystemServerProcess(ZygoteConnection.Arguments parsedArgs) throws Zygote.MethodAndArgsCaller {

        // set umask to 0077 so new files and directories will default to owner-only permissions.
        Os.umask(S_IRWXG | S_IRWXO);

        if (parsedArgs.niceName != null) {
            Process.setArgV0(parsedArgs.niceName);
        }

        final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");//这里添加打印
        if (systemServerClasspath != null) {
            performSystemServerDexOpt(systemServerClasspath);
            // Capturing profiles is only supported for debug or eng builds since selinux normally
            // prevents it.
            boolean profileSystemServer = SystemProperties.getBoolean("dalvik.vm.profilesystemserver", false);
            if (profileSystemServer && (Build.IS_USERDEBUG || Build.IS_ENG)) {
                try {
                    File profileDir = Environment.getDataProfilesDePackageDirectory(Process.SYSTEM_UID, "system_server");
                    File profile = new File(profileDir, "primary.prof");
                    profile.getParentFile().mkdirs();
                    profile.createNewFile();
                    String[] codePaths = systemServerClasspath.split(":");
                    VMRuntime.registerAppInfo(profile.getPath(), codePaths);
                } catch (Exception e) {
                    Log.wtf(TAG, "Failed to set up system server profile", e);
                }
            }
        }

        if (parsedArgs.invokeWith != null) {//不执行括号中代码
            String[] args = parsedArgs.remainingArgs;
            // If we have a non-null system server class path, we'll have to duplicate the
            // existing arguments and append the classpath to it. ART will handle the classpath
            // correctly when we exec a new process.
            if (systemServerClasspath != null) {
                String[] amendedArgs = new String[args.length + 2];
                amendedArgs[0] = "-cp";
                amendedArgs[1] = systemServerClasspath;
                System.arraycopy(args, 0, amendedArgs, 2, args.length);
                args = amendedArgs;
            }

            WrapperInit.execApplication(parsedArgs.invokeWith,parsedArgs.niceName, parsedArgs.targetSdkVersion, VMRuntime.getCurrentInstructionSet(), null, args);
        } else {
            ClassLoader cl = null;
            if (systemServerClasspath != null) {
                cl = createPathClassLoader(systemServerClasspath, parsedArgs.targetSdkVersion);
                Thread.currentThread().setContextClassLoader(cl);
            }

            /*
             * Pass the remaining arguments to SystemServer.
             */
            ZygoteInit.zygoteInit(parsedArgs.targetSdkVersion, parsedArgs.remainingArgs, cl);
        }


        /* should never reach here */
    }



        /**
     * The main function called when started through the zygote process. This
     * could be unified with main(), if the native code in nativeFinishInit()
     * were rationalized with Zygote startup.<p>
     *
     * Current recognized args:
     * <ul>
     *   <li> <code> [--] &lt;start class name&gt;  &lt;args&gt;
     * </ul>
     *
     * @param targetSdkVersion target SDK version
     * @param argv arg strings
     */
    public static final void zygoteInit(int targetSdkVersion, String[] argv,ClassLoader classLoader) throws Zygote.MethodAndArgsCaller {
        if (RuntimeInit.DEBUG) {
            Slog.d(RuntimeInit.TAG, "RuntimeInit: Starting application from zygote");
        }

        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "ZygoteInit");
        RuntimeInit.redirectLogStreams();

        RuntimeInit.commonInit();
        ZygoteInit.nativeZygoteInit();
        RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader);
    }


  








}
