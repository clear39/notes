/**
 * Main entry point for runtime initialization.  Not for
 * public consumption.
 * @hide
 */
public class RuntimeInit {


	    /**
     * Redirect System.out and System.err to the Android log.
     */
    public static void redirectLogStreams() {
        System.out.close();
        System.setOut(new AndroidPrintStream(Log.INFO, "System.out"));
        System.err.close();
        System.setErr(new AndroidPrintStream(Log.WARN, "System.err"));
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
        RuntimeInit.redirectLogStreams();//重定向标准和错误输出

        RuntimeInit.commonInit();
        ZygoteInit.nativeZygoteInit();
        RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader);
    }


     protected static final void commonInit() {
        if (DEBUG) Slog.d(TAG, "Entered RuntimeInit!");

        /*
         * set handlers; these apply to all threads in the VM. Apps can replace
         * the default handler, but not the pre handler.
         */
        Thread.setUncaughtExceptionPreHandler(new LoggingHandler());
        Thread.setDefaultUncaughtExceptionHandler(new KillApplicationHandler());

        /*
         * Install a TimezoneGetter subclass for ZoneInfo.db
         */
        TimezoneGetter.setInstance(new TimezoneGetter() {
            @Override
            public String getId() {
                return SystemProperties.get("persist.sys.timezone");
            }
        });
        TimeZone.setDefault(null);

        /*
         * Sets handler for java.util.logging to use Android log facilities.
         * The odd "new instance-and-then-throw-away" is a mirror of how
         * the "java.util.logging.config.class" system property works. We
         * can't use the system property here since the logger has almost
         * certainly already been initialized.
         */
        LogManager.getLogManager().reset();
        new AndroidConfig();

        /*
         * Sets the default HTTP User-Agent used by HttpURLConnection.
         */
        String userAgent = getDefaultUserAgent();
        System.setProperty("http.agent", userAgent);

        /*
         * Wire socket tagging to traffic stats.
         */
        NetworkManagementSocketTagger.install();

        /*
         * If we're running in an emulator launched with "-trace", put the
         * VM into emulator trace profiling mode so that the user can hit
         * F9/F10 at any time to capture traces.  This has performance
         * consequences, so it's not something you want to do always.
         */
        String trace = SystemProperties.get("ro.kernel.android.tracing");
        if (trace.equals("1")) {
            Slog.i(TAG, "NOTE: emulator trace profiling enabled");
            Debug.enableEmulatorTraceOutput();
        }

        initialized = true;
    }



    protected static void applicationInit(int targetSdkVersion, String[] argv, ClassLoader classLoader)throws Zygote.MethodAndArgsCaller {
        // If the application calls System.exit(), terminate the process
        // immediately without running any shutdown hooks.  It is not possible to
        // shutdown an Android application gracefully.  Among other things, the
        // Android runtime shutdown hooks close the Binder driver, which can cause
        // leftover running threads to crash before the process actually exits.
        nativeSetExitWithoutCleanup(true);

        // We want to be fairly aggressive about heap utilization, to avoid
        // holding on to a lot of memory that isn't needed.
        VMRuntime.getRuntime().setTargetHeapUtilization(0.75f);
        VMRuntime.getRuntime().setTargetSdkVersion(targetSdkVersion);

        final Arguments args;
        try {
            args = new Arguments(argv);
        } catch (IllegalArgumentException ex) {
            Slog.e(TAG, ex.getMessage());
            // let the process exit
            return;
        }

        // The end of of the RuntimeInit event (see #zygoteInit).
        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);

        // Remaining arguments are passed to the start class's static main
        invokeStaticMain(args.startClass, args.startArgs, classLoader);
    }








}