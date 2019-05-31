/***
 * SystemServer启动
 */

//  @/work/workcodes/aosp-p9.x-auto-alpha/frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
public class ZygoteInit {

    private static Runnable forkSystemServer(String abiList, String socketName,ZygoteServer zygoteServer) {
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
            OsConstants.CAP_WAKE_ALARM,
            OsConstants.CAP_BLOCK_SUSPEND
        );
        /* Containers run without some capabilities, so drop any caps that are not available. */
        StructCapUserHeader header = new StructCapUserHeader(OsConstants._LINUX_CAPABILITY_VERSION_3, 0);
        StructCapUserData[] data;
        try {
            data = Os.capget(header);
        } catch (ErrnoException ex) {
            throw new RuntimeException("Failed to capget()", ex);
        }
        capabilities &= ((long) data[0].effective) | (((long) data[1].effective) << 32);

        /* Hardcoded command line to start the system server */
        String args[] = {
            "--setuid=1000",
            "--setgid=1000",
            "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,1024,1032,1065,3001,3002,3003,3006,3007,3009,3010",
            "--capabilities=" + capabilities + "," + capabilities,
            "--nice-name=system_server",
            "--runtime-args",
            "--target-sdk-version=" + VMRuntime.SDK_VERSION_CUR_DEVELOPMENT,  // public static final int SDK_VERSION_CUR_DEVELOPMENT = 10000;
            "com.android.server.SystemServer",
        };
        ZygoteConnection.Arguments parsedArgs = null;

        int pid;

        try {
            parsedArgs = new ZygoteConnection.Arguments(args);
            ZygoteConnection.applyDebuggerSystemProperty(parsedArgs);
            ZygoteConnection.applyInvokeWithSystemProperty(parsedArgs);

            boolean profileSystemServer = SystemProperties.getBoolean("dalvik.vm.profilesystemserver", false); //false
            if (profileSystemServer) {
                parsedArgs.runtimeFlags |= Zygote.PROFILE_SYSTEM_SERVER;
            }

            /* Request to fork the system server process */
            pid = Zygote.forkSystemServer(
                    parsedArgs.uid, parsedArgs.gid,
                    parsedArgs.gids,
                    parsedArgs.runtimeFlags,
                    null,
                    parsedArgs.permittedCapabilities,
                    parsedArgs.effectiveCapabilities);


        } catch (IllegalArgumentException ex) {
            throw new RuntimeException(ex);
        }

        /* For child process */
        if (pid == 0) {
            if (hasSecondZygote(abiList)) {
                waitForSecondaryZygote(socketName);
            }

            zygoteServer.closeServerSocket();
            return handleSystemServerProcess(parsedArgs);
        }

        return null;
    }


     /**
     * Finish remaining work for the newly forked system server process.
     */
    private static Runnable handleSystemServerProcess(ZygoteConnection.Arguments parsedArgs) {
        // set umask to 0077 so new files and directories will default to owner-only permissions.
        Os.umask(S_IRWXG | S_IRWXO);

        if (parsedArgs.niceName != null) {
            Process.setArgV0(parsedArgs.niceName);
        }


        //  这里很重要
        final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");
        if (systemServerClasspath != null) {
            performSystemServerDexOpt(systemServerClasspath);  //分析
            // Capturing profiles is only supported for debug or eng builds since selinux normally prevents it.
            boolean profileSystemServer = SystemProperties.getBoolean("dalvik.vm.profilesystemserver", false); //false
            if (profileSystemServer && (Build.IS_USERDEBUG || Build.IS_ENG)) {
                try {
                    prepareSystemServerProfile(systemServerClasspath);
                } catch (Exception e) {
                    Log.wtf(TAG, "Failed to set up system server profile", e);
                }
            }
        }

        //这里 parsedArgs.invokeWith 为 null （--invoke-with 参数存在）
        if (parsedArgs.invokeWith != null) {
            。。。。。。
        } else {
            ClassLoader cl = null;
            if (systemServerClasspath != null) {
                cl = createPathClassLoader(systemServerClasspath, parsedArgs.targetSdkVersion);

                Thread.currentThread().setContextClassLoader(cl);
            }

            /*
             * Pass the remaining arguments to SystemServer.
             */
            return ZygoteInit.zygoteInit(parsedArgs.targetSdkVersion, parsedArgs.remainingArgs, cl);
        }

        /* should never reach here */
    }


    /**
     * Performs dex-opt on the elements of {@code classPath}, if needed. We
     * choose the instruction set of the current runtime.
     */
    private static void performSystemServerDexOpt(String classPath) {
        final String[] classPathElements = classPath.split(":");
        final IInstalld installd = IInstalld.Stub.asInterface(ServiceManager.getService("installd"));

        
        //  @/work/workcodes/aosp-p9.x-auto-alpha/libcore/libart/src/main/java/dalvik/system/VMRuntime.java
        /***获取指令集 */
        final String instructionSet = VMRuntime.getRuntime().vmInstructionSet(); //arm64

        String classPathForElement = "";
        for (String classPathElement : classPathElements) {
            // System server is fully AOTed and never profiled
            // for profile guided compilation.
            String systemServerFilter = SystemProperties.get("dalvik.vm.systemservercompilerfilter", "speed"); // speed

            int dexoptNeeded;
            try {
                dexoptNeeded = DexFile.getDexOptNeeded(
                    classPathElement, instructionSet, systemServerFilter,null /* classLoaderContext */, false /* newProfile */, false /* downgrade */);
            } catch (FileNotFoundException ignored) {
                // Do not add to the classpath.
                Log.w(TAG, "Missing classpath element for system server: " + classPathElement);
                continue;
            } catch (IOException e) {
                // Not fully clear what to do here as we don't know the cause of the
                // IO exception. Add to the classpath to be conservative, but don't
                // attempt to compile it.
                Log.w(TAG, "Error checking classpath element for system server: " + classPathElement, e);
                dexoptNeeded = DexFile.NO_DEXOPT_NEEDED;
            }

            if (dexoptNeeded != DexFile.NO_DEXOPT_NEEDED) {
                final String packageName = "*";
                final String outputPath = null;
                final int dexFlags = 0;
                final String compilerFilter = systemServerFilter;
                final String uuid = StorageManager.UUID_PRIVATE_INTERNAL;
                final String seInfo = null;
                final String classLoaderContext = getSystemServerClassLoaderContext(classPathForElement);
                final int targetSdkVersion = 0;  // SystemServer targets the system's SDK version
                try {
                    installd.dexopt(classPathElement, Process.SYSTEM_UID, packageName,
                            instructionSet, dexoptNeeded, outputPath, dexFlags, compilerFilter,
                            uuid, classLoaderContext, seInfo, false /* downgrade */,
                            targetSdkVersion, /*profileName*/ null, /*dexMetadataPath*/ null,
                            "server-dexopt");
                } catch (RemoteException | ServiceSpecificException e) {
                    // Ignore (but log), we need this on the classpath for fallback mode.
                    Log.w(TAG, "Failed compiling classpath element for system server: "  + classPathElement, e);
                }
            }

            classPathForElement = encodeSystemServerClassPath(classPathForElement, classPathElement);
        }
    }

}