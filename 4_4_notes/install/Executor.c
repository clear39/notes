private final ExecutorService mService = ConcurrentUtils.newFixedThreadPool(MAX_THREADS,"package-parsing-thread", Process.THREAD_PRIORITY_FOREGROUND);


/**
* Creates a thread pool using
* {@link java.util.concurrent.Executors#newFixedThreadPool(int, ThreadFactory)}
*
* @param nThreads the number of threads in the pool
* @param poolName base name of the threads in the pool
* @param linuxThreadPriority a Linux priority level. see {@link Process#setThreadPriority(int)}
* @return the newly created thread pool
*/
public static ExecutorService newFixedThreadPool(int nThreads, String poolName,int linuxThreadPriority) {
	return Executors.newFixedThreadPool(nThreads,new ThreadFactory() {
		    private final AtomicInteger threadNum = new AtomicInteger(0);

		    @Override
		    public Thread newThread(final Runnable r) {
		        return new Thread(poolName + threadNum.incrementAndGet()) {
		            @Override
		            public void run() {
		                Process.setThreadPriority(linuxThreadPriority);
		                r.run();
		            }
		        };
		    }
		});
}


/**
* Creates a thread pool that reuses a fixed number of threads
* operating off a shared unbounded queue, using the provided
* ThreadFactory to create new threads when needed.  At any point,
* at most {@code nThreads} threads will be active processing
* tasks.  If additional tasks are submitted when all threads are
* active, they will wait in the queue until a thread is
* available.  If any thread terminates due to a failure during
* execution prior to shutdown, a new one will take its place if
* needed to execute subsequent tasks.  The threads in the pool will
* exist until it is explicitly {@link ExecutorService#shutdown
* shutdown}.
*
* @param nThreads the number of threads in the pool
* @param threadFactory the factory to use when creating new threads
* @return the newly created thread pool
* @throws NullPointerException if threadFactory is null
* @throws IllegalArgumentException if {@code nThreads <= 0}
*/
public static ExecutorService newFixedThreadPool(int nThreads, ThreadFactory threadFactory) {
	return new ThreadPoolExecutor(nThreads, nThreads,
		                      0L, TimeUnit.MILLISECONDS,
		                      new LinkedBlockingQueue<Runnable>(),
		                      threadFactory);
}


/**
* Creates a new {@code ThreadPoolExecutor} with the given initial
* parameters and default rejected execution handler.
*
* @param corePoolSize the number of threads to keep in the pool, even
*        if they are idle, unless {@code allowCoreThreadTimeOut} is set
* @param maximumPoolSize the maximum number of threads to allow in the
*        pool
* @param keepAliveTime when the number of threads is greater than
*        the core, this is the maximum time that excess idle threads
*        will wait for new tasks before terminating.
* @param unit the time unit for the {@code keepAliveTime} argument
* @param workQueue the queue to use for holding tasks before they are
*        executed.  This queue will hold only the {@code Runnable}
*        tasks submitted by the {@code execute} method.
* @param threadFactory the factory to use when the executor
*        creates a new thread
* @throws IllegalArgumentException if one of the following holds:<br>
*         {@code corePoolSize < 0}<br>
*         {@code keepAliveTime < 0}<br>
*         {@code maximumPoolSize <= 0}<br>
*         {@code maximumPoolSize < corePoolSize}
* @throws NullPointerException if {@code workQueue}
*         or {@code threadFactory} is null
*/
public ThreadPoolExecutor(int corePoolSize,int maximumPoolSize,long keepAliveTime,TimeUnit unit,BlockingQueue<Runnable> workQueue,ThreadFactory threadFactory) {
	this(corePoolSize, maximumPoolSize, keepAliveTime, unit, workQueue,threadFactory, defaultHandler);
}



/**
* Creates a new {@code ThreadPoolExecutor} with the given initial
* parameters.
*
* @param corePoolSize the number of threads to keep in the pool, even
*        if they are idle, unless {@code allowCoreThreadTimeOut} is set
* @param maximumPoolSize the maximum number of threads to allow in the
*        pool
* @param keepAliveTime when the number of threads is greater than
*        the core, this is the maximum time that excess idle threads
*        will wait for new tasks before terminating.
* @param unit the time unit for the {@code keepAliveTime} argument
* @param workQueue the queue to use for holding tasks before they are
*        executed.  This queue will hold only the {@code Runnable}
*        tasks submitted by the {@code execute} method.
* @param threadFactory the factory to use when the executor
*        creates a new thread
* @param handler the handler to use when execution is blocked
*        because the thread bounds and queue capacities are reached
* @throws IllegalArgumentException if one of the following holds:<br>
*         {@code corePoolSize < 0}<br>
*         {@code keepAliveTime < 0}<br>
*         {@code maximumPoolSize <= 0}<br>
*         {@code maximumPoolSize < corePoolSize}
* @throws NullPointerException if {@code workQueue}
*         or {@code threadFactory} or {@code handler} is null
*/
public ThreadPoolExecutor(int corePoolSize,
                      int maximumPoolSize,
                      long keepAliveTime,
                      TimeUnit unit,
                      BlockingQueue<Runnable> workQueue,
                      ThreadFactory threadFactory,
                      RejectedExecutionHandler handler) {
	if (corePoolSize < 0 ||
	    maximumPoolSize <= 0 ||
	    maximumPoolSize < corePoolSize ||
	    keepAliveTime < 0)
	    throw new IllegalArgumentException();

	if (workQueue == null || threadFactory == null || handler == null)
	    throw new NullPointerException();

	this.corePoolSize = corePoolSize;
	this.maximumPoolSize = maximumPoolSize;
	this.workQueue = workQueue;
	this.keepAliveTime = unit.toNanos(keepAliveTime);
	this.threadFactory = threadFactory;
	this.handler = handler;
}

///////////////////////////////////////////////////////////////////////////////
//parallelPackageParser.submit(file, parseFlags);
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








