/**
 * Helper class for reporting boot timing metrics.
 * @hide
 */
public class BootTimingsTraceLog {

	 // Debug boot time for every step if it's non-user build.
    private static final boolean DEBUG_BOOT_TIME = !"user".equals(Build.TYPE);
    private final Deque<Pair<String, Long>> mStartTimes = DEBUG_BOOT_TIME ? new ArrayDeque<>() : null;
    private final String mTag;
    private long mTraceTag;

    public BootTimingsTraceLog(String tag, long traceTag) {
        mTag = tag;
        mTraceTag = traceTag;
    }

    public void traceBegin(String name) {
        Trace.traceBegin(mTraceTag, name);
        if (DEBUG_BOOT_TIME) {
            mStartTimes.push(Pair.create(name, SystemClock.elapsedRealtime()));
        }
    }

    public void traceEnd() {
        Trace.traceEnd(mTraceTag);
        if (!DEBUG_BOOT_TIME) {
            return;
        }
        if (mStartTimes.peek() == null) {
            Slog.w(mTag, "traceEnd called more times than traceBegin");
            return;
        }
        Pair<String, Long> event = mStartTimes.pop();
        // Log the duration so it can be parsed by external tools for performance reporting
        Slog.d(mTag, event.first + " took to complete: " + (SystemClock.elapsedRealtime() - event.second) + "ms");
    }

}