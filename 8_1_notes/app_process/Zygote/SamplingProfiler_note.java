/**
 * A sampling profiler. It currently is implemented without any
 * virtual machine support, relying solely on {@code
 * Thread.getStackTrace} to collect samples. As such, the overhead is
 * higher than a native approach and it does not provide insight into
 * where time is spent within native code, but it can still provide
 * useful insight into where a program is spending time.
 *
 * <h3>Usage Example</h3>
 *
 * The following example shows how to use the {@code
 * SamplingProfiler}. It samples the current thread's stack to a depth
 * of 12 stack frame elements over two different measurement periods
 * with samples taken every 100 milliseconds. In then prints the
 * results in hprof format to the standard output.
 *
 * <pre> {@code
 * ThreadSet threadSet = SamplingProfiler.newArrayThreadSet(Thread.currentThread());
 * SamplingProfiler profiler = new SamplingProfiler(12, threadSet);
 * profiler.start(100);
 * // period of measurement
 * profiler.stop();
 * // period of non-measurement
 * profiler.start(100);
 * // another period of measurement
 * profiler.stop();
 * profiler.shutdown();
 * AsciiHprofWriter.write(profiler.getHprofData(), System.out);
 * }</pre>
 */
public final class SamplingProfiler {

	
}