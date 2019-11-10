//  /work/workcodes/aosp-p9.x-auto-alpha/frameworks/base/core/java/android/webkit/WebViewZygote.java

public class WebViewZygote {

    public static ZygoteProcess getProcess() {
        synchronized (sLock) {
            if (sZygote != null) return sZygote;  //private static ChildZygoteProcess sZygote;

            //第一次调用时调用
            connectToZygoteIfNeededLocked();
            return sZygote;
        }
    }



    private static void connectToZygoteIfNeededLocked() {
        if (sZygote != null) {
            return;
        }

        if (sPackage == null) {
            Log.e(LOGTAG, "Cannot connect to zygote, no package specified");
            return;
        }

        try {
            sZygote = Process.zygoteProcess.startChildZygote(
                    "com.android.internal.os.WebViewZygoteInit",
                    "webview_zygote",
                    Process.WEBVIEW_ZYGOTE_UID,
                    Process.WEBVIEW_ZYGOTE_UID,
                    null,  // gids
                    0,  // runtimeFlags
                    "webview_zygote",  // seInfo
                    sPackage.applicationInfo.primaryCpuAbi,  // abi
                    null);  // instructionSet

            // All the work below is usually done by LoadedApk, but the zygote can't talk to
            // PackageManager or construct a LoadedApk since it's single-threaded pre-fork, so
            // doesn't have an ActivityThread and can't use Binder.
            // Instead, figure out the paths here, in the system server where we have access to
            // the package manager. Reuse the logic from LoadedApk to determine the correct
            // paths and pass them to the zygote as strings.
            final List<String> zipPaths = new ArrayList<>(10);
            final List<String> libPaths = new ArrayList<>(10);
            LoadedApk.makePaths(null, false, sPackage.applicationInfo, zipPaths, libPaths);
            final String librarySearchPath = TextUtils.join(File.pathSeparator, libPaths);
            final String zip = (zipPaths.size() == 1) ? zipPaths.get(0) :
                    TextUtils.join(File.pathSeparator, zipPaths);

            String libFileName = WebViewFactory.getWebViewLibrary(sPackage.applicationInfo);

            // In the case where the ApplicationInfo has been modified by the stub WebView,
            // we need to use the original ApplicationInfo to determine what the original classpath
            // would have been to use as a cache key.
            LoadedApk.makePaths(null, false, sPackageOriginalAppInfo, zipPaths, null);
            final String cacheKey = (zipPaths.size() == 1) ? zipPaths.get(0) :
                    TextUtils.join(File.pathSeparator, zipPaths);

            ZygoteProcess.waitForConnectionToZygote(sZygote.getPrimarySocketAddress());

            Log.d(LOGTAG, "Preloading package " + zip + " " + librarySearchPath);
            sZygote.preloadPackageForAbi(zip, librarySearchPath, libFileName, cacheKey,
                                         Build.SUPPORTED_ABIS[0]);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error connecting to webview zygote", e);
            stopZygoteLocked();
        }
    }

}