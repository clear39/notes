/**
 * Calls Object.finalize() on objects in the finalizer reference queue. The VM
 * will abort if any finalize() call takes more than the maximum finalize time
 * to complete.
 *
 * @hide
 */
public final class Daemons {


	public static void stop() {
        HeapTaskDaemon.INSTANCE.stop();
        ReferenceQueueDaemon.INSTANCE.stop();
        FinalizerDaemon.INSTANCE.stop();
        FinalizerWatchdogDaemon.INSTANCE.stop();
    }


    public static void startPostZygoteFork() {
        ReferenceQueueDaemon.INSTANCE.startPostZygoteFork();
        FinalizerDaemon.INSTANCE.startPostZygoteFork();
        FinalizerWatchdogDaemon.INSTANCE.startPostZygoteFork();
        HeapTaskDaemon.INSTANCE.startPostZygoteFork();
    }


}