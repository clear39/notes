

public class ActivityManagerService extends IActivityManager.Stub implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback {

    @Override
    public void registerUidObserver(IUidObserver observer, int which, int cutpoint,String callingPackage) {
        if (!hasUsageStatsPermission(callingPackage)) {
            enforceCallingPermission(android.Manifest.permission.PACKAGE_USAGE_STATS,"registerUidObserver");
        }
        synchronized (this) {
            /**
             * final RemoteCallbackList<IUidObserver> mUidObservers = new RemoteCallbackList<>();
             */
            mUidObservers.register(observer, new UidObserverRegistration(Binder.getCallingUid(),callingPackage, which, cutpoint));
        }
    }

    @Override
    public void unregisterUidObserver(IUidObserver observer) {
        synchronized (this) {
            mUidObservers.unregister(observer);
        }
    }


}