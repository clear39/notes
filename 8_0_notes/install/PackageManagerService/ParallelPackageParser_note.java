/**
 * Helper class for parallel parsing of packages using {@link PackageParser}.
 * <p>Parsing requests are processed by a thread-pool of {@link #MAX_THREADS}.
 * At any time, at most {@link #QUEUE_CAPACITY} results are kept in RAM</p>
 */
class ParallelPackageParser implements AutoCloseable {


	private final BlockingQueue<ParseResult> mQueue = new ArrayBlockingQueue<>(QUEUE_CAPACITY);
	private final ExecutorService mService = ConcurrentUtils.newFixedThreadPool(MAX_THREADS,"package-parsing-thread", Process.THREAD_PRIORITY_FOREGROUND);

	ParallelPackageParser(String[] separateProcesses, boolean onlyCoreApps,DisplayMetrics metrics, File cacheDir, PackageParser.Callback callback) {
        mSeparateProcesses = separateProcesses;
        mOnlyCore = onlyCoreApps;
        mMetrics = metrics;
        mCacheDir = cacheDir;
        mPackageParserCallback = callback;
    }

      /**
     * Submits the file for parsing
     * @param scanFile file to scan
     * @param parseFlags parse falgs
     */
    public void submit(File scanFile, int parseFlags) {
        mService.submit(() -> {
            ParseResult pr = new ParseResult();
            Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "parallel parsePackage [" + scanFile + "]");
            try {
                PackageParser pp = new PackageParser();
                pp.setSeparateProcesses(mSeparateProcesses);
                pp.setOnlyCoreApps(mOnlyCore);
                pp.setDisplayMetrics(mMetrics);
                pp.setCacheDir(mCacheDir);
                pp.setCallback(mPackageParserCallback);
                pr.scanFile = scanFile;
                pr.pkg = parsePackage(pp, scanFile, parseFlags);
            } catch (Throwable e) {
                pr.throwable = e;
            } finally {
                Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);
            }
            try {
                mQueue.put(pr);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                // Propagate result to callers of take().
                // This is helpful to prevent main thread from getting stuck waiting on
                // ParallelPackageParser to finish in case of interruption
                mInterruptedInThread = Thread.currentThread().getName();
            }
        });
    }

     @VisibleForTesting
    protected PackageParser.Package parsePackage(PackageParser packageParser, File scanFile, int parseFlags) throws PackageParser.PackageParserException {
        return packageParser.parsePackage(scanFile, parseFlags, true /* useCaches */);
    }

}