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
        bootTimingsTraceLog.traceBegin("PreloadResources");
        preloadResources();
        bootTimingsTraceLog.traceEnd(); // PreloadResources
        ......
        Log.d(TAG, "end preload");

        sPreloadComplete = true;
    }


    /**
     * Load in commonly used resources, so they can be shared across
     * processes.
     *
     * These tend to be a few Kbytes, but are frequently in the 20-40K
     * range, and occasionally even larger.
     */
    private static void preloadResources() {
        final VMRuntime runtime = VMRuntime.getRuntime();

        try {
            mResources = Resources.getSystem();
            mResources.startPreloading();
            if (PRELOAD_RESOURCES) {//	public static final boolean PRELOAD_RESOURCES = true;
                Log.i(TAG, "Preloading resources...");

                long startTime = SystemClock.uptimeMillis();
                //  frameworks/base/core/res/res/values/arrays.xml:24:    <array name="preloaded_drawables">
                TypedArray ar = mResources.obtainTypedArray(com.android.internal.R.array.preloaded_drawables);
                int N = preloadDrawables(ar);
                ar.recycle();
                Log.i(TAG, "...preloaded " + N + " resources in " + (SystemClock.uptimeMillis()-startTime) + "ms.");

                startTime = SystemClock.uptimeMillis();
                //  frameworks/base/core/res/res/values/arrays.xml:145:    <array name="preloaded_color_state_lists">
                ar = mResources.obtainTypedArray(com.android.internal.R.array.preloaded_color_state_lists);
                N = preloadColorStateLists(ar);
                ar.recycle();
                Log.i(TAG, "...preloaded " + N + " resources in " + (SystemClock.uptimeMillis()-startTime) + "ms.");
                //  frameworks/base/core/res/res/values/config.xml:2598:    <bool name="config_freeformWindowManagement">false</bool>
                if (mResources.getBoolean(com.android.internal.R.bool.config_freeformWindowManagement)) {
                    startTime = SystemClock.uptimeMillis();
                    //  frameworks/base/core/res/res/values/arrays.xml:191:   <array name="preloaded_freeform_multi_window_drawables">
                    ar = mResources.obtainTypedArray(com.android.internal.R.array.preloaded_freeform_multi_window_drawables);
                    N = preloadDrawables(ar);
                    ar.recycle();
                    Log.i(TAG, "...preloaded " + N + " resource in "  + (SystemClock.uptimeMillis() - startTime) + "ms.");
                }
            }
            mResources.finishPreloading();
        } catch (RuntimeException e) {
            Log.w(TAG, "Failure preloading resources", e);
        }
    }


       /**
     * Return an array of heterogeneous values.
     *
     * @param id The desired resource identifier, as generated by the aapt
     *           tool. This integer encodes the package, type, and resource
     *           entry. The value 0 is an invalid identifier.
     *
     * @throws NotFoundException Throws NotFoundException if the given ID does not exist.
     *
     * @return Returns a TypedArray holding an array of the array values.
     * Be sure to call {@link TypedArray#recycle() TypedArray.recycle()}
     * when done with it.
     */
    @NonNull
    public TypedArray obtainTypedArray(@ArrayRes int id) throws NotFoundException {
        final ResourcesImpl impl = mResourcesImpl;
        int len = impl.getAssets().getArraySize(id);
        if (len < 0) {
            throw new NotFoundException("Array resource ID #0x" + Integer.toHexString(id));
        }
        
        TypedArray array = TypedArray.obtain(this, len);
        array.mLength = impl.getAssets().retrieveArray(id, array.mData);
        array.mIndices[0] = 0;
        
        return array;
    }


    private static int preloadDrawables(TypedArray ar) {
        int N = ar.length();
        for (int i=0; i<N; i++) {
            int id = ar.getResourceId(i, 0);
            if (false) {
                Log.v(TAG, "Preloading resource #" + Integer.toHexString(id));
            }
            if (id != 0) {
                if (mResources.getDrawable(id, null) == null) {
                    throw new IllegalArgumentException("Unable to find preloaded drawable resource #0x" + Integer.toHexString(id) + " (" + ar.getString(i) + ")");
                }
            }
        }
        return N;
    }
    
}