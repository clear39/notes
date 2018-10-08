

/**
 * A connection that can make spawn requests.
 */
class ZygoteConnection {


     public static void applyDebuggerSystemProperty(Arguments args) {
        if (RoSystemProperties.DEBUGGABLE) {// public static final boolean DEBUGGABLE = SystemProperties.getInt("ro.debuggable", 0) == 1;
            args.debugFlags |= Zygote.DEBUG_ENABLE_JDWP;
        }
    }

    
    
    
    public static void applyInvokeWithSystemProperty(Arguments args) {
        if (args.invokeWith == null && args.niceName != null) {
            String property = "wrap." + args.niceName; //wrap.SystemServer
            args.invokeWith = SystemProperties.get(property);
            if (args.invokeWith != null && args.invokeWith.length() == 0) {
                args.invokeWith = null;
            }
        }
    }



    public class Arguments{

        Arguments(String args[]) throws IllegalArgumentException {
            parseArgs(args);
        }

        /**
         * Parses the commandline arguments intended for the Zygote spawner
         * (such as "--setuid=" and "--setgid=") and creates an array
         * containing the remaining args.
         *
         * Per security review bug #1112214, duplicate args are disallowed in
         * critical cases to make injection harder.

          String args[] = {
            "--setuid=1000",
            "--setgid=1000",
            "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,1032,3001,3002,3003,3006,3007,3009,3010",
            "--capabilities=" + capabilities + "," + capabilities, //parsedArgs.permittedCapabilities
            "--nice-name=system_server",
            "--runtime-args",
            "com.android.server.SystemServer",
        };

         */
        private void parseArgs(String args[])throws IllegalArgumentException {
            int curArg = 0;

            boolean seenRuntimeArgs = false;

            for ( /* curArg */ ; curArg < args.length; curArg++) {
                String arg = args[curArg];

                if (arg.equals("--")) {
                    curArg++;
                    break;
                } else if (arg.startsWith("--setuid=")) {
                    if (uidSpecified) {
                        throw new IllegalArgumentException("Duplicate arg specified");
                    }
                    uidSpecified = true;
                    uid = Integer.parseInt(arg.substring(arg.indexOf('=') + 1)); //1000
                } else if (arg.startsWith("--setgid=")) {
                    if (gidSpecified) {
                        throw new IllegalArgumentException("Duplicate arg specified");
                    }
                    gidSpecified = true;
                    gid = Integer.parseInt(arg.substring(arg.indexOf('=') + 1));//1000
                } else if (arg.startsWith("--target-sdk-version=")) {
                    if (targetSdkVersionSpecified) {
                        throw new IllegalArgumentException( "Duplicate target-sdk-version specified");
                    }
                    targetSdkVersionSpecified = true;
                    targetSdkVersion = Integer.parseInt(arg.substring(arg.indexOf('=') + 1));
                } else if (arg.equals("--enable-jdwp")) {
                    debugFlags |= Zygote.DEBUG_ENABLE_JDWP;
                } else if (arg.equals("--enable-safemode")) {
                    debugFlags |= Zygote.DEBUG_ENABLE_SAFEMODE;
                } else if (arg.equals("--enable-checkjni")) {
                    debugFlags |= Zygote.DEBUG_ENABLE_CHECKJNI;
                } else if (arg.equals("--generate-debug-info")) {
                    debugFlags |= Zygote.DEBUG_GENERATE_DEBUG_INFO;
                } else if (arg.equals("--always-jit")) {
                    debugFlags |= Zygote.DEBUG_ALWAYS_JIT;
                } else if (arg.equals("--native-debuggable")) {
                    debugFlags |= Zygote.DEBUG_NATIVE_DEBUGGABLE;
                } else if (arg.equals("--java-debuggable")) {
                    debugFlags |= Zygote.DEBUG_JAVA_DEBUGGABLE;
                } else if (arg.equals("--enable-jni-logging")) {
                    debugFlags |= Zygote.DEBUG_ENABLE_JNI_LOGGING;
                } else if (arg.equals("--enable-assert")) {
                    debugFlags |= Zygote.DEBUG_ENABLE_ASSERT;
                } else if (arg.equals("--runtime-args")) {
                    seenRuntimeArgs = true;
                } else if (arg.startsWith("--seinfo=")) {
                    if (seInfoSpecified) {
                        throw new IllegalArgumentException( "Duplicate arg specified");
                    }
                    seInfoSpecified = true;
                    seInfo = arg.substring(arg.indexOf('=') + 1);
                } else if (arg.startsWith("--capabilities=")) {
                    if (capabilitiesSpecified) {
                        throw new IllegalArgumentException("Duplicate arg specified");
                    }
                    capabilitiesSpecified = true;
                    String capString = arg.substring(arg.indexOf('=')+1);

                    String[] capStrings = capString.split(",", 2);

                    if (capStrings.length == 1) {
                        effectiveCapabilities = Long.decode(capStrings[0]);
                        permittedCapabilities = effectiveCapabilities;
                    } else {
                        permittedCapabilities = Long.decode(capStrings[0]);  //capabilities
                        effectiveCapabilities = Long.decode(capStrings[1]); //capabilities
                    }
                } else if (arg.startsWith("--rlimit=")) {
                    // Duplicate --rlimit arguments are specifically allowed.
                    String[] limitStrings = arg.substring(arg.indexOf('=')+1).split(",");

                    if (limitStrings.length != 3) {
                        throw new IllegalArgumentException( "--rlimit= should have 3 comma-delimited ints");
                    }
                    int[] rlimitTuple = new int[limitStrings.length];

                    for(int i=0; i < limitStrings.length; i++) {
                        rlimitTuple[i] = Integer.parseInt(limitStrings[i]);
                    }

                    if (rlimits == null) {
                        rlimits = new ArrayList();
                    }

                    rlimits.add(rlimitTuple);
                } else if (arg.startsWith("--setgroups=")) {
                    if (gids != null) {
                        throw new IllegalArgumentException( "Duplicate arg specified");
                    }

                    String[] params  = arg.substring(arg.indexOf('=') + 1).split(",");

                    gids = new int[params.length];

                    for (int i = params.length - 1; i >= 0 ; i--) {
                        gids[i] = Integer.parseInt(params[i]);  //1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,1032,3001,3002,3003,3006,3007,3009,3010
                    }
                } else if (arg.equals("--invoke-with")) {
                    if (invokeWith != null) {
                        throw new IllegalArgumentException("Duplicate arg specified");
                    }
                    try {
                        invokeWith = args[++curArg];
                    } catch (IndexOutOfBoundsException ex) {
                        throw new IllegalArgumentException("--invoke-with requires argument");
                    }
                } else if (arg.startsWith("--nice-name=")) {
                    if (niceName != null) {
                        throw new IllegalArgumentException("Duplicate arg specified");
                    }
                    niceName = arg.substring(arg.indexOf('=') + 1);                         //      SystemServer
                } else if (arg.equals("--mount-external-default")) {
                    mountExternal = Zygote.MOUNT_EXTERNAL_DEFAULT;
                } else if (arg.equals("--mount-external-read")) {
                    mountExternal = Zygote.MOUNT_EXTERNAL_READ;
                } else if (arg.equals("--mount-external-write")) {
                    mountExternal = Zygote.MOUNT_EXTERNAL_WRITE;
                } else if (arg.equals("--query-abi-list")) {
                    abiListQuery = true;
                } else if (arg.startsWith("--instruction-set=")) {
                    instructionSet = arg.substring(arg.indexOf('=') + 1);
                } else if (arg.startsWith("--app-data-dir=")) {
                    appDataDir = arg.substring(arg.indexOf('=') + 1);
                } else if (arg.equals("--preload-package")) {
                    preloadPackage = args[++curArg];
                    preloadPackageLibs = args[++curArg];
                    preloadPackageCacheKey = args[++curArg];
                } else if (arg.equals("--preload-default")) {
                    preloadDefault = true;
                } else {
                    break;
                }
            }

            if (abiListQuery) {//false
                if (args.length - curArg > 0) {
                    throw new IllegalArgumentException("Unexpected arguments after --query-abi-list.");
                }
            } else if (preloadPackage != null) { // false
                if (args.length - curArg > 0) {
                    throw new IllegalArgumentException("Unexpected arguments after --preload-package.");
                }
            } else if (!preloadDefault) { //true
                if (!seenRuntimeArgs) {
                    throw new IllegalArgumentException("Unexpected argument : " + args[curArg]);
                }

                remainingArgs = new String[args.length - curArg];
                System.arraycopy(args, curArg, remainingArgs, 0, remainingArgs.length);
            }
        }
    }
}




